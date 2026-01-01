// GCS filesystem class implementation

#include "gcs_fs.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <set>
#include <chrono>
#include <cstdarg>
#include <fuse_log.h>

GCSFS::GCSFS(const std::string& bucket_name, const GCSFSConfig& config)
    : bucket_name_(bucket_name),
      config_(config),
      gcs_client_()
{
    // Set up FUSE logging if debug or verbose mode enabled
    if (config_.debug_mode || config_.verbose_logging) {
        fuse_set_log_func([](enum fuse_log_level level, const char *fmt, va_list ap) {
            const char* level_str = "UNKNOWN";
            switch (level) {
                case FUSE_LOG_EMERG:   level_str = "EMERG"; break;
                case FUSE_LOG_ALERT:   level_str = "ALERT"; break;
                case FUSE_LOG_CRIT:    level_str = "CRIT"; break;
                case FUSE_LOG_ERR:     level_str = "ERR"; break;
                case FUSE_LOG_WARNING: level_str = "WARNING"; break;
                case FUSE_LOG_NOTICE:  level_str = "NOTICE"; break;
                case FUSE_LOG_INFO:    level_str = "INFO"; break;
                case FUSE_LOG_DEBUG:   level_str = "DEBUG"; break;
            }
            fprintf(stderr, "[FUSE-%s] ", level_str);
            vfprintf(stderr, fmt, ap);
            fprintf(stderr, "\n");
        });
    }
    
    std::cout << "Initializing GCSFS for bucket: " << bucket_name_ << std::endl;
    if (config_.debug_mode) {
        std::cout << "[DEBUG] Stat cache: " << (config_.enable_stat_cache ? "enabled" : "disabled") << std::endl;
        if (config_.enable_stat_cache) {
            std::cout << "[DEBUG] Stat cache TTL: " << config_.stat_cache_timeout << " seconds" << std::endl;
        }
        std::cout << "[DEBUG] File content cache: " << (config_.enable_file_content_cache ? "enabled" : "disabled") << std::endl;
    }
    stat_cache_.setCacheTimeout(config_.stat_cache_timeout);
    
    // Initialize reader based on configuration
    auto gcs_reader = std::make_unique<gcscfuse::GCSDirectReader>(
        bucket_name_, 
        gcs_client_, 
        config_.debug_mode);
    
    if (config_.enable_file_content_cache) {
        reader_ = std::make_unique<gcscfuse::CachedReader>(
            std::move(gcs_reader),
            config_.debug_mode,
            config_.verbose_logging);
    } else {
        reader_ = std::move(gcs_reader);
    }
}

void GCSFS::loadFileList() const
{
    // Deprecated - now using lazy per-directory loading
    // Kept for compatibility but does nothing
}

bool GCSFS::isValidPath(const std::string& path) const
{
    if (path == root_path_) return true;
    
    // Check stat cache first
    if (config_.enable_stat_cache) {
        auto cached = stat_cache_.getStat(path);
        if (cached.has_value()) {
            return true;
        }
    }
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Check if it's a file by trying to get its metadata
    if (gcs_client_.objectExists(bucket_name_, object_name)) {
        return true;
    }
    
    // Check if it's a directory by listing with prefix
    std::string dir_prefix = object_name;
    if (!dir_prefix.empty() && dir_prefix.back() != '/') {
        dir_prefix += '/';
    }
    
    return gcs_client_.directoryExists(bucket_name_, dir_prefix);
}

bool GCSFS::isDirectory(const std::string& path) const
{
    if (path == root_path_) return true;
    
    // Check stat cache first
    if (config_.enable_stat_cache) {
        auto cached = stat_cache_.getStat(path);
        if (cached.has_value() && cached->is_directory) {
            return true;
        }
    }
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Check if any objects exist with this prefix
    std::string dir_prefix = object_name;
    if (!dir_prefix.empty() && dir_prefix.back() != '/') {
        dir_prefix += '/';
    }
    
    return gcs_client_.directoryExists(bucket_name_, dir_prefix);
}

