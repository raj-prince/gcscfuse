# Stat Cache TTL Testing

This directory contains test scripts to verify that the stat cache TTL (Time-To-Live) functionality works correctly and to measure cache performance.

## Test Scripts

### 1. test_cache_ttl.py (Recommended)
**Python script with clear output and timing measurements**

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
python3 test_cache_ttl.py <your-bucket-name>
```

**What it tests:**
- Test 1: Cache with 3-second TTL
  - First stat: Cache MISS (slow - fetches from GCS)
  - Stats 2-3: Cache HIT (fast - from memory)
  - Wait 3.5 seconds for expiration
  - Stat 4: Cache MISS (slow - cache expired, re-fetch)
  - Stat 5: Cache HIT (fast - newly cached)

- Test 2: Cache disabled
  - All stats are slow (every call fetches from GCS)

- Test 3: Performance benchmark
  - 10 consecutive stats with cache enabled
  - Should be very fast (<0.01s total)

**Expected output:**
```
TEST 1: Cache with 3-second TTL
[1] First stat (expect cache MISS):    Time: 0.0523s
[2] Second stat (expect cache HIT):    Time: 0.0001s  ← Fast!
[3] Third stat (expect cache HIT):     Time: 0.0001s  ← Fast!
⏰ Waiting 3.5 seconds for TTL to expire...
[4] Fourth stat after expiration:      Time: 0.0489s  ← Slow (expired)
[5] Fifth stat (expect cache HIT):     Time: 0.0001s  ← Fast again!
```

### 2. benchmark_cache.sh
**Bash script using `time` command for precise measurements**

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
./benchmark_cache.sh <your-bucket-name>
```

Tests cache with TTL=5s, no cache, and TTL=0 (never expires).

### 3. quick_cache_test.sh
**Quick test with debug output to see cache hits/misses**

```bash
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/tests
./quick_cache_test.sh <your-bucket-name>
```

Shows `[DEBUG] ✓ Stat cache HIT` and `[DEBUG] ✗ Stat cache MISS` messages.

## What to Look For

### ✅ TTL Working Correctly
- Initial stat is slow (cache miss)
- Subsequent stats are fast (cache hits)
- After TTL expires, stat becomes slow again (cache miss)
- Following stats are fast again (fresh cache)

### ✅ Performance Improvement
- **With cache**: First stat ~0.05-0.2s, subsequent stats ~0.0001-0.001s
- **Without cache**: All stats ~0.05-0.2s

### ❌ If TTL Not Working
- Stats would remain fast even after TTL expires
- Or stats would always be slow (cache not working at all)

## Debug Output

When running with `--debug` flag, you'll see:
```
[DEBUG] Stat cache TTL: 3 seconds
[DEBUG] ✓ Stat cache HIT for: /test_file.txt
[DEBUG] ✗ Stat cache MISS for: /test_file.txt (expired or not cached)
```

## Manual Testing

If you want to test manually:

```bash
# Terminal 1: Start filesystem with 5-second TTL
cd /home/princer_google_com/dev/gcscfuse/src/gcs_fs/build
./gcs_fs your-bucket /tmp/test_mount --stat-cache-ttl=5 --debug -f

# Terminal 2: Run tests
stat /tmp/test_mount/your-file.txt    # Slow (miss)
stat /tmp/test_mount/your-file.txt    # Fast (hit)
sleep 6                                # Wait for expiration
stat /tmp/test_mount/your-file.txt    # Slow (miss, expired)
stat /tmp/test_mount/your-file.txt    # Fast (hit)
```

## Troubleshooting

### Permission Issues
```bash
sudo usermod -a -G fuse $USER
# Log out and back in
```

### Mount Point Busy
```bash
fusermount -u /tmp/test_mount
```

### Test File Not Found
The scripts automatically create test files. If you see errors, ensure:
- You have write access to the bucket
- `gsutil` is installed and authenticated: `gsutil ls gs://your-bucket`

## Implementation Details

The TTL implementation uses **lazy eviction**:
- Cache entries are not actively removed when they expire
- Expiration is checked when `getStat()` is called
- If `(current_time - cache_time) > timeout`, the entry is treated as expired
- No background cleanup threads needed

See `stat_cache.cpp::getStat()` and `stat_cache.hpp::isExpired()` for implementation.
