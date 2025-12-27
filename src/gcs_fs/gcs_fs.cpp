// GCS filesystem class implementation

#include "gcs_fs.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <set>
#include <chrono>

GCSFS::GCSFS(const std::string& bucket_name, const GCSFSConfig& config)
    : bucket_name_(bucket_name),
      config_(config),
      client_(gcs::Client())
{
    std::cout << "Initializing GCSFS for bucket: " << bucket_name_ << std::endl;
    if (config_.debug_mode) {
        std::cout << "[DEBUG] Stat cache: " << (config_.enable_stat_cache ? "enabled" : "disabled") << std::endl;
        std::cout << "[DEBUG] File content cache: " << (config_.enable_file_content_cache ? "enabled" : "disabled") << std::endl;
    }
}

void GCSFS::loadFileList() const
{
    if (files_loaded_) return;
    
    std::cout << "Loading file list from bucket: " << bucket_name_ << std::endl;
    
    try {
        for (auto&& object_metadata : client_.ListObjects(bucket_name_)) {
            if (!object_metadata) {
                std::cerr << "Error listing objects: " << object_metadata.status() << std::endl;
                break;
            }
            
            std::string name = object_metadata->name();
            // Skip directories (objects ending with /)
            if (!name.empty() && name.back() != '/') {
                file_list_.push_back(name);
                
                // Add to stat cache with metadata (if enabled)
                if (config_.enable_stat_cache) {
                    off_t size = static_cast<off_t>(object_metadata->size());
                    time_t mtime = std::chrono::system_clock::to_time_t(
                        object_metadata->time_created()
                    );
                    stat_cache_.insertFile("/" + name, size, mtime);
                }
                
                if (config_.verbose_logging) {
                    std::cout << "Found file: " << name << " (size: " 
                              << object_metadata->size() << " bytes)" << std::endl;
                } else if (config_.debug_mode) {
                    std::cout << "Found file: " << name << std::endl;
                }
            }
        }
        files_loaded_ = true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading file list: " << e.what() << std::endl;
    }
}

const std::string& GCSFS::getFileContent(const std::string& path) const
{
    // Remove leading slash
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Check cache first (if enabled)
    if (config_.enable_file_content_cache) {
        auto it = file_cache_.find(object_name);
        if (it != file_cache_.end()) {
            if (config_.debug_mode) {
                std::cout << "[DEBUG] Cache hit for: " << object_name << std::endl;
            }
            return it->second;
        }
    }
    
    // Read from GCS
    if (config_.debug_mode) {
        std::cout << "[DEBUG] Reading from GCS: " << object_name << std::endl;
    }
    auto reader = client_.ReadObject(bucket_name_, object_name);
    
    if (!reader) {
        std::cerr << "Error reading object: " << reader.status() << std::endl;
        static const std::string empty;
        return empty;
    }
    
    std::string content{std::istreambuf_iterator<char>{reader}, {}};
    
    // Cache the content (if enabled)
    if (config_.enable_file_content_cache) {
        file_cache_[object_name] = content;
        if (config_.verbose_logging) {
            std::cout << "Cached " << content.size() << " bytes for " << object_name << std::endl;
        }
        return file_cache_[object_name];
    } else {
        // Store temporarily for this request
        static thread_local std::string temp_content;
        temp_content = std::move(content);
        return temp_content;
    }
    return file_cache_[object_name];
}

bool GCSFS::isValidPath(const std::string& path) const
{
    if (path == root_path_) return true;
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    const_cast<GCSFS*>(this)->loadFileList();
    
    // Check if it's a file
    if (std::find(file_list_.begin(), file_list_.end(), object_name) != file_list_.end()) {
        return true;
    }
    
    // Check if it's a directory (a prefix of any file)
    std::string dir_prefix = object_name;
    if (!dir_prefix.empty() && dir_prefix.back() != '/') {
        dir_prefix += '/';
    }
    
    for (const auto& file : file_list_) {
        if (file.find(dir_prefix) == 0) {
            return true;
        }
    }
    
    return false;
}

bool GCSFS::isDirectory(const std::string& path) const
{
    if (path == root_path_) return true;
    
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    const_cast<GCSFS*>(this)->loadFileList();
    
    // Check if any file starts with this path as a directory prefix
    std::string dir_prefix = object_name;
    if (!dir_prefix.empty() && dir_prefix.back() != '/') {
        dir_prefix += '/';
    }
    
    for (const auto& file : file_list_) {
        if (file.find(dir_prefix) == 0) {
            return true;
        }
    }
    
    return false;
}

