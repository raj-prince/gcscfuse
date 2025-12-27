# GCSFS Tests

This directory contains tests for the GCSFS implementation.

## Test Types

### 1. Unit Tests (`unit/`)

Fast, isolated tests for individual components without external dependencies.

**Run unit tests:**
```bash
cd unit/build
./stat_cache_test
```

**Features:**
- 26 tests for StatCache class
- Tests basic operations, TTL expiration, path handling, edge cases
- Runs in ~2 seconds
- No GCS bucket required

See [unit/README.md](unit/README.md) for details.

### 2. Integration Tests

Tests with actual GCS bucket and mounted filesystem.

**Available tests:**

- **test_cache_ttl.py** - Python test with detailed timing (recommended)
- **benchmark_cache.sh** - Bash benchmark using `time` command
- **quick_cache_test.sh** - Quick test with debug output
- **test_stat_cache_ttl.sh** - Full test suite

See [TESTING_CACHE_TTL.md](TESTING_CACHE_TTL.md) for details.

### 3. Performance Tests (FIO)

Benchmark filesystem I/O performance using FIO.

**Prerequisites:**
```bash
sudo apt install fio jq bc
```

**Run comprehensive benchmark:**
```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
./fio_benchmark.sh <your-bucket-name>
```

**Compare cache performance:**
```bash
./fio_compare.sh <your-bucket-name>
```

**Tests performed:**
- Sequential write (1MB blocks)
- Sequential read (1MB blocks)  
- Random write (4K blocks)
- Random read (4K blocks)
- Mixed read/write workload (70/30)
- Metadata operations (100 stat calls)

**Output:**
- JSON files with detailed metrics
- Summary report with bandwidth and IOPS
- Results directory: `fio_results_<timestamp>/`

**Expected performance:**
- Sequential: 10-100 MB/s (network dependent)
- Random with cache: Better than without
- Metadata with cache: 100x-1000x speedup

- **test_cache_ttl.py** - Python test with detailed timing (recommended)
- **benchmark_cache.sh** - Bash benchmark using `time` command
- **quick_cache_test.sh** - Quick test with debug output
- **test_stat_cache_ttl.sh** - Full test suite
- **fio_benchmark.sh** - FIO performance benchmark (NEW)
- **fio_compare.sh** - Compare cache vs no-cache performance (NEW)

See [TESTING_CACHE_TTL.md](TESTING_CACHE_TTL.md) for detailed documentation.

## Running Tests

### Quick Start

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
python3 test_cache_ttl.py <your-bucket-name>
```

### Prerequisites

- GCS bucket with read/write access
- `gsutil` installed and authenticated
- FUSE installed (`sudo apt install fuse3`)
- Python 3 (for Python tests)
- Google Test (`sudo apt install libgtest-dev`) for unit tests
- FIO (`sudo apt install fio jq bc`) for performance tests

### Building

Before running tests, build the project:

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/build
make
```

## Test Results

Tests verify:
- ✅ Stat cache TTL expiration works correctly
- ✅ Cache provides significant performance improvements
- ✅ Cache hits are orders of magnitude faster than GCS fetches
- ✅ Expired entries are re-fetched from GCS
- ✅ Filesystem I/O performance meets expectations
- ✅ Sequential and random workloads perform correctly

Expected performance:
- **Unit tests**: ~2 seconds for 26 tests
- **With cache**: ~0.0001s per stat (cached)
- **Without cache**: ~0.05-0.2s per stat (GCS fetch)
- **Cache speedup**: 100x-1000x for repeated operations
- **Sequential I/O**: 10-100 MB/s (network dependent)
- **Random I/O with cache**: Better latency and throughput
