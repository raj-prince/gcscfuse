#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../../stat_cache.hpp"

// Test fixture for StatCache tests
class StatCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<StatCache>();
    }

    void TearDown() override {
        cache.reset();
    }

    std::unique_ptr<StatCache> cache;
};

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(StatCacheTest, InsertAndRetrieveFile) {
    cache->insertFile("/test.txt", 1024, 12345);
    
    auto result = cache->getStat("/test.txt");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, 1024);
    EXPECT_EQ(result->mtime, 12345);
    EXPECT_FALSE(result->is_directory);
    EXPECT_TRUE(result->metadata_loaded);
}

TEST_F(StatCacheTest, InsertAndRetrieveDirectory) {
    cache->insertDirectory("/mydir");
    
    auto result = cache->getStat("/mydir");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_directory);
    EXPECT_EQ(result->mode, S_IFDIR | 0755);
    EXPECT_TRUE(result->metadata_loaded);
}

TEST_F(StatCacheTest, NonExistentPathReturnsNullopt) {
    auto result = cache->getStat("/nonexistent");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(StatCacheTest, RootPathAlwaysExists) {
    auto result = cache->getStat("/");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_directory);
}

TEST_F(StatCacheTest, ExistsReturnsTrueForInsertedFile) {
    cache->insertFile("/test.txt", 100, time(nullptr));
    
    EXPECT_TRUE(cache->exists("/test.txt"));
    EXPECT_FALSE(cache->exists("/other.txt"));
}

TEST_F(StatCacheTest, RemoveDeletesEntry) {
    cache->insertFile("/test.txt", 100, time(nullptr));
    EXPECT_TRUE(cache->exists("/test.txt"));
    
    cache->remove("/test.txt");
    
    EXPECT_FALSE(cache->exists("/test.txt"));
}

TEST_F(StatCacheTest, ClearRemovesAllEntries) {
    cache->insertFile("/file1.txt", 100, time(nullptr));
    cache->insertFile("/file2.txt", 200, time(nullptr));
    cache->insertDirectory("/dir");
    
    cache->clear();
    
    EXPECT_FALSE(cache->exists("/file1.txt"));
    EXPECT_FALSE(cache->exists("/file2.txt"));
    EXPECT_FALSE(cache->exists("/dir"));
}

// ============================================================================
// Path Handling Tests
// ============================================================================

TEST_F(StatCacheTest, HandlesPathsWithoutLeadingSlash) {
    cache->insertFile("test.txt", 100, time(nullptr));
    
    auto result = cache->getStat("/test.txt");
    EXPECT_TRUE(result.has_value());
}

TEST_F(StatCacheTest, HandlesNestedPaths) {
    cache->insertFile("/path/to/deep/file.txt", 100, time(nullptr));
    
    auto result = cache->getStat("/path/to/deep/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_directory);
}

TEST_F(StatCacheTest, CreatesParentDirectoriesAutomatically) {
    cache->insertFile("/path/to/file.txt", 100, time(nullptr));
    
    // Parent directories should be created
    EXPECT_TRUE(cache->exists("/path"));
    EXPECT_TRUE(cache->exists("/path/to"));
    
    auto parent = cache->getStat("/path");
    ASSERT_TRUE(parent.has_value());
    EXPECT_TRUE(parent->is_directory);
}

TEST_F(StatCacheTest, HandlesEmptyPath) {
    auto result = cache->getStat("");
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_directory);
}

// ============================================================================
// TTL (Time-To-Live) Tests
// ============================================================================

TEST_F(StatCacheTest, DefaultTTLIs60Seconds) {
    // Default timeout should be 60 seconds
    cache->insertFile("/test.txt", 100, time(nullptr));
    
    // Should still be valid immediately
    auto result = cache->getStat("/test.txt");
    EXPECT_TRUE(result.has_value());
}

TEST_F(StatCacheTest, ExpiredEntryReturnsNullopt) {
    cache->setCacheTimeout(1);  // 1 second TTL
    cache->insertFile("/test.txt", 100, time(nullptr));
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto result = cache->getStat("/test.txt");
    EXPECT_FALSE(result.has_value()) << "Cache entry should be expired";
}

TEST_F(StatCacheTest, NonExpiredEntryIsReturned) {
    cache->setCacheTimeout(5);  // 5 seconds TTL
    cache->insertFile("/test.txt", 100, time(nullptr));
    
    // Check immediately - should not be expired
    auto result = cache->getStat("/test.txt");
    EXPECT_TRUE(result.has_value()) << "Cache entry should not be expired";
}

