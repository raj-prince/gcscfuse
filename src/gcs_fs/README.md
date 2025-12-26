# GCSFS

A FUSE filesystem that provides read-only access to Google Cloud Storage buckets.

## Features

- Read files directly from GCS buckets
- In-memory caching of file contents
- Lists all objects in the bucket as files in root directory
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
./build/gcs_fs <bucket-name> ~/gcs
```

Example:
```bash
./build/gcs_fs my-test-bucket ~/gcs -f
```

Test:
```bash
ls ~/gcs
cat ~/gcs/test.txt
```

Unmount:
```bash
umount ~/gcs
```

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

- [main.cpp](main.cpp) - Entry point with bucket name argument
- [gcs_fs.hpp](gcs_fs.hpp) - GCSFS class definition
- [gcs_fs.cpp](gcs_fs.cpp) - GCS operations and FUSE implementation
- [CMakeLists.txt](CMakeLists.txt) - Build configuration with automatic dependency fetching

## Limitations

- Read-only filesystem
- All files appear in root directory (no subdirectories yet)
- Files are fully cached in memory on first read
