// Test for reader abstraction

#include <gtest/gtest.h>
#include "../src/reader.hpp"
#include <cstring>

using namespace gcscfuse;

TEST(ReaderTest, DummyReaderBasicRead) {
    DummyReader reader;
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf)); // Fill with non-zero
    int bytes_read = reader.read("test.txt", buf, 13, 0);
    
    EXPECT_EQ(bytes_read, 13);
    // Verify all bytes are zero
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, DummyReaderPartialRead) {
    DummyReader reader;
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = reader.read("test.txt", buf, 5, 0);
    
    EXPECT_EQ(bytes_read, 5);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, DummyReaderOffsetRead) {
    DummyReader reader;
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = reader.read("test.txt", buf, 5, 7);
    
    EXPECT_EQ(bytes_read, 5);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, DummyReaderAlwaysReturnsData) {
    DummyReader reader;
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = reader.read("any-file.txt", buf, 10, 0);
    
    EXPECT_EQ(bytes_read, 10);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, DummyReaderLargeRead) {
    DummyReader reader;
    
    char buf[1000];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = reader.read("test.txt", buf, 1000, 0);
    
    EXPECT_EQ(bytes_read, 1000);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, DummyReaderClear) {
    DummyReader reader;
    
    reader.clear(); // Should be no-op
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = reader.read("test.txt", buf, 10, 0);
    
    EXPECT_EQ(bytes_read, 10);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

// ==================== CachedReader Tests ====================

TEST(ReaderTest, CachedReaderBasicRead) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = cached_reader.read("test.txt", buf, 50, 0);
    
    EXPECT_EQ(bytes_read, 50);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, CachedReaderCachesContent) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy), false, true);
    
    // First read - should cache the full object
    char buf1[50];
    std::memset(buf1, 0xFF, sizeof(buf1));
    int bytes_read1 = cached_reader.read("file.txt", buf1, 50, 0);
    EXPECT_EQ(bytes_read1, 50);
    
    // Second read from offset - should come from cache
    char buf2[30];
    std::memset(buf2, 0xFF, sizeof(buf2));
    int bytes_read2 = cached_reader.read("file.txt", buf2, 30, 20);
    EXPECT_EQ(bytes_read2, 30);
    
    // Verify data is zeros
    for (int i = 0; i < bytes_read2; i++) {
        EXPECT_EQ(buf2[i], 0);
    }
}

TEST(ReaderTest, CachedReaderPartialRead) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    // Read small chunk
    char buf[20];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = cached_reader.read("test.txt", buf, 20, 0);
    
    EXPECT_EQ(bytes_read, 20);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, CachedReaderOffsetRead) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    // Read from middle of file
    char buf[100];
    std::memset(buf, 0xFF, sizeof(buf));
    int bytes_read = cached_reader.read("test.txt", buf, 100, 500);
    
    EXPECT_EQ(bytes_read, 100);
    for (int i = 0; i < bytes_read; i++) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(ReaderTest, CachedReaderInvalidate) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    // First read
    char buf1[50];
    int bytes_read1 = cached_reader.read("file.txt", buf1, 50, 0);
    EXPECT_EQ(bytes_read1, 50);
    
    // Invalidate cache
    cached_reader.invalidate("file.txt");
    
    // Read again - should re-cache
    char buf2[50];
    int bytes_read2 = cached_reader.read("file.txt", buf2, 50, 0);
    EXPECT_EQ(bytes_read2, 50);
}

TEST(ReaderTest, CachedReaderClear) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    // Read and cache multiple files
    char buf1[50];
    cached_reader.read("file1.txt", buf1, 50, 0);
    
    char buf2[50];
    cached_reader.read("file2.txt", buf2, 50, 0);
    
    // Clear all cache
    cached_reader.clear();
    
    // Read again - should re-cache
    char buf3[50];
    int bytes_read = cached_reader.read("file1.txt", buf3, 50, 0);
    EXPECT_EQ(bytes_read, 50);
}

TEST(ReaderTest, CachedReaderMultipleFiles) {
    auto dummy = std::make_unique<DummyReader>();
    CachedReader cached_reader(std::move(dummy));
    
    // Read different files
    char buf1[30];
    int bytes1 = cached_reader.read("file1.txt", buf1, 30, 0);
    EXPECT_EQ(bytes1, 30);
    
    char buf2[40];
    int bytes2 = cached_reader.read("file2.txt", buf2, 40, 0);
    EXPECT_EQ(bytes2, 40);
    
    char buf3[25];
    int bytes3 = cached_reader.read("file3.txt", buf3, 25, 0);
    EXPECT_EQ(bytes3, 25);
    
    // Read file1 again - should be cached
    char buf4[30];
    int bytes4 = cached_reader.read("file1.txt", buf4, 30, 0);
    EXPECT_EQ(bytes4, 30);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

