#include "gcs_sdk_interface.hpp"

namespace gcscfuse {

GCSSDKClientImpl::GCSSDKClientImpl() : client_(gcs::Client()) {}

GCSSDKClientImpl::GCSSDKClientImpl(const gcs::Client& client) : client_(client) {}

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
    // When both prefix and delimiter are empty, list all objects in bucket
    if (request.prefix.empty() && request.delimiter.empty()) {
        return client_.ListObjects(request.bucket_name);
    }
    
    // When only prefix is empty but delimiter is set
    if (request.prefix.empty()) {
        return client_.ListObjects(
            request.bucket_name,
            gcs::Delimiter(request.delimiter),
            gcs::MaxResults(request.max_results)
        );
    }
    
    // When both prefix and delimiter are set
    return client_.ListObjects(
        request.bucket_name,
        gcs::Prefix(request.prefix),
        gcs::Delimiter(request.delimiter),
        gcs::MaxResults(request.max_results)
    );
}

} // namespace gcscfuse
