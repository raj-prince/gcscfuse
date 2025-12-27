# Lazy Loading and Stat Cache Timeout Improvements

## Overview
Implemented two major improvements to GCSFS:
1. **Lazy per-directory loading** - replaced upfront bucket-wide listing with on-demand per-directory loading
2. **Timeout-based stat cache eviction** - added configurable TTL for cached metadata entries

## Changes Made

### 1. Lazy Per-Directory Loading

**Problem**: The previous implementation loaded ALL objects from the entire bucket at initialization (`loadFileList()`), which caused:
- Long initialization times for large buckets
- High memory usage (all file paths stored in memory)
- Not scalable for buckets with millions of objects

**Solution**: Implemented lazy loading that lists only the immediate children of a directory when accessed.

#### Modified Functions:

**loadFileList()** ([gcs_fs.cpp](gcs_fs.cpp))
- Now deprecated and does nothing
- Left as a stub for compatibility

**readdir()** ([gcs_fs.cpp](gcs_fs.cpp))
- Now uses `client_.ListObjects()` with `Prefix` and `Delimiter` parameters
- Lists only objects within the specific directory being accessed
- Example: listing `/foo/bar/` only fetches objects with prefix `foo/bar/` and delimiter `/`
- Populates stat cache entries as directories are explored

**isValidPath()** ([gcs_fs.cpp](gcs_fs.cpp))
- No longer uses the global `file_list_`
- First checks stat cache for cached results
- Then tries `GetObjectMetadata()` to check if path is a file
- Finally uses `ListObjects()` with `Prefix` and `MaxResults(1)` to check if path is a directory

**isDirectory()** ([gcs_fs.cpp](gcs_fs.cpp))
- Similar approach to `isValidPath()`
- Checks cache first, then queries GCS with prefix listing

**Removed/Commented**:
- `file_list_` member variable (was storing all file paths)
- `files_loaded_` flag
- References to `file_list_` in `create()` and `unlink()` operations

#### Benefits:
- ✅ Fast startup - no upfront bucket scan
- ✅ Memory efficient - only caches accessed paths
- ✅ Scales to millions of objects
- ✅ Works with stat cache for performance

### 2. Timeout-Based Stat Cache Eviction

**Problem**: Cached stat information (file sizes, modification times, permissions) never expired, leading to stale metadata if files changed outside the filesystem.

**Solution**: Added configurable TTL (Time To Live) for cache entries with lazy eviction.

#### Modified Files:

**stat_cache.hpp**:
```cpp
struct StatInfo {
    ...
    time_t cache_time;  // NEW: Time when entry was cached
    ...
};

// NEW: Cache timeout configuration
void setCacheTimeout(int timeout_seconds);

// NEW: Helper to check expiration
bool isExpired(const StatInfo& info) const;

// NEW: Member variable
int cache_timeout_ = 60;  // Default 60 seconds
```

**stat_cache.cpp**:
- `insertFile()`: Sets `cache_time = time(nullptr)` when entries are added
- `insertDirectory()`: Sets `cache_time = time(nullptr)` when entries are added
- `getStat()`: Checks `isExpired()` and returns `std::nullopt` if entry is stale

**config.hpp** & **config.cpp**:
- Added `stat_cache_timeout` config option (default: 60 seconds)
- Added `--stat-cache-ttl=N` command-line flag
- Value of 0 disables timeout (cache never expires)

**gcs_fs.cpp**:
- Constructor calls `stat_cache_.setCacheTimeout(config_.stat_cache_timeout)`
- Debug output shows configured TTL value

#### Eviction Strategy:
- **Lazy eviction**: Entries are not actively removed from cache
- **Check on access**: When `getStat()` is called, timestamp is checked
- If `(current_time - cache_time) > timeout`, returns `nullopt` as if entry doesn't exist
- Next access will re-fetch from GCS and update the cache with fresh data

#### Benefits:
- ✅ Prevents stale metadata issues
- ✅ Configurable TTL per use case
- ✅ Low overhead (no background cleanup threads)
- ✅ Can disable timeout with `--stat-cache-ttl=0`

## Usage Examples

### Default behavior (60 second cache TTL):
```bash
./gcs_fs my-bucket /mnt/gcs
```

### Disable cache timeout (cache never expires):
```bash
./gcs_fs my-bucket /mnt/gcs --stat-cache-ttl=0
```

### Short TTL for frequently changing data:
```bash
./gcs_fs my-bucket /mnt/gcs --stat-cache-ttl=10
```

### Debug mode to see cache activity:
```bash
./gcs_fs my-bucket /mnt/gcs -d --stat-cache-ttl=30
```

## Performance Characteristics

### Before (Global Loading):
- Startup time: O(n) where n = total objects in bucket
- Memory: O(n) - all file paths in memory
- Directory listing: O(1) - already loaded
- First access: Fast (after initial load)

### After (Lazy Loading):
- Startup time: O(1) - immediate
- Memory: O(accessed paths) - only accessed directories cached
- Directory listing: O(m) where m = objects in that directory
- First access: Slightly slower (one GCS API call per directory)
- Subsequent access: Fast (cached with TTL)

### Optimal For:
- ✅ Large buckets (millions of objects)
- ✅ Deep directory hierarchies
- ✅ Accessing only a subset of bucket contents
- ✅ Long-running mounts where consistency matters

## Implementation Notes

1. **GCS API Usage**:
   - `Prefix(path)`: Filters objects starting with path
   - `Delimiter("/")`: Groups objects by directory level
   - `MaxResults(1)`: Efficient existence checks

2. **Cache Strategy**:
   - Stat cache populated as directories are explored
   - Cache checked before GCS API calls
   - Expired entries treated as cache miss

3. **Thread Safety**:
   - All cache operations are const-correct
   - GCS client is thread-safe
   - No explicit locking needed (FUSE handles serialization)

4. **Backward Compatibility**:
   - Old `loadFileList()` function kept as no-op stub
   - Existing command-line flags unchanged
   - Default behavior improved, no breaking changes

## Testing Recommendations

1. **Verify lazy loading**:
   ```bash
   # Mount filesystem
   ./gcs_fs my-bucket /mnt/gcs -d
   
   # List a deep directory - should be fast startup
   ls /mnt/gcs/path/to/deep/dir
   ```

2. **Verify cache timeout**:
   ```bash
   # Upload file externally
   gsutil cp test.txt gs://my-bucket/test.txt
   
   # Check via FUSE (should see old size/mtime until TTL expires)
   stat /mnt/gcs/test.txt
   
   # Wait for TTL
   sleep 65
   
   # Check again (should see new metadata)
   stat /mnt/gcs/test.txt
   ```

3. **Performance test**:
   ```bash
   # Large bucket test
   time ls /mnt/gcs/  # Should be fast even with millions of objects
   ```

## Future Enhancements

- Background cache refresh for frequently accessed paths
- Adaptive TTL based on file change frequency
- LRU eviction for memory-constrained environments
- Prefetching for predictable access patterns
