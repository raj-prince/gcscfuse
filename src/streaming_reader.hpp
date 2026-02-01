#pragma once

#include "reader.hpp"
#include "gcs/gcs_client.hpp"
#include <memory>
#include <string>

namespace gcscfuse {

/**
 * StreamingReader - Optimizes sequential reads by reusing GCS streams
 * 
 * Features:
 * - Detects sequential read patterns
 * - Doubles stream size on each new sequential stream
 * - Reuses existing streams for multiple reads
 * - Resets on non-sequential access
 * 
 * Strategy:
 * - First read: Open stream for request size
 * - Sequential reads: Double stream size when opening new stream
 * - Serves multiple reads from same stream (no doubling)
 * - Non-sequential: Reset to initial size
 */
class StreamingReader : public IReader {
public:
    explicit StreamingReader(
        std::unique_ptr<IReader> base_reader,
        size_t max_stream_size = 128 * 1024 * 1024  // 128MB default
    );
    
    ~StreamingReader() override;
    
    int read(const std::string& object_name, char* buf, size_t size, off_t offset) override;
    
    // Statistics for monitoring
    struct Stats {
        uint64_t total_reads = 0;
        uint64_t stream_reuses = 0;
        uint64_t streams_opened = 0;
        uint64_t bytes_from_stream = 0;
    };
    
    Stats getStats() const { return stats_; }
    void resetStats() { stats_ = Stats{}; }

private:
    struct StreamState {
        std::string file_path;
        off_t stream_start_offset = 0;
        off_t stream_end_offset = 0;      // Exclusive end
        off_t stream_current_pos = 0;      // Current read position in stream
        size_t current_stream_size = 0;    // Size of current open stream
        
        // Tracking for next stream
        off_t last_read_end = -1;          // Where last read ended
        
        // Active stream data (buffered from base reader)
        std::vector<char> buffer;
        size_t buffer_pos = 0;
        
        bool isActive() const { return !buffer.empty(); }
    };
    
    // Check if can serve from existing stream
    bool canServeFromStream(const std::string& path, off_t offset, size_t size) const;
    
    // Read from existing stream buffer
    int readFromStream(char* buf, size_t size);
    
    // Close current stream
    void closeStream();
    
    // Calculate size for next stream
    size_t calculateNextStreamSize(const std::string& path, off_t offset, size_t request_size,
                                    const std::string& last_file_path, off_t last_read_end, size_t last_stream_size);
    
    std::unique_ptr<IReader> base_reader_;
    size_t max_stream_size_;
    StreamState stream_state_;
    Stats stats_;
};

} // namespace gcscfuse