int GCSFS::getattr(const char *path, struct stat *stbuf, struct fuse_file_info *)
{
    const auto ptr = this_();
    
    memset(stbuf, 0, sizeof(struct stat));
    
    // Ensure file list is loaded to populate cache
    ptr->loadFileList();
    
    // Try to get stat from cache first (if enabled)
    if (ptr->config_.enable_stat_cache) {
        auto cached_stat = ptr->stat_cache_.getStat(path);
        if (cached_stat.has_value()) {
            const auto& info = cached_stat.value();
            stbuf->st_mode = info.mode;
            stbuf->st_nlink = info.is_directory ? 2 : 1;
            stbuf->st_size = info.size;
            stbuf->st_mtime = info.mtime;
            
            if (ptr->config_.debug_mode) {
                std::cout << "[DEBUG] Stat cache hit for: " << path << std::endl;
            }
            return 0;
        }
    }
    
    // Fallback to old behavior if not in cache
    if (path == ptr->rootPath()) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    if (!ptr->isValidPath(path)) {
        return -ENOENT;
    }
    
    // Check if it's a directory
    if (ptr->isDirectory(path)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    // It's a file - with read-write permissions
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    
    // Get file size from write buffer or cache or GCS
    std::string object_name = path;
    if (!object_name.empty() && object_name[0] == '/') {
        object_name = object_name.substr(1);
    }
    
    // Check write buffer first
    auto wb_it = ptr->write_buffers_.find(object_name);
    if (wb_it != ptr->write_buffers_.end()) {
        stbuf->st_size = static_cast<off_t>(wb_it->second.length());
    } else {
        const auto& content = ptr->getFileContent(path);
        stbuf->st_size = static_cast<off_t>(content.length());
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
    
    // Load file list
    ptr->loadFileList();
    
    // Track subdirectories and files at this level
    std::set<std::string> entries;
    
    for (const auto& file : ptr->file_list_) {
        // Check if file is in this directory
        if (dir_path.empty() || file.find(dir_path) == 0) {
            std::string relative = dir_path.empty() ? file : file.substr(dir_path.length());
            
            // Find the first slash to determine if it's a subdirectory or file
            size_t slash_pos = relative.find('/');
            std::string entry_name;
            bool is_subdir = false;
            
            if (slash_pos != std::string::npos) {
                // It's a subdirectory
                entry_name = relative.substr(0, slash_pos);
                is_subdir = true;
            } else {
                // It's a file in this directory
                entry_name = relative;
                is_subdir = false;
            }
            
            if (!entry_name.empty() && entries.find(entry_name) == entries.end()) {
                entries.insert(entry_name);
                
                // Populate stat cache for this entry (if enabled)
                if (ptr->config_.enable_stat_cache) {
                    std::string full_path = path;
                    if (full_path != "/" && full_path.back() != '/') {
                        full_path += "/";
                    }
                    full_path += entry_name;
                    
                    // Ensure the entry is in the cache
                    if (is_subdir) {
                        ptr->stat_cache_.insertDirectory(full_path);
                    }
                    // Files are already in cache from loadFileList()
                }
                
                filler(buf, entry_name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
            }
        }
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
    const std::string* content_ptr;
    
    if (wb_it != ptr->write_buffers_.end()) {
        content_ptr = &wb_it->second;
        if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] Reading from write buffer: " << object_name << std::endl;
        }
    } else {
        content_ptr = &ptr->getFileContent(path);
    }
    
    const auto& content = *content_ptr;
    const auto len = content.length();
    
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
    
    // Add to file list
    if (std::find(ptr->file_list_.begin(), ptr->file_list_.end(), object_name) == ptr->file_list_.end()) {
        ptr->file_list_.push_back(object_name);
    }
    
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
        // Load from GCS if not in write buffer yet
        content = ptr->getFileContent(path);
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
        auto status = ptr->client_.DeleteObject(ptr->bucket_name_, object_name);
        if (!status.ok()) {
            std::cerr << "Error deleting object: " << status.message() << std::endl;
            return -EIO;
        }
        
        // Remove from caches
        ptr->file_cache_.erase(object_name);
        ptr->write_buffers_.erase(object_name);
        ptr->dirty_files_.erase(object_name);
        ptr->stat_cache_.remove(std::string("/") + object_name);
        
        auto it = std::find(ptr->file_list_.begin(), ptr->file_list_.end(), object_name);
        if (it != ptr->file_list_.end()) {
            ptr->file_list_.erase(it);
        }
        
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
        auto writer = client_.WriteObject(bucket_name_, object_name);
        writer.write(content.c_str(), content.size());
        writer.Close();
        
        if (!writer.metadata()) {
            std::cerr << "Error uploading object: " << writer.metadata().status() << std::endl;
            return -EIO;
        }
        
        // Update file cache
        if (config_.enable_file_content_cache) {
            file_cache_[object_name] = content;
        }
        
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
