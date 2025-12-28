# Benchmarking Guide

This document summarizes the main test suite and example performance numbers for gcscfuse.

## Test Suite

### fio_benchmark.sh
- Runs a comprehensive set of I/O benchmarks (sequential/random read/write, mixed, metadata)
- Usage:
  ```bash
  ./fio_benchmark.sh <bucket-name> [mount-point]
  ```
- Output: JSON and summary files in `fio_results_<timestamp>/`

## Example Results

| Test                  | Bandwidth (MB/s) | IOPS   | Notes                        |
|----------------------|------------------|--------|------------------------------|
| Sequential Write     | 42.5             | 42     | GCS upload                   |
| Sequential Read      | 85.3             | 85     | GCS download                 |
| Random Write (4K)    | 8.2              | 2099   | Small block, stresses cache  |
| Random Read (4K)     | 12.4             | 3174   | Cache benefits               |
| Mixed Read/Write     | 10.8             | 2765   | 70% read, 30% write          |
| Metadata (100 stats) | -                | -      | 0.15s (stat cache test)      |

- With cache enabled, random and metadata operations are much faster.
- Sequential I/O is limited by network and GCS throughput.

## How to Run

1. Build and mount the filesystem as described in the main README.
2. Run the benchmark script from the tests directory.
3. Review the summary output for bandwidth, IOPS, and latency.

---

For more details, see the full scripts and JSON output in the test results directory.