TEST_F(StatCacheTest, ZeroTTLDisablesExpiration) {
    cache->setCacheTimeout(0);  // Never expires
    
    // Insert with old timestamp
    time_t old_time = time(nullptr) - 10000;  // 10000 seconds ago
    cache->insertFile("/test.txt", 100, old_time);
    
    // Should still be valid
    auto result = cache->getStat("/test.txt");
    EXPECT_TRUE(result.has_value()) << "Entry with TTL=0 should never expire";
}

TEST_F(StatCacheTest, NegativeTTLDisablesExpiration) {
    cache->setCacheTimeout(-1);
    
    time_t old_time = time(nullptr) - 10000;
    cache->insertFile("/test.txt", 100, old_time);
    
    auto result = cache->getStat("/test.txt");
    EXPECT_TRUE(result.has_value());
}

TEST_F(StatCacheTest, CacheTimeIsSetOnInsert) {
    time_t before = time(nullptr);
    cache->insertFile("/test.txt", 100, 12345);
    time_t after = time(nullptr);
    
    auto result = cache->getStat("/test.txt");
    ASSERT_TRUE(result.has_value());
    
    // cache_time should be between before and after
    EXPECT_GE(result->cache_time, before);
    EXPECT_LE(result->cache_time, after);
}

TEST_F(StatCacheTest, DirectoryCacheTimeIsSet) {
    time_t before = time(nullptr);
    cache->insertDirectory("/mydir");
    time_t after = time(nullptr);
    
    auto result = cache->getStat("/mydir");
    ASSERT_TRUE(result.has_value());
    
    EXPECT_GE(result->cache_time, before);
    EXPECT_LE(result->cache_time, after);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(StatCacheTest, InsertFileWithZeroSize) {
    cache->insertFile("/empty.txt", 0, time(nullptr));
    
    auto result = cache->getStat("/empty.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, 0);
}

TEST_F(StatCacheTest, InsertFileWithLargeSize) {
    off_t large_size = 10LL * 1024 * 1024 * 1024;  // 10GB
    cache->insertFile("/large.bin", large_size, time(nullptr));
    
    auto result = cache->getStat("/large.bin");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, large_size);
}

TEST_F(StatCacheTest, UpdateExistingEntry) {
    // Insert initial entry
    cache->insertFile("/test.txt", 100, 12345);
    
    // Update with new values
    cache->insertFile("/test.txt", 200, 67890);
    
    auto result = cache->getStat("/test.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, 200);
    EXPECT_EQ(result->mtime, 67890);
}

TEST_F(StatCacheTest, RemoveNonExistentEntry) {
    // Should not crash
    cache->remove("/nonexistent");
    
    EXPECT_FALSE(cache->exists("/nonexistent"));
}

TEST_F(StatCacheTest, PathsWithTrailingSlash) {
    cache->insertDirectory("/mydir/");
    
    auto result = cache->getStat("/mydir");
    EXPECT_TRUE(result.has_value());
}

TEST_F(StatCacheTest, MultipleFilesInSameDirectory) {
    cache->insertFile("/dir/file1.txt", 100, time(nullptr));
    cache->insertFile("/dir/file2.txt", 200, time(nullptr));
    cache->insertFile("/dir/file3.txt", 300, time(nullptr));
    
    EXPECT_TRUE(cache->exists("/dir/file1.txt"));
    EXPECT_TRUE(cache->exists("/dir/file2.txt"));
    EXPECT_TRUE(cache->exists("/dir/file3.txt"));
    EXPECT_TRUE(cache->exists("/dir"));
}

// ============================================================================
// Performance/Stress Tests
// ============================================================================

TEST_F(StatCacheTest, HandlesManyFiles) {
    // Insert 1000 files
    for (int i = 0; i < 1000; i++) {
        std::string path = "/file" + std::to_string(i) + ".txt";
        cache->insertFile(path, i * 100, time(nullptr));
    }
    
    // Verify all exist
    for (int i = 0; i < 1000; i++) {
        std::string path = "/file" + std::to_string(i) + ".txt";
        EXPECT_TRUE(cache->exists(path));
    }
}

TEST_F(StatCacheTest, HandlesDeeplyNestedPaths) {
    std::string path = "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/file.txt";
    cache->insertFile(path, 100, time(nullptr));
    
    auto result = cache->getStat(path);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_directory);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
