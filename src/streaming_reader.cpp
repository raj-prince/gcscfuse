#include "streaming_reader.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace gcscfuse {

StreamingReader::StreamingReader(
    std::unique_ptr<IReader> base_reader,
    size_t max_stream_size
) : base_reader_(std::move(base_reader)),
    max_stream_size_(max_stream_size) {
}

StreamingReader::~StreamingReader() {
    closeStream();
}

int StreamingReader::read(const std::string& object_name, char* buf, size_t size, off_t offset) {
    auto total_start = std::chrono::high_resolution_clock::now();
    stats_.total_reads++;
    
    // Check if we can serve from existing stream
    if (canServeFromStream(object_name, offset, size)) {
        stats_.stream_reuses++;
        stats_.bytes_from_stream += size;
        
        auto read_start = std::chrono::high_resolution_clock::now();
        int result = readFromStream(buf, size);
        auto read_end = std::chrono::high_resolution_clock::now();
        
        if (result < 0) return result;
        
        stream_state_.stream_current_pos += result;
        stream_state_.last_read_end = offset + result;
        
        auto total_end = std::chrono::high_resolution_clock::now();
        auto copy_us = std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start).count();
        auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();
        
        if (stream_state_.current_stream_size > 1024 * 1024) {  // Log only for streams > 1MB
            std::cout << "[STREAM] Reused stream for " << object_name 
                      << " [" << offset << ", " << (offset + result) << ")"
                      << " from stream [" << stream_state_.stream_start_offset 
                      << ", " << stream_state_.stream_end_offset << ")"
                      << " copy_time=" << copy_us << "us"
                      << " total_time=" << total_us << "us" << std::endl;
        }
        
        return result;
    }
    
    // Need new stream
    // Save info for next stream calculation BEFORE closing
    std::string last_file = stream_state_.file_path;
    off_t last_end = stream_state_.last_read_end;
    size_t last_size = stream_state_.current_stream_size;
    
    closeStream();
    
    // Calculate size for new stream (with doubling logic)
    size_t new_stream_size = calculateNextStreamSize(object_name, offset, size, last_file, last_end, last_size);
    
    // OPTIMIZATION: Read directly into output buffer first to avoid one memcpy
    auto stream_start = std::chrono::high_resolution_clock::now();
    int result = base_reader_->read(object_name, buf, size, offset);
    auto direct_read_end = std::chrono::high_resolution_clock::now();
    
    if (result < 0) return result;
    
    // Always set file_path for sequential tracking
    stream_state_.file_path = object_name;
    
    // Now read the prefetch portion into our buffer (if stream size > request size)
    size_t prefetch_size = (new_stream_size > size) ? (new_stream_size - size) : 0;
    if (prefetch_size > 0) {
        stream_state_.buffer.resize(prefetch_size);
        int prefetch_read = base_reader_->read(object_name, stream_state_.buffer.data(), prefetch_size, offset + result);
        
        if (prefetch_read > 0) {
            stream_state_.buffer.resize(prefetch_read);
            // Setup stream state for future reads
            stream_state_.stream_start_offset = offset + result;  // Starts after initial read
            stream_state_.stream_end_offset = offset + result + prefetch_read;
            stream_state_.stream_current_pos = offset + result;
            stream_state_.current_stream_size = new_stream_size;
            stream_state_.buffer_pos = 0;
        } else {
            // Prefetch failed, but we still served the initial request
            stream_state_.buffer.clear();
            stream_state_.current_stream_size = size;  // Only got the initial request
        }
    } else {
        // No prefetch needed
        stream_state_.buffer.clear();
        stream_state_.current_stream_size = size;
    }
    
    auto stream_end = std::chrono::high_resolution_clock::now();
    stats_.streams_opened++;
    stats_.bytes_from_stream += result;
    stream_state_.last_read_end = offset + result;
    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto gcs_us = std::chrono::duration_cast<std::chrono::microseconds>(stream_end - stream_start).count();
    auto direct_us = std::chrono::duration_cast<std::chrono::microseconds>(direct_read_end - stream_start).count();
    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();
    
    size_t actual_stream = result + (stream_state_.buffer.size());
    std::cout << "[STREAM] Opened new stream for " << object_name 
              << " [" << offset << ", " << (offset + result) << ")"
              << " stream_size=" << (new_stream_size / 1024 / 1024) << "MB"
              << " actual=" << (actual_stream / 1024 / 1024) << "MB"
              << " gcs_time=" << gcs_us << "us"
              << " (direct_read=" << direct_us << "us)"
              << " total_time=" << total_us << "us"
              << " zero_copy=true" << std::endl;
    
    return result;
}

bool StreamingReader::canServeFromStream(const std::string& object_name, off_t offset, size_t size) const {
    if (!stream_state_.isActive()) return false;
    if (object_name != stream_state_.file_path) return false;
    
    // Must be sequential from current position
    if (offset != stream_state_.stream_current_pos) return false;
    
    // Must have enough data in stream
    return (offset + size <= stream_state_.stream_end_offset);
}

int StreamingReader::readFromStream(char* buf, size_t size) {
    if (stream_state_.buffer_pos + size > stream_state_.buffer.size()) {
        // Not enough data in buffer
        size_t available = stream_state_.buffer.size() - stream_state_.buffer_pos;
        if (available == 0) return 0;
        size = available;  // Read what's available
    }
    
    std::memcpy(buf, stream_state_.buffer.data() + stream_state_.buffer_pos, size);
    stream_state_.buffer_pos += size;
    
    return static_cast<int>(size);
}

void StreamingReader::closeStream() {
    stream_state_ = StreamState{};
    // Explicitly free buffer memory
    stream_state_.buffer = std::vector<char>();
}

size_t StreamingReader::calculateNextStreamSize(
    const std::string& object_name, 
    off_t offset, 
    size_t request_size,
    const std::string& last_file_path,
    off_t last_read_end,
    size_t last_stream_size
) {
    // Check if this is sequential from last read
    bool is_sequential = (last_read_end >= 0) && 
                        (offset == last_read_end) &&
                        (object_name == last_file_path);
    
    size_t new_stream_size;
    
    if (!is_sequential || last_stream_size == 0) {
        // First stream or non-sequential: start with request size
        new_stream_size = request_size;
        
        if (last_stream_size > 0) {
            std::cout << "[STREAM] Non-sequential access detected, resetting stream size to " 
                      << (request_size / 1024 / 1024) << "MB" << std::endl;
        }
    } else {
        // Sequential: DOUBLE the previous stream size
        new_stream_size = last_stream_size * 2;
        
        // Cap at max
        new_stream_size = std::min(new_stream_size, max_stream_size_);
        
        // Ensure at least covers current request
        new_stream_size = std::max(new_stream_size, request_size);
        
        std::cout << "[STREAM] Sequential access detected, doubling from " 
                  << (last_stream_size / 1024 / 1024) << "MB to "
                  << (new_stream_size / 1024 / 1024) << "MB" << std::endl;
    }
    
    return new_stream_size;
}

} // namespace gcscfuse
