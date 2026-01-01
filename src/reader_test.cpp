// Test for reader abstraction

#include <gtest/gtest.h>
#include "../src/reader.hpp"
#include <cstring>

using namespace gcscfuse;

TEST(ReaderTest, DummyReaderBasicRead) {
    DummyReader reader;
    reader.setMockData("test.txt", "Hello, World!");
    
    char buf[100];
    int bytes_read = reader.read("test.txt", buf, 13, 0);
    
    EXPECT_EQ(bytes_read, 13);
    EXPECT_EQ(std::string(buf, bytes_read), "Hello, World!");
}

TEST(ReaderTest, DummyReaderPartialRead) {
    DummyReader reader;
    reader.setMockData("test.txt", "Hello, World!");
    
    char buf[100];
    int bytes_read = reader.read("test.txt", buf, 5, 0);
    
    EXPECT_EQ(bytes_read, 5);
    EXPECT_EQ(std::string(buf, bytes_read), "Hello");
}

TEST(ReaderTest, DummyReaderOffsetRead) {
    DummyReader reader;
    reader.setMockData("test.txt", "Hello, World!");
    
    char buf[100];
    int bytes_read = reader.read("test.txt", buf, 5, 7);
    
    EXPECT_EQ(bytes_read, 5);
    EXPECT_EQ(std::string(buf, bytes_read), "World");
}

TEST(ReaderTest, DummyReaderNonExistent) {
    DummyReader reader;
    
    char buf[100];
    int bytes_read = reader.read("nonexistent.txt", buf, 10, 0);
    
    EXPECT_EQ(bytes_read, -1);
}

TEST(ReaderTest, DummyReaderOffsetBeyondEnd) {
    DummyReader reader;
    reader.setMockData("test.txt", "Hello");
    
    char buf[100];
    int bytes_read = reader.read("test.txt", buf, 10, 100);
    
    EXPECT_EQ(bytes_read, 0);
}

TEST(ReaderTest, DummyReaderClear) {
    DummyReader reader;
    reader.setMockData("test.txt", "Hello, World!");
    
    reader.clear();
    
    char buf[100];
    int bytes_read = reader.read("test.txt", buf, 10, 0);
    
    EXPECT_EQ(bytes_read, -1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
