#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "gcs/gcs_client.hpp"

namespace gcscfuse {

// Interface for reading file content from persistent storage
// This abstracts away the actual source (GCS, cache, dummy data)
class IReader {
public:
    virtual ~IReader() = default;
    
    // Read content from a file at the given object name
    // Returns the number of bytes read, or -1 on error
    virtual int read(const std::string& object_name, 
                     char* buf, 
                     size_t size, 
                     off_t offset) = 0;
    
    // Optional: Invalidate cache for a specific object
    virtual void invalidate(const std::string& object_name) {}
    
    // Optional: Clear all cached data
    virtual void clear() {}
};

// Direct GCS reader - terminal implementation that always reads from GCS
class GCSDirectReader : public IReader {
public:
    GCSDirectReader(const std::string& bucket_name, 
                    gcscfuse::GCSClient& gcs_client,
                    bool debug_mode = false)
        : bucket_name_(bucket_name),
          gcs_client_(gcs_client),
          debug_mode_(debug_mode) {}
    
    int read(const std::string& object_name, 
             char* buf, 
             size_t size, 
             off_t offset) override {
        if (debug_mode_) {
            std::cout << "[DEBUG] Reading from GCS: " << object_name << std::endl;
        }
        
        gcscfuse::IGCSSDKClient::ReadObjectRequest req;
        req.bucket_name = bucket_name_;
        req.object_name = object_name;
        req.range = std::make_optional(std::make_pair(
            static_cast<std::int64_t>(offset), 
            static_cast<std::int64_t>(offset + size)));
        
        std::string content = gcs_client_.readObject(req);
        size_t len = content.length();
        
        if (len > 0) {
            if (len < size) size = len;
            std::memcpy(buf, content.c_str(), size);
            return static_cast<int>(size);
        }
        
        return 0;
    }

private:
    std::string bucket_name_;
    gcscfuse::GCSClient& gcs_client_;
    bool debug_mode_;
};

// Cached reader - decorator that wraps another reader with caching
class CachedReader : public IReader {
public:
    CachedReader(std::unique_ptr<IReader> underlying_reader,
                 bool debug_mode = false,
                 bool verbose_logging = false)
        : underlying_reader_(std::move(underlying_reader)),
          debug_mode_(debug_mode),
          verbose_logging_(verbose_logging) {}
    
    int read(const std::string& object_name, 
             char* buf, 
             size_t size, 
             off_t offset) override {
        // Check cache first
        auto it = cache_.find(object_name);
        if (it != cache_.end()) {
            if (debug_mode_) {
                std::cout << "[DEBUG] Cache hit for: " << object_name << std::endl;
            }
            return readFromCache(it->second, buf, size, offset);
        }
        
        // Cache miss - need to read full object for caching
        if (debug_mode_) {
            std::cout << "[DEBUG] Cache miss for: " << object_name << std::endl;
        }
        
        // Read entire object from beginning
        std::string full_content;
        full_content.resize(1024 * 1024); // Start with 1MB
        int total_read = 0;
        
        // Read in chunks until we get the full object
        while (true) {
            if (total_read >= static_cast<int>(full_content.size())) {
                full_content.resize(full_content.size() * 2);
            }
            
            int bytes_read = underlying_reader_->read(
                object_name,
                &full_content[total_read],
                full_content.size() - total_read,
                0);  // Always read from offset 0 to get full object
            
            if (bytes_read <= 0) {
                break;
            }
            
            // If we got less than requested, we're done
            if (bytes_read < static_cast<int>(full_content.size() - total_read)) {
                total_read += bytes_read;
                break;
            }
            
            total_read += bytes_read;
        }
        
        full_content.resize(total_read);
        
        if (total_read == 0) {
            return 0;
        }
        
        // Cache it
        cache_[object_name] = full_content;
        
        if (verbose_logging_) {
            std::cout << "Cached " << total_read << " bytes for " << object_name << std::endl;
        }
        
        // Return requested portion
        return readFromCache(full_content, buf, size, offset);
    }
    
    void invalidate(const std::string& object_name) override {
        cache_.erase(object_name);
    }
    
    void clear() override {
        cache_.clear();
    }

private:
    int readFromCache(const std::string& content, char* buf, size_t size, off_t offset) {
        size_t len = content.length();
        if (static_cast<size_t>(offset) >= len) {
            return 0;
        }
        
        if (offset + size > len) {
            size = len - offset;
        }
        
        std::memcpy(buf, content.c_str() + offset, size);
        return static_cast<int>(size);
    }

    std::unique_ptr<IReader> underlying_reader_;
    std::unordered_map<std::string, std::string> cache_;
    bool debug_mode_;
    bool verbose_logging_;
};

// Dummy reader - returns predefined data for testing
class DummyReader : public IReader {
public:
    DummyReader() = default;
    
    // Set mock data for a specific object
    void setMockData(const std::string& object_name, const std::string& content) {
        mock_data_[object_name] = content;
    }
    
    int read(const std::string& object_name, 
             char* buf, 
             size_t size, 
             off_t offset) override {
        auto it = mock_data_.find(object_name);
        if (it == mock_data_.end()) {
            return -1; // Object not found
        }
        
        const std::string& content = it->second;
        size_t len = content.length();
        
        if (static_cast<size_t>(offset) >= len) {
            return 0;
        }
        
        if (offset + size > len) {
            size = len - offset;
        }
        
        std::memcpy(buf, content.c_str() + offset, size);
        return static_cast<int>(size);
    }
    
    void clear() override {
        mock_data_.clear();
    }

private:
    std::unordered_map<std::string, std::string> mock_data_;
};

} // namespace gcscfuse
