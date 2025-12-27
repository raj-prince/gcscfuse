// GCS filesystem class definition

#pragma once

#include <string>
#include <map>
#include <vector>
#include "fuse_cpp_wrapper.hpp"
#include "google/cloud/storage/client.h"
#include "stat_cache.hpp"
#include "config.hpp"

namespace gcs = ::google::cloud::storage;

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

    // FUSE operations
    static int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *);
    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags);
    static int open(const char *path, struct fuse_file_info *fi);
    static int read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *);

    // Accessors
    const std::string& bucketName() const { return bucket_name_; }
    const std::string& rootPath() const { return root_path_; }

private:
    std::string bucket_name_;
    std::string root_path_ = "/";
    GCSFSConfig config_;
    mutable gcs::Client client_;  // mutable because GCS client methods are non-const
    
    // Stat cache for metadata
    mutable StatCache stat_cache_;
    
    // Cache for file contents
    mutable std::map<std::string, std::string> file_cache_;
    mutable std::vector<std::string> file_list_;
    mutable bool files_loaded_ = false;

    // Helper functions
    void loadFileList() const;
    const std::string& getFileContent(const std::string& path) const;
    bool isValidPath(const std::string& path) const;
    bool isDirectory(const std::string& path) const;
};
