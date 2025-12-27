# GCSFS Unit Tests

Unit tests for GCSFS components using Google Test framework.

## Prerequisites

Install Google Test:

```bash
sudo apt update
sudo apt install libgtest-dev cmake
```

## Building Tests

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests/unit
mkdir -p build
cd build
cmake ..
make
```

## Running Tests

### Run all tests:
```bash
./stat_cache_test
```

### Run with verbose output:
```bash
./stat_cache_test --gtest_verbose
```

### Run specific test:
```bash
./stat_cache_test --gtest_filter=StatCacheTest.InsertAndRetrieveFile
```

### Run tests matching a pattern:
```bash
./stat_cache_test --gtest_filter=*TTL*
```

### Using CTest:
```bash
ctest
ctest --verbose
```

## Test Organization

### StatCacheTest
Tests for the StatCache class:

- **Basic Operations**: Insert, retrieve, remove, clear
- **Path Handling**: Nested paths, parent directories, edge cases
- **TTL (Time-To-Live)**: Expiration, timeout configuration
- **Edge Cases**: Empty files, large files, updates, concurrent access
- **Performance**: Many files, deeply nested paths

## Adding New Tests

1. Add test to `stat_cache_test.cpp`:
```cpp
TEST_F(StatCacheTest, YourTestName) {
    // Arrange
    cache->insertFile("/test.txt", 100, time(nullptr));
    
    // Act
    auto result = cache->getStat("/test.txt");
    
    // Assert
    EXPECT_TRUE(result.has_value());
}
```

2. Rebuild and run:
```bash
make
./stat_cache_test
```

## Test Results

Expected output:
```
[==========] Running 30 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 30 tests from StatCacheTest
[ RUN      ] StatCacheTest.InsertAndRetrieveFile
[       OK ] StatCacheTest.InsertAndRetrieveFile (0 ms)
...
[----------] 30 tests from StatCacheTest (1234 ms total)
[==========] 30 tests from 1 test suite ran. (1234 ms total)
[  PASSED  ] 30 tests.
```

## Continuous Integration

These tests should be run automatically on:
- Every commit
- Before merging pull requests
- Nightly builds

## Coverage

To generate coverage report:

```bash
# Install coverage tools
sudo apt install lcov

# Build with coverage flags
cmake -DCMAKE_CXX_FLAGS="--coverage" ..
make
./stat_cache_test

# Generate report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

View `coverage_html/index.html` in a browser.

## Troubleshooting

### Google Test not found
```bash
sudo apt install libgtest-dev
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib
```

### Tests fail to compile
- Check C++17 support: `g++ --version`
- Verify include paths in CMakeLists.txt
- Check that stat_cache.hpp/cpp are accessible

### Tests timeout
- Some tests use `sleep()` for TTL testing
- Total runtime should be < 10 seconds
- Use `--gtest_filter` to run specific tests
