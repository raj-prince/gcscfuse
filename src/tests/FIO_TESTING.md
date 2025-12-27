# FIO Performance Testing Guide

This guide explains how to use FIO (Flexible I/O Tester) to benchmark GCSFS performance.

## What is FIO?

FIO is an industry-standard I/O benchmarking tool that can simulate various workload patterns:
- Sequential/random reads and writes
- Different block sizes
- Multiple concurrent jobs
- Mixed read/write ratios

## Installation

```bash
sudo apt update
sudo apt install -y fio jq bc
```

**Required tools:**
- `fio` - I/O benchmarking tool
- `jq` - JSON parser for processing results
- `bc` - Calculator for summary statistics

## Available Tests

### 1. fio_benchmark.sh

Comprehensive benchmark suite testing various I/O patterns.

**Usage:**
```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
./fio_benchmark.sh <bucket-name> [mount-point]
```

**Tests performed:**
1. **Sequential Write** (1MB blocks)
   - Measures sustained write throughput
   - Tests GCS upload performance
   
2. **Sequential Read** (1MB blocks)
   - Measures sustained read throughput
   - Tests GCS download performance
   
3. **Random Write** (4K blocks)
   - Tests small random write performance
   - Stresses write buffering
   
4. **Random Read** (4K blocks)
   - Tests small random read performance
   - Benefits from file content cache
   
5. **Mixed Read/Write** (70% read, 30% write, 4K blocks)
   - Simulates realistic workload
   - Tests cache effectiveness under mixed operations
   
6. **Metadata Operations**
   - 100 stat() calls
   - Tests stat cache performance

**Configuration:**
- Test size: 100MB per job
- Runtime: 30 seconds per test
- Jobs: 4 concurrent jobs
- Results: JSON files in `fio_results_<timestamp>/`

**Example output:**
```
01_seq_write:                  BW:    42.50 MB/s, IOPS:       42
02_seq_read:                   BW:    85.30 MB/s, IOPS:       85
03_rand_write_4k:              BW:     8.20 MB/s, IOPS:     2099
04_rand_read_4k:               BW:    12.40 MB/s, IOPS:     3174
05_mixed_rw:                   BW:    10.80 MB/s, IOPS:     2765
Metadata (100 stats):          0.15s
```

### 2. fio_compare.sh

Compares performance with cache enabled vs disabled.

**Usage:**
```bash
./fio_compare.sh <bucket-name>
```

**Purpose:**
- Demonstrates cache effectiveness
- Shows performance impact of disabling caches
- Useful for tuning cache settings

**Expected differences:**
- Sequential I/O: Minimal difference (direct GCS I/O)
- Random I/O: Better with cache (reduced API calls)
- Metadata ops: Significantly better with cache (100x-1000x)

## Understanding Results

### Bandwidth (BW)
- Measured in MB/s
- Higher is better
- Depends on network speed to GCS

**Typical ranges:**
- Sequential: 10-100 MB/s
- Random: 5-20 MB/s
- With cache: Can be much higher for repeated access

### IOPS (I/O Operations Per Second)
- Measured in operations/second
- Higher is better
- Important for small block sizes

**Typical ranges:**
- 4K random with cache: 1000-5000 IOPS
- 4K random without cache: 100-1000 IOPS

### Latency
- Measured in microseconds (us) or milliseconds (ms)
- Lower is better
- Found in detailed JSON output

**Typical ranges:**
- Cache hit: < 1ms
- Cache miss (GCS): 50-200ms

## Interpreting Results

### Good Performance Indicators
✅ Sequential read/write > 30 MB/s  
✅ Random 4K IOPS > 1000 (with cache)  
✅ Metadata operations < 1 second for 100 stats  
✅ Cache hits significantly faster than misses

### Performance Issues
❌ Sequential < 10 MB/s (check network)  
❌ Random IOPS < 100 (cache may be disabled)  
❌ High latency (> 500ms) consistently  
❌ No difference with/without cache (cache not working)

## Detailed Results

### JSON Output

Each test produces a JSON file with detailed metrics:

