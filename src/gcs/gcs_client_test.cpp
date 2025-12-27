#include "gcs_client.hpp"
#include "gcs_sdk_interface.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <sstream>

using ::testing::Return;
using ::testing::_;
using ::testing::Invoke;
namespace gcs = ::google::cloud::storage;

// Mock the raw GCS SDK interface
class MockGCSSDKClient : public gcscfuse::IGCSSDKClient {
public:
    MOCK_METHOD(
        gcs::ObjectReadStream,
        ReadObject,
        (const std::string& bucket_name, const std::string& object_name),
        (const, override)
    );
    
    MOCK_METHOD(
        (google::cloud::StatusOr<gcs::ObjectMetadata>),
        GetObjectMetadata,
        (const std::string& bucket_name, const std::string& object_name),
        (const, override)
    );
    
    MOCK_METHOD(
        gcs::ObjectWriteStream,
        WriteObject,
        (const std::string& bucket_name, const std::string& object_name),
        (const, override)
    );
    
    MOCK_METHOD(
        google::cloud::Status,
        DeleteObject,
        (const std::string& bucket_name, const std::string& object_name),
        (const, override)
    );
    
    MOCK_METHOD(
        gcs::ListObjectsReader,
        ListObjects,
        (const std::string& bucket_name, const std::string& prefix, 
         const std::string& delimiter, int max_results),
        (const, override)
    );
};

class GCSClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sdk_client = std::make_unique<MockGCSSDKClient>();
        mock_sdk_client_ptr = mock_sdk_client.get();
    }
    
    std::unique_ptr<MockGCSSDKClient> mock_sdk_client;
    MockGCSSDKClient* mock_sdk_client_ptr;
    
    // Helper to create mock ObjectMetadata using SDK builder
    static gcs::ObjectMetadata createMockMetadata(const std::string& name, std::int64_t size) {
        return gcs::ObjectMetadata{}
            .set_name(name)
            .set_size(size);
    }
};

// Test getObjectMetadata - Success case (tests transformation logic)
TEST_F(GCSClientTest, GetObjectMetadata_Success) {
    const std::string bucket = "test-bucket";
    const std::string object = "test-object.txt";
    
    auto mock_metadata = createMockMetadata(object, 12345);
    
    EXPECT_CALL(*mock_sdk_client_ptr, GetObjectMetadata(bucket, object))
        .WillOnce(Return(mock_metadata));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    auto result = client.getObjectMetadata(bucket, object);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, object);
    EXPECT_EQ(result->size, 12345);
    EXPECT_FALSE(result->is_directory);
}

// Test getObjectMetadata - Not found (tests error handling)
TEST_F(GCSClientTest, GetObjectMetadata_NotFound) {
    const std::string bucket = "test-bucket";
    const std::string object = "non-existent.txt";
    
    google::cloud::Status not_found_status(google::cloud::StatusCode::kNotFound, "Object not found");
    
    EXPECT_CALL(*mock_sdk_client_ptr, GetObjectMetadata(bucket, object))
        .WillOnce(Return(not_found_status));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    auto result = client.getObjectMetadata(bucket, object);
    
    EXPECT_FALSE(result.has_value());
}

// Test readObject - Error handling (tests error handling for failed streams)
TEST_F(GCSClientTest, ReadObject_ErrorHandling) {
    const std::string bucket = "test-bucket";
    const std::string object = "test-object.txt";
    
    // Test that GCSClient correctly handles error streams from the SDK
    // The actual stream reading is SDK responsibility and tested by Google
    // We test that our error handling logic works correctly
    
    EXPECT_CALL(*mock_sdk_client_ptr, ReadObject(bucket, object))
        .WillOnce(Invoke([](const std::string&, const std::string&) {
            // Return a default-constructed stream which is invalid
            // A default ObjectReadStream has no buffer and will fail status checks
            return gcs::ObjectReadStream();
        }));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    std::string result = client.readObject(bucket, object);
    
    // Should return empty string on error
    EXPECT_EQ(result, "");
}


// Test objectExists - Tests logic that uses getObjectMetadata
TEST_F(GCSClientTest, ObjectExists_True) {
    const std::string bucket = "test-bucket";
    const std::string object = "existing-file.txt";
    
    auto mock_metadata = createMockMetadata(object, 100);
    
    EXPECT_CALL(*mock_sdk_client_ptr, GetObjectMetadata(bucket, object))
        .WillOnce(Return(mock_metadata));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    bool exists = client.objectExists(bucket, object);
    
    EXPECT_TRUE(exists);
}

// Test objectExists - Object does not exist
TEST_F(GCSClientTest, ObjectExists_False) {
    const std::string bucket = "test-bucket";
    const std::string object = "non-existent-file.txt";
    
    google::cloud::Status not_found(google::cloud::StatusCode::kNotFound, "Not found");
    
    EXPECT_CALL(*mock_sdk_client_ptr, GetObjectMetadata(bucket, object))
        .WillOnce(Return(not_found));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    bool exists = client.objectExists(bucket, object);
    
    EXPECT_FALSE(exists);
}

// Test deleteObject - Success (tests status handling)
TEST_F(GCSClientTest, DeleteObject_Success) {
    const std::string bucket = "test-bucket";
    const std::string object = "file-to-delete.txt";
    
    EXPECT_CALL(*mock_sdk_client_ptr, DeleteObject(bucket, object))
        .WillOnce(Return(google::cloud::Status()));  // OK status
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    bool result = client.deleteObject(bucket, object);
    
    EXPECT_TRUE(result);
}

// Test deleteObject - Failure (tests error handling)
TEST_F(GCSClientTest, DeleteObject_Failure) {
    const std::string bucket = "test-bucket";
    const std::string object = "protected-file.txt";
    
    google::cloud::Status error_status(google::cloud::StatusCode::kPermissionDenied, "Permission denied");
    
    EXPECT_CALL(*mock_sdk_client_ptr, DeleteObject(bucket, object))
        .WillOnce(Return(error_status));
    
    gcscfuse::GCSClient client(std::move(mock_sdk_client));
    bool result = client.deleteObject(bucket, object);
    
    EXPECT_FALSE(result);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
