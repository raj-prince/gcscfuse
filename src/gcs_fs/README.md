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

### 4. Set up GCS authentication

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

- `--enable-stat-cache` / `--disable-stat-cache` - Enable/disable metadata caching (default: enabled)
- `--enable-file-cache` / `--disable-file-cache` - Enable/disable file content caching (default: enabled)
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

## Architecture

### Write Operations

Files are written to an in-memory buffer and marked as "dirty". On `flush()` or `release()` (file close), the buffer is uploaded to GCS. This design:
- Minimizes GCS API calls (no write per byte)
- Provides fast write performance
- Ensures data consistency

### Stat Cache

A trie-based cache stores file/directory metadata:
- O(path_depth) lookup complexity
- Automatically updated on create/delete/upload operations
- Can be disabled with `--disable-stat-cache`

### File Content Cache

Optional in-memory cache for file contents:
- Caches files on first read
- Reduces repeated GCS reads
- Can be disabled with `--disable-file-cache`

## Quick Start on New VM

Complete setup from scratch:

```bash
# Install system dependencies
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git libfuse3-dev \
    libcurl4-openssl-dev libssl-dev zlib1g-dev libc-ares-dev libre2-dev

# Clone and build (CMake handles all C++ dependencies)
git clone <your-repo>
cd gcscfuse/src/gcs_fs
mkdir build && cd build
cmake ..
make -j$(nproc)

# Setup authentication
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json

# Mount
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
