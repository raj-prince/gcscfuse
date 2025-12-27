# GCSFS

A FUSE filesystem that provides full read-write access to Google Cloud Storage buckets.

## Features

- **Full read-write support**: Create, modify, and delete files
- **Directory support**: Navigate nested directory structures
- **Trie-based stat cache**: Fast metadata lookups with O(path_depth) complexity
- **Write buffering**: In-memory buffering with lazy GCS uploads on flush/close
- **Configurable caching**: Optional stat cache and file content cache
- **Command-line flags**: Debug mode, verbose logging, cache control
- **Uses vcpkg for dependency management**

## Prerequisites

### 1. Install system dependencies

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libfuse3-dev curl zip unzip tar
```

### 2. Install vcpkg (one-time setup)

```bash
git clone https://github.com/microsoft/vcpkg $HOME/vcpkg
$HOME/vcpkg/bootstrap-vcpkg.sh
```

### 3. Install google-cloud-cpp via vcpkg

This downloads and builds all C++ dependencies (Abseil, Protobuf, gRPC, CRC32C, google-cloud-cpp).  
**Note:** First time takes 15-20 minutes. Dependencies are cached for future builds.

```bash
$HOME/vcpkg/vcpkg install google-cloud-cpp[core,storage]
```

### 4. Install Google Test (for unit tests)

Required only if you want to run unit tests:

```bash
sudo apt-get install -y libgtest-dev
```

### 5. Install fio (for performance testing)

Required only if you want to run performance benchmarks:

```bash
sudo apt-get install -y fio jq bc
```

### 6. Set up GCS authentication

```bash
# Using service account
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/service-account-key.json

# Or using gcloud
gcloud auth application-default login
```

## Build

```bash
cd /path/to/gcscfuse/src/gcs_fs
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
```

## Run

```bash
mkdir -p ~/gcs
./build/gcs_fs <bucket-name> ~/gcs [options]
```

### Command-line Options

```bash
# Basic usage
./build/gcs_fs my-bucket ~/gcs -f

# With debug output
./build/gcs_fs my-bucket ~/gcs --debug -f

# Disable stat cache
./build/gcs_fs my-bucket ~/gcs --disable-stat-cache -f

# Disable file content cache
./build/gcs_fs my-bucket ~/gcs --disable-file-cache -f

# Show help
./build/gcs_fs --help
```

### Available Flags

- `--disable-stat-cache` - Disable metadata caching (enabled by default)
- `--stat-cache-ttl=N` - Stat cache timeout in seconds (default: 60, 0=no timeout)
- `--disable-file-cache` - Disable file content caching (enabled by default)
- `--debug` - Show debug messages including cache hits and GCS operations
- `--verbose` - Show verbose logging
- `--help` - Display usage information
- `-f` - Run in foreground (FUSE option)
- `-d` - FUSE debug mode
- `-o <option>` - Pass options to FUSE

### Testing Write Operations

```bash
# Create a file
echo "Hello GCS!" > ~/gcs/test.txt

# Append to file
echo "More data" >> ~/gcs/test.txt

# Read file
cat ~/gcs/test.txt

# Modify file
truncate -s 5 ~/gcs/test.txt

# Delete file
rm ~/gcs/test.txt

# Navigate directories
ls ~/gcs/some/nested/path/
cd ~/gcs/some/nested/path/
```

Unmount:
```bash
fusermount -u ~/gcs
# or
umount ~/gcs
```

## Testing

### Unit Tests

Fast, isolated tests for individual components (no GCS bucket required).

**Install test dependencies:**
```bash
sudo apt-get install -y libgtest-dev cmake
```

**Build and run:**
```bash
cd tests/unit
mkdir -p build && cd build
cmake ..
make
./stat_cache_test
```

**Run specific tests:**
```bash
# Run TTL-related tests only
./stat_cache_test --gtest_filter=*TTL*

# Run with verbose output
./stat_cache_test --gtest_verbose
```

**Test coverage:**
- 26 tests for StatCache class
- Basic operations, TTL expiration, path handling, edge cases
- Runs in ~2 seconds

See [tests/unit/README.md](tests/unit/README.md) for details.

### Integration Tests

Tests with actual GCS bucket and mounted filesystem.

**Run integration tests:**
```bash
cd tests
python3 test_cache_ttl.py <bucket-name>

