#!/bin/bash

# Simple performance test for stat cache
# Usage: ./quick_cache_test.sh <bucket_name>

if [ $# -lt 1 ]; then
    echo "Usage: $0 <bucket_name>"
    exit 1
fi

BUCKET=$1
MOUNT_POINT="/tmp/gcs_test_$$"
TEST_FILE="cache_test_file.txt"

echo "Creating test file in GCS..."
echo "Test content $(date)" | gsutil cp - "gs://${BUCKET}/${TEST_FILE}"

mkdir -p "$MOUNT_POINT"

cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
    gsutil rm "gs://${BUCKET}/${TEST_FILE}" 2>/dev/null || true
}
trap cleanup EXIT

echo ""
echo "=== Test 1: Cache with 3 second TTL ==="
echo "Starting filesystem with --stat-cache-ttl=3..."
../build/gcscfuse "$BUCKET" "$MOUNT_POINT" --stat-cache-ttl=3 --debug -f 2>&1 | grep -E '\[DEBUG\]|Stat cache' &
FUSE_PID=$!
sleep 3

echo ""
echo "Stat #1 (expect MISS - not in cache yet):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #2 (expect HIT - cached from #1):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #3 (expect HIT - still cached):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'

echo ""
echo "Waiting 3.5 seconds for TTL to expire..."
sleep 3.5

echo ""
echo "Stat #4 (expect MISS - cache expired):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #5 (expect HIT - fresh cache from #4):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'

kill $FUSE_PID 2>/dev/null || true
wait $FUSE_PID 2>/dev/null || true
fusermount -u "$MOUNT_POINT" 2>/dev/null || true
sleep 2

echo ""
echo ""
echo "=== Test 2: No cache (--disable-stat-cache) ==="
echo "Starting filesystem with cache disabled..."
../build/gcscfuse "$BUCKET" "$MOUNT_POINT" --disable-stat-cache --debug -f 2>&1 | grep -E '\[DEBUG\]' &
FUSE_PID=$!
sleep 3

echo ""
echo "Stat #1 (no cache, fetches from GCS):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #2 (no cache, fetches from GCS again):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #3 (no cache, fetches from GCS again):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'

kill $FUSE_PID 2>/dev/null || true
wait $FUSE_PID 2>/dev/null || true

echo ""
echo ""
echo "=== Test 3: Using Config File ==="
CONFIG_FILE="/tmp/gcsfuse_test_config_$$.yaml"
cat > "$CONFIG_FILE" << EOF
bucket_name: $BUCKET
mount_point: $MOUNT_POINT
enable_stat_cache: true
stat_cache_timeout: 3
debug: true
EOF

echo "Starting filesystem with config file..."
../build/gcscfuse --config "$CONFIG_FILE" -f 2>&1 | grep -E '[DEBUG]|Stat cache' &
FUSE_PID=$!
sleep 3

echo ""
echo "Stat #1 (config file - expect MISS):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'
sleep 0.5

echo ""
echo "Stat #2 (config file - expect HIT):"
stat "$MOUNT_POINT/$TEST_FILE" 2>&1 | grep -E 'File:|Size:'

kill $FUSE_PID 2>/dev/null || true
wait $FUSE_PID 2>/dev/null || true
rm -f "$CONFIG_FILE"

echo ""
echo "=== Test Complete ==="
echo "Check the debug output above:"
echo "  - Test 1: Should see MISS, HIT, HIT, MISS (expired), HIT"
echo "  - Test 2: Should NOT see any cache messages (cache disabled)"
echo "  - Test 3: Config file should work same as command-line flags"