```bash
# View detailed results
jq . fio_results_<timestamp>/01_seq_write.json

# Extract specific metrics
jq '.jobs[0].write.bw_bytes' result.json  # Bandwidth in bytes/s
jq '.jobs[0].write.iops' result.json      # IOPS
jq '.jobs[0].write.lat_ns.mean' result.json  # Mean latency
```

### Key Metrics in JSON
- `bw_bytes` - Bandwidth in bytes per second
- `iops` - I/O operations per second
- `lat_ns` - Latency distribution (nanoseconds)
- `clat_ns` - Completion latency
- `slat_ns` - Submission latency

## Customizing Tests

### Change Test Duration
```bash
# Edit fio_benchmark.sh
RUNTIME="60"  # 60 seconds instead of 30
```

### Change File Size
```bash
TEST_SIZE="1G"  # 1GB instead of 100MB
```

### Change Concurrency
```bash
NUMJOBS="8"  # 8 jobs instead of 4
```

### Run Single Test
```bash
# Sequential write only
fio --name=test \
    --directory=/mnt/gcs \
    --rw=write \
    --bs=1M \
    --size=100M \
    --numjobs=4 \
    --group_reporting
```

## Common Workload Patterns

### Database-like (random small I/O)
```bash
fio --name=database \
    --rw=randrw \
    --rwmixread=80 \
    --bs=8K \
    --size=500M \
    --numjobs=8 \
    --runtime=60 \
    --time_based
```

### Streaming (large sequential)
```bash
fio --name=streaming \
    --rw=read \
    --bs=4M \
    --size=1G \
    --numjobs=1 \
    --runtime=60 \
    --time_based
```

### Mixed Application
```bash
fio --name=mixed \
    --rw=randrw \
    --rwmixread=70 \
    --bs=4K:16K:64K \
    --size=200M \
    --numjobs=4 \
    --runtime=60 \
    --time_based
```

## Troubleshooting

### FIO not found
```bash
sudo apt install fio
which fio  # Verify installation
```

### Permission denied
```bash
# Check mount point permissions
ls -ld /mnt/gcs

# Ensure FUSE is mounted
mountpoint /mnt/gcs
```

### Test fails with "No space left"
- GCS bucket may be full
- Check quota: `gsutil du -s gs://bucket-name`

### Results seem incorrect
- Verify cache settings with `--debug` flag
- Check network speed: `speedtest-cli`
- Compare with baseline tests

## Best Practices

1. **Warm up first**: Run a small test before benchmarking
2. **Use consistent test size**: Don't exceed available memory
3. **Run multiple times**: Average results for accuracy
4. **Monitor resources**: Check CPU, memory, network during tests
5. **Compare apples-to-apples**: Same test parameters for comparisons
6. **Document environment**: Network speed, VM specs, etc.

## Example Workflow

```bash
# 1. Build filesystem
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/build
make

# 2. Run comprehensive benchmark
cd ../tests
./fio_benchmark.sh my-bucket

# 3. Review results
cat fio_results_*/summary.txt

# 4. Compare with cache disabled
./fio_compare.sh my-bucket

# 5. Analyze specific test
jq . fio_results_*/04_rand_read_4k.json | less
```

## Performance Tuning Tips

### For Sequential Workloads
- Use larger block sizes (1MB - 4MB)
- Reduce number of concurrent jobs
- Disable stat cache (won't help much here)

### For Random Workloads
- Enable stat cache with appropriate TTL
- Enable file content cache
- Use smaller block sizes (4K - 64K)
- Increase concurrent jobs

### For Metadata-Heavy Workloads
- Enable stat cache (critical!)
- Set longer TTL if data changes infrequently
- Consider using `--stat-cache-ttl=0` for static data

## References

- [FIO Documentation](https://fio.readthedocs.io/)
- [FIO GitHub](https://github.com/axboe/fio)
- [GCSFS Testing Guide](TESTING_CACHE_TTL.md)
- [GCSFS Unit Tests](unit/README.md)

## Support

For issues or questions:
- Check [tests/README.md](README.md)
- Review debug output with `--debug` flag
- Compare results with baseline metrics
