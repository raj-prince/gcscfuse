// GCS filesystem class implementation

#include "gcs_fs.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <set>
#include <chrono>
#include <thread>
#include <cstdarg>
#include <fuse_log.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <sstream>

// Forward declare for friend relationship
class GCSFS;

namespace {
    // RAII helper class to track I/O depth for read operations
    class IODepthTracker {
        GCSFS* fs_;
        std::string path_;
    public:
        IODepthTracker(GCSFS* fs, const std::string& path, off_t offset, size_t size) 
            : fs_(fs), path_(path) {
            fs_->incrementIODepth(path_, offset, size);
        }
        
        ~IODepthTracker() {
            fs_->decrementIODepth(path_);
        }
    };
    
    // Get device major/minor number for a mount point
    bool getDeviceMajorMinor(const std::string& mount_point, unsigned int& major, unsigned int& minor) {
        struct stat st;
        if (stat(mount_point.c_str(), &st) != 0) {
            std::cerr << "[WARN] Failed to stat mount point: " << mount_point << std::endl;
            return false;
        }
        
        major = (st.st_dev >> 8) & 0xff;
        minor = st.st_dev & 0xff;
        return true;
    }
    
    // Set kernel read-ahead via sysfs
    // Why system() can cause VM to hang:
    // 1. system() spawns /bin/sh and blocks waiting for completion
    // 2. If sudo prompts for password (even with -n), shell may hang waiting for input
    // 3. Shell I/O redirection can deadlock if file descriptors are inherited incorrectly
    // 4. No timeout - if subprocess hangs, system() blocks forever
    // 5. During FUSE init(), this blocks the entire mount process
    
    // Generic helper to set FUSE/kernel parameters via sysfs
    bool setFUSEParameter(const std::string& sysfs_path, int value, const std::string& param_name, bool debug) {
        if (debug) {
            std::cout << "[DEBUG] Setting " << param_name << " to " << value 
                      << " via " << sysfs_path << std::endl;
        }
        
        // First try direct write (works if running as root)
        std::ofstream sysfs_file(sysfs_path);
        if (sysfs_file.is_open()) {
            sysfs_file << value;
            sysfs_file.close();
            if (sysfs_file.good()) {
                if (debug) {
                    std::cout << "[DEBUG] " << param_name << " successfully set to " << value << std::endl;
                }
                return true;
            }
        }
        
        // Fallback to sudo with fork/exec and timeout
        if (debug) {
            std::cout << "[DEBUG] Direct write failed, trying with sudo (2s timeout)..." << std::endl;
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: execute sudo command
            int devnull = open("/dev/null", O_WRONLY);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
            
            std::ostringstream val_str;
            val_str << value;
            
            execlp("/bin/sh", "sh", "-c",
                   ("echo " + val_str.str() + " | sudo -n tee " + sysfs_path).c_str(),
                   nullptr);
            
            _exit(1);
        } else if (pid > 0) {
            // Parent process: wait with timeout
            auto start = std::chrono::steady_clock::now();
            const int timeout_seconds = 2;
            
            while (true) {
                int status;
                pid_t result = waitpid(pid, &status, WNOHANG);
                
                if (result == pid) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        if (debug) {
                            std::cout << "[DEBUG] " << param_name << " successfully set to " << value << " (via sudo)" << std::endl;
                        }
                        return true;
                    } else {
                        std::cerr << "[WARN] Failed to set " << param_name << ". "
                                  << "Run as root or configure passwordless sudo." << std::endl;
                        return false;
                    }
                }
                
                auto elapsed = std::chrono::steady_clock::now() - start;
                if (elapsed > std::chrono::seconds(timeout_seconds)) {
                    std::cerr << "[WARN] " << param_name << " setup timed out after " << timeout_seconds << "s. Killing subprocess." << std::endl;
                    kill(pid, SIGKILL);
                    waitpid(pid, nullptr, 0);
                    return false;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        } else {
            std::cerr << "[WARN] Failed to fork for " << param_name << " setup" << std::endl;
            return false;
        }
        
        return false;
    }
    
