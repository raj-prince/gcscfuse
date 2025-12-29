#include "gcs_client.hpp"
#include <iostream>

namespace gcscfuse {

GCSClient::GCSClient() : sdk_client_(std::make_unique<GCSSDKClientImpl>()) {}

GCSClient::GCSClient(const gcs::Client& client) 
    : sdk_client_(std::make_unique<GCSSDKClientImpl>(client)) {}

GCSClient::GCSClient(std::unique_ptr<IGCSSDKClient> sdk_client) 
    : sdk_client_(std::move(sdk_client)) {}

std::optional<ObjectMetadata> GCSClient::getObjectMetadata(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    try {
        auto metadata = sdk_client_->GetObjectMetadata(bucket_name, object_name);
        if (!metadata) {
            return std::nullopt;
        }
        
        ObjectMetadata obj_meta;
        obj_meta.name = metadata->name();
        obj_meta.size = metadata->size();
        obj_meta.updated = metadata->updated();
        obj_meta.is_directory = false;
        
        return obj_meta;
    } catch (const std::exception& e) {
        std::cerr << "Error getting metadata for " << object_name << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string GCSClient::readObject(const IGCSSDKClient::ReadObjectRequest& request) const {
    auto reader = sdk_client_->ReadObject(request);
    if (!reader) {
        std::cerr << "Error reading object: " << reader.status().message() << std::endl;
        return "";
    }
    std::string content{std::istreambuf_iterator<char>{reader}, {}};
    return content;
}

bool GCSClient::writeObject(
    const std::string& bucket_name,
    const std::string& object_name,
    const std::string& content) const 
{
    try {
        auto writer = sdk_client_->WriteObject(bucket_name, object_name);
        writer << content;
        writer.Close();
        
        if (!writer.metadata()) {
            std::cerr << "Error writing object: " << writer.metadata().status().message() << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error writing object " << object_name << ": " << e.what() << std::endl;
        return false;
    }
}

bool GCSClient::deleteObject(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    auto status = sdk_client_->DeleteObject(bucket_name, object_name);
    if (!status.ok()) {
        std::cerr << "Error deleting object: " << status.message() << std::endl;
        return false;
    }
    return true;
}

std::vector<ObjectMetadata> GCSClient::listObjects(
    const std::string& bucket_name,
    const std::string& prefix,
    const std::string& delimiter,
    int max_results) const 
{
    std::vector<ObjectMetadata> results;
    
    try {
        auto objects = sdk_client_->ListObjects(bucket_name, prefix, delimiter, max_results);
        
        for (auto&& object_metadata : objects) {
            if (!object_metadata) {
                std::cerr << "Error listing objects: " << object_metadata.status().message() << std::endl;
                break;
            }
            
            ObjectMetadata obj_meta;
            obj_meta.name = object_metadata->name();
            obj_meta.size = object_metadata->size();
            obj_meta.updated = object_metadata->updated();
            obj_meta.is_directory = false;
            
            results.push_back(obj_meta);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing objects: " << e.what() << std::endl;
    }
    
    return results;
}

bool GCSClient::objectExists(
    const std::string& bucket_name,
    const std::string& object_name) const 
{
    auto metadata = getObjectMetadata(bucket_name, object_name);
    return metadata.has_value();
}

bool GCSClient::directoryExists(
    const std::string& bucket_name,
    const std::string& dir_prefix) const 
{
    auto objects = sdk_client_->ListObjects(bucket_name, dir_prefix, "", 1);
    
    try {
        auto it = objects.begin();
        return it != objects.end();
    } catch (const std::exception& e) {
        std::cerr << "Error checking directory: " << e.what() << std::endl;
        return false;
    }
}

} // namespace gcscfuse
