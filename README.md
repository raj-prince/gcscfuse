# gcscfuse

C++ implementation of FUSE filesystem for Google Cloud Storage with lazy loading, intelligent caching, and high-performance I/O.

## Features

- **Lazy Loading**: On-demand per-directory listing instead of upfront bucket scanning
- **Stat Cache**: TTL-based metadata caching with configurable timeout (default: 60s)
- **File Content Cache**: In-memory file content caching for improved read performance
- **GCS Integration**: Full read-write access to Google Cloud Storage buckets

## Prerequisites

1. Install system dependencies:
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libfuse3-dev libgtest-dev curl zip unzip tar \
    fio jq bc
```

2. Install vcpkg (C++ package manager):
```bash
git clone https://github.com/microsoft/vcpkg $HOME/vcpkg
$HOME/vcpkg/bootstrap-vcpkg.sh
```

3. Install google-cloud-cpp (first time takes 15-20 minutes):
```bash
$HOME/vcpkg/vcpkg install google-cloud-cpp[core,storage]
```

## Build

```bash
cd /path/to/gcscfuse
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make build
```

## Run Unit Tests

```bash
cd build
make test
```

## Mount Filesystem

```bash
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/service-account-key.json
mkdir -p ~/gcs
./build/gcscfuse <bucket-name> ~/gcs
```

Unmount:
```bash
umount ~/gcs
```

## Command-Line Options

```bash
./build/gcscfuse [options] <bucket-name> <mount-point>

Options:
  --disable-stat-cache     Disable metadata cache
  --stat-cache-ttl=N       Cache timeout in seconds (default: 60, 0=no timeout)
  --disable-file-cache     Disable file content cache
  --debug                  Enable debug output
  --verbose                Enable verbose logging
  -d                       FUSE debug mode (foreground)
  -f                       FUSE foreground mode
```

## Testing

### Unit Tests
Fast component tests using Google Test:
```bash
cd build && make test
```

### E2E Tests
Cache TTL verification with real GCS:
```bash
cd build && make e2e
```

### Performance Tests
I/O benchmarking with FIO:
```bash
cd build && make perf
```

For detailed testing information, see:
- [doc/UNIT_TEST_SETUP.md](doc/UNIT_TEST_SETUP.md) - Unit testing guide
- [doc/TESTING_CACHE_TTL.md](doc/TESTING_CACHE_TTL.md) - E2E testing guide
- [doc/FIO_TESTING.md](doc/FIO_TESTING.md) - Performance testing guide
- [doc/TESTING.md](doc/TESTING.md) - General testing overview

## Project Structure

```
gcscfuse/
├── build/              # Build output directory
├── src/                # Source code
│   ├── main.cpp
│   ├── gcs_fs.cpp/hpp
│   ├── stat_cache.cpp/hpp
│   ├── config.cpp/hpp
│   ├── fuse_cpp_wrapper.hpp
│   └── stat_cache_test.cpp
├── tests/              # Test suites
│   ├── e2e/            # End-to-end tests
│   └── perf/           # Performance tests
├── doc/                # Documentation
│   ├── README_gcs_fs.md
│   ├── LAZY_LOADING_IMPROVEMENTS.md
│   ├── TESTING.md
│   ├── TESTING_CACHE_TTL.md
│   ├── FIO_TESTING.md
│   └── UNIT_TEST_SETUP.md
├── CMakeLists.txt      # Unified build configuration
└── README.md
```

## Documentation

- [doc/README_gcs_fs.md](doc/README_gcs_fs.md) - GCS filesystem implementation details
- [doc/LAZY_LOADING_IMPROVEMENTS.md](doc/LAZY_LOADING_IMPROVEMENTS.md) - Lazy loading architecture
- [doc/TESTING.md](doc/TESTING.md) - Testing overview
- [doc/UNIT_TEST_SETUP.md](doc/UNIT_TEST_SETUP.md) - Unit testing guide
- [doc/TESTING_CACHE_TTL.md](doc/TESTING_CACHE_TTL.md) - E2E testing guide
- [doc/FIO_TESTING.md](doc/FIO_TESTING.md) - Performance testing guide

## License

See [LICENSE](LICENSE) file for details.