# Or benchmark cache performance
./benchmark_cache.sh <bucket-name>
```

See [tests/TESTING_CACHE_TTL.md](tests/TESTING_CACHE_TTL.md) for details.

### Performance Tests (FIO)

Benchmark filesystem performance using FIO (Flexible I/O tester).

**Install FIO:**
```bash
sudo apt-get install -y fio jq bc
```

**Run comprehensive benchmark:**
```bash
cd tests
./fio_benchmark.sh <bucket-name>
```

**Compare cache enabled vs disabled:**
```bash
./fio_compare.sh <bucket-name>
```

**Tests performed:**
- Sequential read/write (1MB blocks)
- Random read/write (4K blocks)
- Mixed workload (70% read, 30% write)
- Metadata operations (stat)

**Expected results:**
- Sequential ops: ~10-50 MB/s (network dependent)
- Random ops with cache: Significantly faster
- Metadata ops with cache: 100x-1000x faster

Results are saved as JSON files with detailed metrics (bandwidth, IOPS, latency).

## Architecture

### Write Operations

Files are written to an in-memory buffer and marked as "dirty". On `flush()` or `release()` (file close), the buffer is uploaded to GCS. This design:
- Minimizes GCS API calls (no write per byte)
- Provides fast write performance
- Ensures data consistency

### Stat Cache

A trie-based cache stores file/directory metadata with configurable TTL (Time-To-Live):
- O(path_depth) lookup complexity
- Lazy expiration - entries checked on access
- Automatically updated on create/delete/upload operations
- Default 60-second TTL (configurable with `--stat-cache-ttl`)
- Can be disabled with `--disable-stat-cache`

**Cache performance:**
- Cache hits: ~0.0001s (instant)
- Cache misses: ~0.05-0.2s (GCS fetch)
- Provides 100x-1000x speedup for repeated operations

### File Content Cache

Optional in-memory cache for file contents:
- Caches files on first read
- Reduces repeated GCS reads
- Can be disabled with `--disable-file-cache`

## Quick Start on New VM

Complete setup from scratch:

```bash
# Install system dependencies (including Google Test for unit tests)
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git libfuse3-dev \
    libcurl4-openssl-dev libssl-dev zlib1g-dev libc-ares-dev libre2-dev \
    libgtest-dev fio jq bc

# Clone and build (CMake handles all C++ dependencies)
git clone <your-repo>
cd gcscfuse/src/gcs_fs
mkdir build && cd build
cmake ..
make -j$(nproc)

# Setup authentication
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json

# Run unit tests
cd ../tests/unit && mkdir -p build && cd build
cmake .. && make
./stat_cache_test

# Mount filesystem
cd ../../build
mkdir -p ~/gcs
./gcs_fs my-bucket ~/gcs
```

## Implementation

- [main.cpp](main.cpp) - Entry point with command-line flag parsing
- [gcs_fs.hpp](gcs_fs.hpp) / [gcs_fs.cpp](gcs_fs.cpp) - GCSFS class with read/write operations
- [config.hpp](config.hpp) / [config.cpp](config.cpp) - Configuration structure and flag parsing
- [stat_cache.hpp](stat_cache.hpp) / [stat_cache.cpp](stat_cache.cpp) - Trie-based metadata cache
- [CMakeLists.txt](CMakeLists.txt) - Build configuration with vcpkg integration

## FUSE Operations Implemented

### Read Operations
- `getattr()` - Get file/directory attributes
- `readdir()` - List directory contents
- `open()` - Open file for reading/writing
- `read()` - Read file contents

### Write Operations
- `create()` - Create new files
- `write()` - Write data to files
- `truncate()` - Resize files
- `flush()` - Sync dirty data to GCS
- `release()` - Close file and ensure sync
- `unlink()` - Delete files

## Performance Notes

- **Stat cache**: Reduces GCS metadata API calls significantly
- **File cache**: Eliminates redundant content downloads
- **Write buffering**: Minimizes GCS uploads (batches writes)
- **Directory support**: Efficient prefix-based navigation

## Limitations

- Files are fully cached in memory during write operations
- Large file writes limited by available memory
- No directory creation/deletion operations yet
- No symlink support