int GCSFS::getattr(const char *path, struct stat *stbuf, struct fuse_file_info *)
{
    const auto ptr = this_();
    
    memset(stbuf, 0, sizeof(struct stat));
    
    // Root directory handling
    if (path == ptr->rootPath()) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // 1. Check write buffer first (for dirty/modified files)
    auto wb_it = ptr->write_buffers_.find(object_name);
    if (wb_it != ptr->write_buffers_.end()) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<off_t>(wb_it->second.length());
        stbuf->st_mtime = time(nullptr);
        if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] getattr from write buffer: " << path << std::endl;
        }
        return 0;
    }
    
    // 2. Try to get stat from cache (if enabled)
    if (ptr->config_.enable_stat_cache) {
        auto cached_stat = ptr->stat_cache_.getStat(path);
        if (cached_stat.has_value()) {
            const auto& info = cached_stat.value();
            stbuf->st_mode = info.mode;
            stbuf->st_nlink = info.is_directory ? 2 : 1;
            stbuf->st_size = info.size;
            stbuf->st_mtime = info.mtime;
            
            if (ptr->config_.debug_mode) {
                std::cout << "[DEBUG] ✓ Stat cache HIT for: " << path << std::endl;
            }
            return 0;
        } else if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] ✗ Stat cache MISS for: " << path << " (expired or not cached)" << std::endl;
        }
    }
    
    // 3. Not in write buffer or cache - fetch from GCS and populate cache
    if (!ptr->isValidPath(path)) {
        return -ENOENT;
    }
    
    // Check if it's a directory
    if (ptr->isDirectory(path)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        
        // Populate stat cache for directory
        if (ptr->config_.enable_stat_cache) {
            ptr->stat_cache_.insertDirectory(path);
        }
        return 0;
    }
    
    // It's a file - fetch metadata from GCS
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    
    auto obj_meta = ptr->gcs_client_.getObjectMetadata(ptr->bucket_name_, object_name);
    if (!obj_meta.has_value()) {
        return -ENOENT;
    }
    
    stbuf->st_size = static_cast<off_t>(obj_meta->size);
    stbuf->st_mtime = std::chrono::system_clock::to_time_t(obj_meta->updated);
    
    // Populate stat cache for future requests
    if (ptr->config_.enable_stat_cache) {
        ptr->stat_cache_.insertFile(path, stbuf->st_size, stbuf->st_mtime);
    }
    
    return 0;
}

int GCSFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t, struct fuse_file_info *, enum fuse_readdir_flags)
{
    const auto ptr = this_();
    
    std::string dir_path = path;
    if (!dir_path.empty() && dir_path[0] == '/') {
        dir_path = dir_path.substr(1);
    }
    
    // Add trailing slash for non-root directories
    if (!dir_path.empty() && dir_path.back() != '/') {
        dir_path += '/';
    }
    
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
    
    // Lazy load: List objects with this directory as prefix
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Listing directory: " << (dir_path.empty() ? "/" : dir_path) << std::endl;
    }
    
    // Track subdirectories and files at this level
    std::set<std::string> entries;
    
    try {
        // List objects with this directory prefix and delimiter to get immediate children
        auto objects = ptr->gcs_client_.listObjects(ptr->bucket_name_, dir_path, "/");
        
        for (const auto& obj_meta : objects) {
            std::string name = obj_meta.name;
            
            // Remove directory prefix
            std::string relative = dir_path.empty() ? name : name.substr(dir_path.length());
            
            // Skip if empty or is a directory marker (ends with /)
            if (relative.empty() || relative.back() == '/') {
                continue;
            }
            
            // Find first slash to determine if subdirectory or file
            size_t slash_pos = relative.find('/');
            std::string entry_name;
            bool is_subdir = false;
            
            if (slash_pos != std::string::npos) {
                // Subdirectory
                entry_name = relative.substr(0, slash_pos);
                is_subdir = true;
            } else {
                // File in this directory
                entry_name = relative;
                is_subdir = false;
            }
            
            if (!entry_name.empty() && entries.find(entry_name) == entries.end()) {
                entries.insert(entry_name);
                
                // Populate stat cache
                if (ptr->config_.enable_stat_cache) {
                    std::string full_path = path;
                    if (full_path != "/" && full_path.back() != '/') {
                        full_path += "/";
                    }
                    full_path += entry_name;
                    
                    if (is_subdir) {
                        ptr->stat_cache_.insertDirectory(full_path);
                    } else {
                        off_t size = static_cast<off_t>(obj_meta.size);
                        time_t mtime = std::chrono::system_clock::to_time_t(obj_meta.updated);
                        ptr->stat_cache_.insertFile(full_path, size, mtime);
                    }
                }
                
                if (ptr->config_.debug_mode) {
                    std::cout << "[DEBUG] Found entry: " << entry_name 
                              << (is_subdir ? " (dir)" : " (file)") << std::endl;
                }
                
                filler(buf, entry_name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing directory: " << e.what() << std::endl;
        return -EIO;
    }
    
    return 0;
}

int GCSFS::open(const char *path, struct fuse_file_info *fi)
{
    const auto ptr = this_();
    
    // Allow opening files for write
    int flags = fi->flags & O_ACCMODE;
    if (flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR) {
        return -EINVAL;
    }
    
    // For write access, ensure file exists or can be created
    if (flags == O_WRONLY || flags == O_RDWR) {
        if (!ptr->isValidPath(path)) {
            // File doesn't exist yet - that's okay, create will be called
            return 0;
        }
        
        if (ptr->isDirectory(path)) {
            return -EISDIR;
        }
    } else {
        // Read-only access
        if (!ptr->isValidPath(path)) {
            return -ENOENT;
        }
        
        if (ptr->isDirectory(path)) {
            return -EISDIR;
        }
    }
    
    return 0;
}

int GCSFS::read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *)
{
    const auto ptr = this_();
    
    if (!ptr->isValidPath(path)) {
        return -ENOENT;
    }
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Check write buffer first (for recently written data)
    auto wb_it = ptr->write_buffers_.find(object_name);
    if (wb_it != ptr->write_buffers_.end()) {
        const std::string& content = wb_it->second;
        if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] Reading from write buffer: " << object_name << std::endl;
        }
        size_t len = content.length();
        if (static_cast<size_t>(offset) < len) {
            if (offset + size > len) {
                size = len - offset;
            }
            memcpy(buf, content.c_str() + offset, size);
        } else {
            size = 0;
        }
        return static_cast<int>(size);
    }

    // Fall back to reader for persistent storage (GCS/Cache)
    return ptr->reader_->read(object_name, buf, size, offset);
}

// ==================== Write Operations ====================

int GCSFS::create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    const auto ptr = this_();
    
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Creating file: " << path << std::endl;
    }
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Initialize empty file in write buffer
    ptr->write_buffers_[object_name] = "";
    ptr->markDirty(object_name);
    
    // Update stat cache
    if (ptr->config_.enable_stat_cache) {
        ptr->stat_cache_.insertFile(path, 0, time(nullptr));
    }
    
    fi->flags |= O_CREAT;
    return 0;
}

int GCSFS::write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *)
{
    const auto ptr = this_();
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Writing " << size << " bytes to " << object_name 
                  << " at offset " << offset << std::endl;
    }
    
    // Get or create write buffer
    std::string& content = ptr->write_buffers_[object_name];
    
    // If writing beyond current size, expand with zeros
    if (static_cast<size_t>(offset) > content.size()) {
        content.resize(offset, '\0');
    }
    
    // Write data
    if (static_cast<size_t>(offset) + size > content.size()) {
        content.resize(offset + size);
    }
    
    memcpy(&content[offset], buf, size);
    ptr->markDirty(object_name);
    
    // Update stat cache with new size
    if (ptr->config_.enable_stat_cache) {
        ptr->stat_cache_.insertFile(path, content.size(), time(nullptr));
    }
    
    return static_cast<int>(size);
}

