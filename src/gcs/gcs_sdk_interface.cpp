#include "gcs_sdk_interface.hpp"
#include "google/cloud/options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include <iostream>

namespace gcscfuse {

GCSSDKClientImpl::GCSSDKClientImpl() : client_(gcs::Client()) {}

GCSSDKClientImpl::GCSSDKClientImpl(const gcs::Client& client) : client_(client) {}

GCSSDKClientImpl::GCSSDKClientImpl(const std::string& protocol) {
    if (protocol == "grpc") {
        auto options = google::cloud::Options{};
        client_ = gcs::MakeGrpcClient(options);
        std::cout << "[INFO] gRPC client created successfully" << std::endl;
        std::cout << "[INFO] Using gRPC transport for Google Cloud Storage" << std::endl;
    } else if (protocol == "json") {
        // Use default JSON/REST protocol with standard settings
        client_ = gcs::Client();
        std::cout << "[INFO] Using JSON/REST protocol" << std::endl;
    } else {
        throw std::runtime_error("Invalid protocol: " + protocol + ". Must be 'json' or 'grpc'");
    }
}

gcs::ObjectReadStream GCSSDKClientImpl::ReadObject(const ReadObjectRequest& request) const {
    return client_.ReadObject(request.bucket_name, request.object_name);
}

StatusOr<gcs::ObjectMetadata> GCSSDKClientImpl::GetObjectMetadata(const GetObjectMetadataRequest& request) const {
    return client_.GetObjectMetadata(request.bucket_name, request.object_name);
}

gcs::ObjectWriteStream GCSSDKClientImpl::WriteObject(const WriteObjectRequest& request) const {
    return client_.WriteObject(request.bucket_name, request.object_name);
}

Status GCSSDKClientImpl::DeleteObject(const DeleteObjectRequest& request) const {
    return client_.DeleteObject(request.bucket_name, request.object_name);
}

gcs::ListObjectsReader GCSSDKClientImpl::ListObjects(const ListObjectsRequest& request) const {
    return client_.ListObjects(
        request.bucket_name,
        gcs::Prefix(request.prefix),
        gcs::Delimiter(request.delimiter),
        gcs::MaxResults(request.max_results)
    );
}

} // namespace gcscfuse
