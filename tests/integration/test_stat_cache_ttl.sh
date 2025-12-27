#!/bin/bash

# Test script to verify stat cache TTL and measure performance
# Usage: ./test_stat_cache_ttl.sh [bucket_name]
# Or: BUCKET=bucket_name ./test_stat_cache_ttl.sh

set -e

# Use command line argument if provided, otherwise use environment variable or default
if [ $# -ge 1 ]; then
    BUCKET=$1
else
    BUCKET="${BUCKET:-princer-working-dirs}"
fi
MOUNT_POINT="/tmp/gcs_test_mount_$$"
TEST_FILE="test_cache_file.txt"
BINARY="./gcscfuse"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Stat Cache TTL Test ===${NC}\n"

# Create mount point
mkdir -p "$MOUNT_POINT"

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

# Create a test file in GCS
echo -e "${YELLOW}Creating test file in GCS...${NC}"
echo "Test content for cache timing" | gsutil cp - "gs://${BUCKET}/${TEST_FILE}"
echo -e "${GREEN}✓ Test file created${NC}\n"

# Test 1: With cache enabled (TTL=5 seconds)
echo -e "${GREEN}=== Test 1: With Cache (TTL=5 seconds) ===${NC}"
echo "Mounting with cache enabled, TTL=5 seconds..."
$BINARY "$BUCKET" "$MOUNT_POINT" --stat-cache-ttl=5 --debug -f &
FUSE_PID=$!
sleep 2  # Wait for mount

echo -e "\n${YELLOW}First stat (cache miss - will fetch from GCS):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Second stat (cache hit - should be instant):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Third stat (cache hit - should be instant):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Waiting 6 seconds for TTL to expire...${NC}"
sleep 6

echo -e "\n${YELLOW}Fourth stat (cache expired - will fetch from GCS again):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Fifth stat (cache hit again):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

# Unmount
fusermount -u "$MOUNT_POINT"
wait $FUSE_PID 2>/dev/null || true

echo -e "\n${GREEN}=== Test 2: Without Cache ===${NC}"
echo "Mounting with cache disabled..."
$BINARY "$BUCKET" "$MOUNT_POINT" --disable-stat-cache --debug -f &
FUSE_PID=$!
sleep 2  # Wait for mount

echo -e "\n${YELLOW}First stat (no cache - always fetches from GCS):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Second stat (no cache - always fetches from GCS):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Third stat (no cache - always fetches from GCS):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

# Unmount
fusermount -u "$MOUNT_POINT"
wait $FUSE_PID 2>/dev/null || true

echo -e "\n${GREEN}=== Test 3: With Long TTL (no expiration) ===${NC}"
echo "Mounting with cache enabled, TTL=0 (never expires)..."
$BINARY "$BUCKET" "$MOUNT_POINT" --stat-cache-ttl=0 --debug -f &
FUSE_PID=$!
sleep 2  # Wait for mount

echo -e "\n${YELLOW}First stat (cache miss):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

echo -e "\n${YELLOW}Waiting 6 seconds...${NC}"
sleep 6

echo -e "\n${YELLOW}Second stat (should still be cached):${NC}"
time stat "$MOUNT_POINT/$TEST_FILE" > /dev/null

# Unmount
fusermount -u "$MOUNT_POINT"
wait $FUSE_PID 2>/dev/null || true

# Cleanup test file
echo -e "\n${YELLOW}Cleaning up test file from GCS...${NC}"
gsutil rm "gs://${BUCKET}/${TEST_FILE}"

echo -e "\n${GREEN}=== Test Complete ===${NC}"
echo -e "Expected results:"
echo -e "  - Test 1: First stat slow, 2nd-3rd fast (cached), 4th slow (expired), 5th fast"
echo -e "  - Test 2: All stats slow (no cache)"
echo -e "  - Test 3: First stat slow, 2nd fast even after 6 seconds (never expires)"
