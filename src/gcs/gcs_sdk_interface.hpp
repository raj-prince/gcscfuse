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
    
    // Request struct for ReadObject
    struct ReadObjectRequest {
        std::string bucket_name;
        std::string object_name;
        // Optional range: [start, end) (inclusive, exclusive)
        std::optional<std::pair<std::int64_t, std::int64_t>> range;

        bool operator==(const ReadObjectRequest& other) const {
            return bucket_name == other.bucket_name && object_name == other.object_name && range == other.range;
        }
    };

    // Request struct for GetObjectMetadata
    struct GetObjectMetadataRequest {
        std::string bucket_name;
        std::string object_name;

        bool operator==(const GetObjectMetadataRequest& other) const {
            return bucket_name == other.bucket_name && object_name == other.object_name;
        }
    };

    // Request struct for WriteObject
    struct WriteObjectRequest {
        std::string bucket_name;
        std::string object_name;

        bool operator==(const WriteObjectRequest& other) const {
            return bucket_name == other.bucket_name && object_name == other.object_name;
        }
    };

    // Request struct for DeleteObject
    struct DeleteObjectRequest {
        std::string bucket_name;
        std::string object_name;

        bool operator==(const DeleteObjectRequest& other) const {
            return bucket_name == other.bucket_name && object_name == other.object_name;
        }
    };

    // Request struct for ListObjects
    struct ListObjectsRequest {
        std::string bucket_name;
        std::string prefix;
        std::string delimiter;
        int max_results = 0;

        bool operator==(const ListObjectsRequest& other) const {
            return bucket_name == other.bucket_name && 
                   prefix == other.prefix && 
                   delimiter == other.delimiter && 
                   max_results == other.max_results;
        }
    };

    // Read object - returns SDK's ObjectReadStream
    virtual gcs::ObjectReadStream ReadObject(const ReadObjectRequest& request) const = 0;
    
    // Get object metadata - returns SDK's StatusOr result
    virtual StatusOr<gcs::ObjectMetadata> GetObjectMetadata(const GetObjectMetadataRequest& request) const = 0;
    
    // Write object - returns SDK's ObjectWriteStream
    virtual gcs::ObjectWriteStream WriteObject(const WriteObjectRequest& request) const = 0;
    
    // Delete object - returns SDK's Status
    virtual Status DeleteObject(const DeleteObjectRequest& request) const = 0;
    
    // List objects - returns SDK's ListObjectsReader
    virtual gcs::ListObjectsReader ListObjects(const ListObjectsRequest& request) const = 0;
};

/**
 * Real implementation - thin wrapper over google::cloud::storage::Client
 * Just forwards calls to the SDK with no business logic
 */
class GCSSDKClientImpl : public IGCSSDKClient {
public:
    GCSSDKClientImpl();
    explicit GCSSDKClientImpl(const gcs::Client& client);
    
    gcs::ObjectReadStream ReadObject(const ReadObjectRequest& request) const override;
    
    StatusOr<gcs::ObjectMetadata> GetObjectMetadata(const GetObjectMetadataRequest& request) const override;
    
    gcs::ObjectWriteStream WriteObject(const WriteObjectRequest& request) const override;
    
    Status DeleteObject(const DeleteObjectRequest& request) const override;
    
    gcs::ListObjectsReader ListObjects(const ListObjectsRequest& request) const override;

private:
    mutable gcs::Client client_;
};

} // namespace gcscfuse
