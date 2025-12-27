#!/bin/bash

# FIO Comparison Benchmark - Compare cache enabled vs disabled
# Usage: ./fio_compare.sh <bucket_name>

set -e

# Accept bucket from environment variable, command-line argument, or default
BUCKET=${BUCKET:-${1:-princer-working-dirs}}
RESULTS_DIR="./fio_comparison_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  FIO Comparison Benchmark${NC}"
echo -e "${BLUE}  Cache Enabled vs Disabled${NC}"
echo -e "${BLUE}=========================================${NC}"

# Test with cache enabled
echo -e "\n${GREEN}Running benchmark WITH cache...${NC}"
./fio_benchmark.sh "$BUCKET" "/tmp/gcs_bench_cached_$$" | tee "$RESULTS_DIR/with_cache.log"

sleep 5

# Test with cache disabled
echo -e "\n${GREEN}Running benchmark WITHOUT cache...${NC}"
MOUNT_POINT="/tmp/gcs_bench_nocache_$$"
mkdir -p "$MOUNT_POINT"

../build/gcs_fs "$BUCKET" "$MOUNT_POINT" --disable-stat-cache --disable-file-cache -f &> "$RESULTS_DIR/nocache_mount.log" &
FUSE_PID=$!
sleep 3

# Run limited benchmark (shorter runtime for comparison)
fio --name=comparison \
    --directory="$MOUNT_POINT" \
    --rw=randrw \
    --rwmixread=70 \
    --bs=4K \
    --size=50M \
    --numjobs=2 \
    --group_reporting \
    --time_based \
    --runtime=15 \
    --output="$RESULTS_DIR/without_cache.json" \
    --output-format=json

fusermount -u "$MOUNT_POINT" 2>/dev/null || true
rmdir "$MOUNT_POINT"

# Generate comparison report
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Comparison Results${NC}"
echo -e "${BLUE}=========================================${NC}"

# Parse and compare results
# (Basic comparison - can be enhanced with more detailed analysis)

echo "Results saved to: $RESULTS_DIR"
echo ""
echo "Expected outcomes:"
echo "  - Sequential operations: Similar performance (direct GCS I/O)"
echo "  - Random operations: Better with cache (reduced GCS API calls)"
echo "  - Metadata operations: Significantly better with cache"
