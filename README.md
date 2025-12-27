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

### Integration Tests
Cache TTL verification with real GCS:
```bash
cd src/tests
python3 test_cache_ttl.py
```

### Performance Tests (FIO)
I/O benchmarking with FIO:
```bash
cd src/tests
./fio_benchmark.sh <bucket-name>
./fio_compare.sh <bucket-name>  # Compare cache enabled vs disabled
```

See [src/tests/FIO_TESTING.md](src/tests/FIO_TESTING.md) for detailed performance testing guide.

## Project Structure

```
gcscfuse/
├── build/              # Build output directory (parallel to src)
├── src/                # Source code
│   ├── main.cpp
│   ├── gcs_fs.cpp/hpp
│   ├── stat_cache.cpp/hpp
│   ├── config.cpp/hpp
│   ├── fuse_cpp_wrapper.hpp
│   ├── unit_tests/     # Unit tests
│   └── tests/          # Integration and performance tests
├── CMakeLists.txt      # Single unified build configuration
└── README.md
```

## Documentation

- [LAZY_LOADING_IMPROVEMENTS.md](LAZY_LOADING_IMPROVEMENTS.md) - Implementation details
- [src/tests/FIO_TESTING.md](src/tests/FIO_TESTING.md) - Performance testing guide
- [README_gcs_fs.md](README_gcs_fs.md) - Additional GCS filesystem documentation

## License

See [LICENSE](LICENSE) file for details.
