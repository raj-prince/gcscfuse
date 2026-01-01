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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

