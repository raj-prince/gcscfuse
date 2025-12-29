#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "google/cloud/storage/client.h"
#include "gcs_sdk_interface.hpp"

namespace gcs = ::google::cloud::storage;

namespace gcscfuse {

/**
 * ObjectMetadata - Simplified metadata structure for GCS objects
 */
struct ObjectMetadata {
    std::string name;
    std::int64_t size;
    std::chrono::system_clock::time_point updated;
    bool is_directory;
};

/**
 * GCSClient - Wrapper around Google Cloud Storage client
 * 
 * Provides a simplified interface for GCS operations used by the filesystem.
 * Uses dependency injection with IGCSSDKClient to enable proper unit testing.
 */
class GCSClient {
public:
    GCSClient();
    explicit GCSClient(const gcs::Client& client);
    // Constructor for dependency injection (enables mocking in tests)
    explicit GCSClient(std::unique_ptr<IGCSSDKClient> sdk_client);
    virtual ~GCSClient() = default;
    
    // Object operations
    virtual std::optional<ObjectMetadata> getObjectMetadata(
        const std::string& bucket_name,
        const std::string& object_name) const;
    
    virtual std::string readObject(
        const IGCSSDKClient::ReadObjectRequest& request) const;
    
    virtual bool writeObject(
        const std::string& bucket_name,
        const std::string& object_name,
        const std::string& content) const;
    
    virtual bool deleteObject(
        const std::string& bucket_name,
        const std::string& object_name) const;
    
    // List operations
    virtual std::vector<ObjectMetadata> listObjects(
        const std::string& bucket_name,
        const std::string& prefix,
        const std::string& delimiter = "",
        int max_results = 0) const;
    
    // Existence checks
    virtual bool objectExists(
        const std::string& bucket_name,
        const std::string& object_name) const;
    
    virtual bool directoryExists(
        const std::string& bucket_name,
        const std::string& dir_prefix) const;

private:
    std::unique_ptr<IGCSSDKClient> sdk_client_;
};

} // namespace gcscfuse
