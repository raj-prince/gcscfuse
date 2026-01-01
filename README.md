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
    libfuse3-dev curl zip unzip tar ninja-build
```

2. (First time only) Setup vcpkg and dependencies:
```bash
cd /path/to/gcscfuse
mkdir -p build && cd build
cmake .. && make setup
```

This will:
- Clone vcpkg if not present
- Bootstrap vcpkg
- Install all required C++ dependencies automatically via **vcpkg.json manifest**
  - google-cloud-cpp[storage]
  - gtest
  - yaml-cpp

3. Configure and build:
```bash
# Dependencies are installed automatically during CMake configure via vcpkg.json
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make build
```

**Note:** The vcpkg.json manifest approach automatically manages all dependencies during CMake configuration.

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
├── tests/              # Test suites
│   ├── e2e/            # End-to-end tests
│   └── perf/           # Performance tests
├── doc/                # Documentation
```

## License

See [LICENSE](LICENSE) file for details.