int GCSFS::truncate(const char *path, off_t size, struct fuse_file_info *)
{
    const auto ptr = this_();
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Truncating " << object_name << " to " << size << " bytes" << std::endl;
    }
    
    // Get current content
    std::string& content = ptr->write_buffers_[object_name];
    if (content.empty() && ptr->isValidPath(path)) {
        // Load from persistent storage if not in write buffer yet
        content.resize(1024 * 1024); // Start with 1MB
        off_t total_read = 0;
        
        while (true) {
            if (total_read >= static_cast<off_t>(content.size())) {
                content.resize(content.size() * 2);
            }
            
            int bytes_read = ptr->reader_->read(
                object_name,
                &content[total_read],
                content.size() - total_read,
                total_read);
            
            if (bytes_read <= 0) {
                break;
            }
            
            total_read += bytes_read;
        }
        
        content.resize(total_read);
    }
    
    // Resize
    content.resize(size, '\0');
    ptr->markDirty(object_name);
    
    // Update stat cache
    if (ptr->config_.enable_stat_cache) {
        ptr->stat_cache_.insertFile(path, size, time(nullptr));
    }
    
    return 0;
}

int GCSFS::flush(const char *path, struct fuse_file_info *)
{
    const auto ptr = this_();
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Only flush if file is dirty
    if (!ptr->isDirty(object_name)) {
        return 0;
    }
    
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Flushing " << object_name << " to GCS" << std::endl;
    }
    
    return ptr->uploadToGCS(path);
}

int GCSFS::release(const char *path, struct fuse_file_info *)
{
    const auto ptr = this_();
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Sync any pending writes on file close
    if (ptr->isDirty(object_name)) {
        if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] Releasing and syncing " << object_name << std::endl;
        }
        int result = ptr->uploadToGCS(path);
        if (result != 0) {
            return result;
        }
    }
    
    return 0;
}

int GCSFS::unlink(const char *path)
{
    const auto ptr = this_();
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Deleting " << object_name << std::endl;
    }
    
    if (!ptr->isValidPath(path)) {
        return -ENOENT;
    }
    
    if (ptr->isDirectory(path)) {
        return -EISDIR;
    }
    
    // Delete from GCS
    try {
        bool success = ptr->gcs_client_.deleteObject(ptr->bucket_name_, object_name);
        if (!success) {
            std::cerr << "Error deleting object: " << object_name << std::endl;
            return -EIO;
        }
        
        // Remove from caches
        ptr->reader_->invalidate(object_name);
        ptr->write_buffers_.erase(object_name);
        ptr->dirty_files_.erase(object_name);
        ptr->stat_cache_.remove(std::string("/") + object_name);
        
        if (ptr->config_.verbose_logging) {
            std::cout << "Deleted " << object_name << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception deleting file: " << e.what() << std::endl;
        return -EIO;
    }
}

// ==================== Write Helper Functions ====================

int GCSFS::uploadToGCS(const std::string& path) const
{
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    auto it = write_buffers_.find(object_name);
    if (it == write_buffers_.end()) {
        // Nothing to upload
        return 0;
    }
    
    const std::string& content = it->second;
    
    if (config_.verbose_logging) {
        std::cout << "Uploading " << content.size() << " bytes to " << object_name << std::endl;
    }
    
    try {
        bool success = gcs_client_.writeObject(bucket_name_, object_name, content);
        
        if (!success) {
            std::cerr << "Error uploading object: " << object_name << std::endl;
            return -EIO;
        }
        
        // Invalidate cache to ensure fresh read on next access
        reader_->invalidate(object_name);
        
        // Clear dirty flag
        dirty_files_[object_name] = false;
        
        // Update stat cache
        if (config_.enable_stat_cache) {
            stat_cache_.insertFile(path, content.size(), time(nullptr));
        }
        
        if (config_.debug_mode) {
            std::cout << "[DEBUG] Successfully uploaded " << object_name << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception uploading file: " << e.what() << std::endl;
        return -EIO;
    }
}

void GCSFS::markDirty(const std::string& path) const
{
    dirty_files_[path] = true;
}

bool GCSFS::isDirty(const std::string& path) const
{
    auto it = dirty_files_.find(path);
    return it != dirty_files_.end() && it->second;
}
