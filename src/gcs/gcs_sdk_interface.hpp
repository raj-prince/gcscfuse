#pragma once

#include <string>
#include <optional>
#include <chrono>
#include "google/cloud/storage/client.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"

namespace gcs = ::google::cloud::storage;
using google::cloud::Status;
using google::cloud::StatusOr;

namespace gcscfuse {

/**
 * Raw interface wrapper for GCS SDK - minimal logic, just exposes SDK types
 * This allows mocking the SDK in tests while GCSClient contains the business logic
 */
class IGCSSDKClient {
public:
    virtual ~IGCSSDKClient() = default;
    
    // Read object - returns SDK's ObjectReadStream
    virtual gcs::ObjectReadStream ReadObject(
        const std::string& bucket_name,
        const std::string& object_name) const = 0;
    
    // Get object metadata - returns SDK's StatusOr result
    virtual StatusOr<gcs::ObjectMetadata> GetObjectMetadata(
        const std::string& bucket_name,
        const std::string& object_name) const = 0;
    
    // Write object - returns SDK's ObjectWriteStream
    virtual gcs::ObjectWriteStream WriteObject(
        const std::string& bucket_name,
        const std::string& object_name) const = 0;
    
    // Delete object - returns SDK's Status
    virtual Status DeleteObject(
        const std::string& bucket_name,
        const std::string& object_name) const = 0;
    
    // List objects - returns SDK's ListObjectsReader
    virtual gcs::ListObjectsReader ListObjects(
        const std::string& bucket_name,
        const std::string& prefix,
        const std::string& delimiter,
        int max_results) const = 0;
};

/**
 * Real implementation - thin wrapper over google::cloud::storage::Client
 * Just forwards calls to the SDK with no business logic
 */
class GCSSDKClientImpl : public IGCSSDKClient {
public:
    GCSSDKClientImpl();
    explicit GCSSDKClientImpl(const gcs::Client& client);
    
    gcs::ObjectReadStream ReadObject(
        const std::string& bucket_name,
        const std::string& object_name) const override;
    
    StatusOr<gcs::ObjectMetadata> GetObjectMetadata(
        const std::string& bucket_name,
        const std::string& object_name) const override;
    
    gcs::ObjectWriteStream WriteObject(
        const std::string& bucket_name,
        const std::string& object_name) const override;
    
    Status DeleteObject(
        const std::string& bucket_name,
        const std::string& object_name) const override;
    
    gcs::ListObjectsReader ListObjects(
        const std::string& bucket_name,
        const std::string& prefix,
        const std::string& delimiter,
        int max_results) const override;

private:
    mutable gcs::Client client_;
};

} // namespace gcscfuse
