#include "gcs_sdk_interface.hpp"

namespace gcscfuse {

GCSSDKClientImpl::GCSSDKClientImpl() : client_(gcs::Client()) {}

GCSSDKClientImpl::GCSSDKClientImpl(const gcs::Client& client) : client_(client) {}

gcs::ObjectReadStream GCSSDKClientImpl::ReadObject(const ReadObjectRequest& request) const {
    return client_.ReadObject(request.bucket_name, request.object_name);
}

StatusOr<gcs::ObjectMetadata> GCSSDKClientImpl::GetObjectMetadata(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    return client_.GetObjectMetadata(bucket_name, object_name);
}

gcs::ObjectWriteStream GCSSDKClientImpl::WriteObject(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    return client_.WriteObject(bucket_name, object_name);
}

Status GCSSDKClientImpl::DeleteObject(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    return client_.DeleteObject(bucket_name, object_name);
}

gcs::ListObjectsReader GCSSDKClientImpl::ListObjects(
    const std::string& bucket_name,
    const std::string& prefix,
    const std::string& delimiter,
    int max_results) const 
{
    return client_.ListObjects(
        bucket_name,
        gcs::Prefix(prefix),
        gcs::Delimiter(delimiter),
        gcs::MaxResults(max_results)
    );
}

} // namespace gcscfuse