    void setKernelReadAhead(const std::string& mount_point, int readahead_kb, bool debug) {
        unsigned int major, minor;
        if (!getDeviceMajorMinor(mount_point, major, minor)) {
            return;
        }
        
        // Build sysfs path: /sys/class/bdi/<major>:<minor>/read_ahead_kb
        std::ostringstream sysfs_path;
        sysfs_path << "/sys/class/bdi/" << major << ":" << minor << "/read_ahead_kb";
        
        setFUSEParameter(sysfs_path.str(), readahead_kb, "kernel read-ahead (KB)", debug);
    }
    
    void setMaxBackground(const std::string& mount_point, int max_background, bool debug) {
        unsigned int major, minor;
        if (!getDeviceMajorMinor(mount_point, major, minor)) {
            return;
        }
        
        // Build sysfs path: /sys/fs/fuse/connections/<minor>/max_background
        std::ostringstream sysfs_path;
        sysfs_path << "/sys/fs/fuse/connections/" << minor << "/max_background";
        
        setFUSEParameter(sysfs_path.str(), max_background, "max_background", debug);
    }
    
    void setCongestionThreshold(const std::string& mount_point, int congestion_threshold, bool debug) {
        unsigned int major, minor;
        if (!getDeviceMajorMinor(mount_point, major, minor)) {
            return;
        }
        
        // Build sysfs path: /sys/fs/fuse/connections/<minor>/congestion_threshold
        std::ostringstream sysfs_path;
        sysfs_path << "/sys/fs/fuse/connections/" << minor << "/congestion_threshold";
        
        setFUSEParameter(sysfs_path.str(), congestion_threshold, "congestion_threshold", debug);
    }
}  // End anonymous namespace

// I/O Depth tracking implementation
void GCSFS::incrementIODepth(const std::string& path, off_t offset, size_t size) const {
    std::lock_guard<std::mutex> lock(io_depth_mutex_);
    
    // Initialize if not exists
    if (read_io_depth_.find(path) == read_io_depth_.end()) {
        read_io_depth_[path].store(0);
        read_io_depth_max_[path] = 0;
    }
    
    // Increment current depth
    int current_depth = ++read_io_depth_[path];
    
    // Update max if needed
    if (current_depth > read_io_depth_max_[path]) {
        read_io_depth_max_[path] = current_depth;
    }
    
    // Log if verbose or debug
    if (config_.verbose_logging || config_.debug_mode) {
        std::cout << "[IO-DEPTH] " << path << " - current: " << current_depth 
                  << ", max: " << read_io_depth_max_[path] 
                  << " (offset: " << offset << ", size: " << size << " bytes)" << std::endl;
    }
}

