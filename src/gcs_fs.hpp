// GCS filesystem class definition

#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "fuse_cpp_wrapper.hpp"
#include "gcs/gcs_client.hpp"
#include "stat_cache.hpp"
#include "config.hpp"
#include "reader.hpp"

/**
 * GCSFS - A FUSE filesystem that reads files from Google Cloud Storage
 * 
 * This implementation provides read-only access to files in a GCS bucket.
 * Files are cached in memory upon first read.
 */
class GCSFS : public Fusepp::Fuse<GCSFS>
{
public:
    explicit GCSFS(const std::string& bucket_name, const GCSFSConfig& config);
    ~GCSFS() override = default;

    // FUSE operations - read
    static int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *);
    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags);
    static int open(const char *path, struct fuse_file_info *fi);
    static int read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *);
    
    // FUSE operations - write
    static int create(const char *path, mode_t mode, struct fuse_file_info *fi);
    static int write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *);
    static int truncate(const char *path, off_t size, struct fuse_file_info *fi);
    static int flush(const char *path, struct fuse_file_info *fi);
    static int release(const char *path, struct fuse_file_info *fi);
    static int unlink(const char *path);

    // Accessors
    const std::string& bucketName() const { return bucket_name_; }
    const std::string& rootPath() const { return root_path_; }

private:
    std::string bucket_name_;
    std::string root_path_ = "/";
    GCSFSConfig config_;
    mutable gcscfuse::GCSClient gcs_client_;
    
    // Stat cache for metadata
    mutable StatCache stat_cache_;
    
    // Reader abstraction for persistent storage (GCS/Cache/Dummy)
    std::unique_ptr<gcscfuse::IReader> reader_;
    
    // Deprecated: file_list_ and files_loaded_ are no longer used (lazy loading per-directory now)
    // mutable std::vector<std::string> file_list_;
    // mutable bool files_loaded_ = false;
    
    // Write buffers for modified files (path -> content)
    mutable std::map<std::string, std::string> write_buffers_;
    mutable std::map<std::string, bool> dirty_files_;  // Track which files need sync

    // Helper functions
    void loadFileList() const;
    bool isValidPath(const std::string& path) const;
    bool isDirectory(const std::string& path) const;
    
    // Write helpers
    int uploadToGCS(const std::string& path) const;
    void markDirty(const std::string& path) const;
    bool isDirty(const std::string& path) const;
};
