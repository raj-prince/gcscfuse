#!/bin/bash
set -e

MOUNT_POINT=$(mktemp -d)
echo "=== Testing FUSE Performance Options ==="
echo "Mount point: $MOUNT_POINT"
echo

cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

# Test with default values
echo "Test 1: Default performance settings (background)"
cd /home/princer_google_com/dev/gcscfuse/build
./gcscfuse --enable-dummy-reader --debug princer-working-dirs "$MOUNT_POINT" > /tmp/fuse_test_1.log 2>&1 &
PID=$!
sleep 2
echo "Checking debug output:"
grep -E "Configuring FUSE performance|max_background|congestion_threshold|async_read|max_readahead" /tmp/fuse_test_1.log || echo "No debug output captured"
fusermount -u "$MOUNT_POINT"
wait $PID 2>/dev/null || true
echo

# Test with custom values
echo "Test 2: Custom performance settings (background)"
./gcscfuse --enable-dummy-reader --debug --max-background=20 --congestion-threshold=15 --disable-async-read --max-readahead=262144 princer-working-dirs "$MOUNT_POINT" > /tmp/fuse_test_2.log 2>&1 &
PID=$!
sleep 2
echo "Checking debug output:"
grep -E "Configuring FUSE performance|max_background|congestion_threshold|async_read|max_readahead" /tmp/fuse_test_2.log || echo "No debug output captured"
fusermount -u "$MOUNT_POINT"
wait $PID 2>/dev/null || true
echo

echo "✅ FUSE performance options test complete"