void GCSFS::decrementIODepth(const std::string& path) const {
    --read_io_depth_[path];
}

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
        if (config_.enable_dummy_reader) {
            std::cout << "[DEBUG] Using dummy reader (returns zeros)" << std::endl;
        }
    }
    stat_cache_.setCacheTimeout(config_.stat_cache_timeout);
    
    // Initialize reader based on configuration
    std::unique_ptr<gcscfuse::IReader> base_reader;
    
    if (config_.enable_dummy_reader) {
        // Use dummy reader for testing
        base_reader = std::make_unique<gcscfuse::DummyReader>();
    } else {
        // Use GCS reader
        base_reader = std::make_unique<gcscfuse::GCSDirectReader>(
            bucket_name_, 
            gcs_client_, 
            config_.debug_mode);
    }
    
    if (config_.enable_file_content_cache) {
        reader_ = std::make_unique<gcscfuse::CachedReader>(
            std::move(base_reader),
            config_.debug_mode,
            config_.verbose_logging);
    } else {
        reader_ = std::move(base_reader);
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

void* GCSFS::init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    const auto ptr = this_();
    
    // Apply FUSE performance configuration
    if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Configuring FUSE performance options:" << std::endl;
        std::cout << "[DEBUG]   max_background: " << ptr->config_.max_background << std::endl;
        std::cout << "[DEBUG]   congestion_threshold: " << ptr->config_.congestion_threshold << std::endl;
        std::cout << "[DEBUG]   async_read: " << (ptr->config_.async_read ? "enabled" : "disabled") << std::endl;
        std::cout << "[DEBUG]   max_readahead: " << ptr->config_.max_readahead << " bytes" << std::endl;
    }
    
    // Configure connection options
    conn->max_background = ptr->config_.max_background;
    conn->congestion_threshold = ptr->config_.congestion_threshold;
    
    // Configure async read
    if (ptr->config_.async_read) {
        conn->want |= FUSE_CAP_ASYNC_READ;
    } else {
        conn->want &= ~FUSE_CAP_ASYNC_READ;
    }
    
    // Configure kernel readahead (only if explicitly set > 0)
    if (ptr->config_.max_readahead > 0) {
        // Convert KB to bytes for FUSE
        conn->max_readahead = ptr->config_.max_readahead * 1024;
        
        if (ptr->config_.debug_mode) {
            std::cout << "[DEBUG] Setting FUSE max_readahead to " << ptr->config_.max_readahead << " KB (" 
                      << conn->max_readahead << " bytes)" << std::endl;
        }
    } else if (ptr->config_.debug_mode) {
        std::cout << "[DEBUG] Using system default read-ahead (max_readahead not configured)" << std::endl;
    }
    
    return ptr;
}

int GCSFS::run(int argc, char **argv)
{
    // Use default FUSE run - kernel read-ahead will be configured after mount via init callback
    return Fusepp::Fuse<GCSFS>::run(argc, argv);
}

void GCSFS::configureFUSEKernelSettings() const
{
    if (config_.debug_mode) {
        std::cout << "[DEBUG] Waiting for mount to complete (max 5s timeout)..." << std::endl;
    }
    
    // Wait and verify mount is actually completed (with timeout)
    bool mounted = false;
    for (int i = 0; i < 50; i++) {  // Max 5 seconds
        std::ifstream mounts("/proc/mounts");
        std::string line;
        while (std::getline(mounts, line)) {
            if (line.find(config_.mount_point) != std::string::npos) {
                mounted = true;
                break;
            }
        }
        if (mounted) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!mounted) {
        std::cerr << "[WARN] Mount point not detected in /proc/mounts after 5s, skipping kernel parameter setup" << std::endl;
        return;
    }
    
    if (config_.debug_mode) {
        std::cout << "[DEBUG] Mount confirmed, configuring FUSE kernel parameters" << std::endl;
    }
    
    // Set kernel read-ahead if configured
    if (config_.max_readahead > 0) {
        setKernelReadAhead(config_.mount_point, config_.max_readahead, config_.debug_mode);
    }
    
    // Set max_background via sysfs (overrides FUSE connection setting)
    setMaxBackground(config_.mount_point, config_.max_background, config_.debug_mode);
    
    // Set congestion_threshold via sysfs (overrides FUSE connection setting)
    setCongestionThreshold(config_.mount_point, config_.congestion_threshold, config_.debug_mode);
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
        // List objects with this directory prefix (no delimiter to get all objects)
        // We manually filter to show only direct children
        // Note: Using delimiter would be more efficient, but the GCS C++ SDK's
        // ListObjectsReader only provides objects, not common prefixes
        auto objects = ptr->gcs_client_.listObjects(ptr->bucket_name_, dir_path, "");
        
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
                // Subdirectory - extract just the directory name
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
    
    // Track I/O depth for this read (RAII - auto decrements on exit)
    IODepthTracker tracker(ptr, object_name, offset, size);
    
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
