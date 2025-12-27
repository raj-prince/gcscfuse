#!/bin/bash

# FIO Performance Test for GCSFS
# Tests various I/O patterns to benchmark filesystem performance
# Usage: ./fio_benchmark.sh <bucket_name> [mount_point]

set -e

# Accept bucket from environment variable, command-line argument, or default
BUCKET=${BUCKET:-${1:-princer-working-dirs}}
MOUNT_POINT=${2:-"/tmp/gcs_fio_bench_$$"}
BINARY="../build/gcscfuse"
RESULTS_DIR="./fio_results_$(date +%Y%m%d_%H%M%S)"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  GCSFS FIO Performance Benchmark${NC}"
echo -e "${BLUE}=========================================${NC}"

# Check if fio is installed
if ! command -v fio &> /dev/null; then
    echo -e "${RED}Error: fio is not installed${NC}"
    echo "Install with: sudo apt install fio"
    exit 1
fi

# Create directories
mkdir -p "$MOUNT_POINT"
mkdir -p "$RESULTS_DIR"

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

# Mount filesystem
echo -e "\n${YELLOW}Mounting GCSFS...${NC}"
$BINARY "$BUCKET" "$MOUNT_POINT" -f &> "$RESULTS_DIR/mount.log" &
FUSE_PID=$!
sleep 3

if ! mountpoint -q "$MOUNT_POINT"; then
    echo -e "${RED}Failed to mount filesystem${NC}"
    cat "$RESULTS_DIR/mount.log"
    exit 1
fi

echo -e "${GREEN}✓ Mounted at $MOUNT_POINT${NC}"

# Test configuration
TEST_SIZE="100M"
RUNTIME="30"
NUMJOBS="4"

echo -e "\n${BLUE}Test Configuration:${NC}"
echo "  File size: $TEST_SIZE"
echo "  Runtime: ${RUNTIME}s per test"
echo "  Jobs: 1 (Tests 1-4), 16 (Test 5 - Multi-threaded)"
echo "  Results: $RESULTS_DIR"

# ============================================================================
# Test 1: Sequential Write
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 1: Sequential Write${NC}"
echo -e "${BLUE}=========================================${NC}"

fio --name=seq-write \
    --directory="$MOUNT_POINT" \
    --rw=write \
    --bs=1M \
    --size=$TEST_SIZE \
    --numjobs=1 \
    --group_reporting \
    --time_based \
    --runtime=$RUNTIME \
    --output="$RESULTS_DIR/01_seq_write.json" \
    --output-format=json

# ============================================================================
# Test 2: Sequential Read
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 2: Sequential Read${NC}"
echo -e "${BLUE}=========================================${NC}"

fio --name=seq-read \
    --directory="$MOUNT_POINT" \
    --rw=read \
    --bs=1M \
    --size=$TEST_SIZE \
    --numjobs=1 \
    --group_reporting \
    --time_based \
    --runtime=$RUNTIME \
    --output="$RESULTS_DIR/02_seq_read.json" \
    --output-format=json

# ============================================================================
# ============================================================================
# Test 3: Random Write (1MB blocks)
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 3: Random Write (1MB blocks)${NC}"
echo -e "${BLUE}=========================================${NC}"

fio --name=rand-write \
    --directory="$MOUNT_POINT" \
    --rw=randwrite \
    --bs=1M \
    --size=$TEST_SIZE \
    --numjobs=1 \
    --group_reporting \
    --time_based \
    --runtime=$RUNTIME \
    --output="$RESULTS_DIR/03_rand_write_1mb.json" \
    --output-format=json

# ============================================================================
# Test 4: Random Read (1MB blocks)
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 4: Random Read (1MB blocks)${NC}"
echo -e "${BLUE}=========================================${NC}"

fio --name=rand-read \
    --directory="$MOUNT_POINT" \
    --rw=randread \
    --bs=1M \
    --size=$TEST_SIZE \
    --numjobs=1 \
    --group_reporting \
    --time_based \
    --runtime=$RUNTIME \
    --output="$RESULTS_DIR/04_rand_read_1mb.json" \
    --output-format=json

# ============================================================================
# Test 5: Multi-threaded Read (16 threads, 1MB blocks)
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 5: Multi-threaded Read (16 threads)${NC}"
echo -e "${BLUE}=========================================${NC}"

fio --name=multithread-read \
    --directory="$MOUNT_POINT" \
    --rw=read \
    --bs=1M \
    --size=$TEST_SIZE \
    --numjobs=16 \
    --group_reporting \
    --time_based \
    --runtime=$RUNTIME \
    --output="$RESULTS_DIR/05_multithread_read.json" \
    --output-format=json

# ============================================================================
# Test 6: Metadata Operations (stat)
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Test 6: Metadata Operations${NC}"
echo -e "${BLUE}=========================================${NC}"

# Create test files
echo "Creating test files..."
for i in {1..100}; do
    echo "test" > "$MOUNT_POINT/stat_test_$i.txt"
done

# Benchmark stat operations
echo "Running stat benchmark..."
START=$(date +%s.%N)
for i in {1..100}; do
    stat "$MOUNT_POINT/stat_test_$i.txt" > /dev/null 2>&1
done
END=$(date +%s.%N)
STAT_TIME=$(echo "$END - $START" | bc)
echo "100 stat operations: ${STAT_TIME}s"
echo "{\"stat_100_files_seconds\": $STAT_TIME}" > "$RESULTS_DIR/06_metadata_ops.json"

# Cleanup test files
rm -f "$MOUNT_POINT"/stat_test_*.txt

# ============================================================================
# Generate Summary Report
# ============================================================================
echo -e "\n${BLUE}=========================================${NC}"
echo -e "${BLUE}Generating Summary Report${NC}"
echo -e "${BLUE}=========================================${NC}"

cat > "$RESULTS_DIR/summary.txt" << EOF
GCSFS FIO Performance Benchmark Summary
========================================
Date: $(date)
Bucket: $BUCKET
Test Size: $TEST_SIZE
Runtime: ${RUNTIME}s per test
Jobs: 1 (Tests 1-4), 16 (Test 5 - Multi-threaded)

Test Results:
-------------
EOF

# Parse JSON results and add to summary
for result_file in "$RESULTS_DIR"/*.json; do
    if [ -f "$result_file" ]; then
        test_name=$(basename "$result_file" .json)
        
        if [[ "$test_name" == "06_metadata_ops" ]]; then
            stat_time=$(jq -r '.stat_100_files_seconds' "$result_file" 2>/dev/null || echo "N/A")
            echo "Metadata (100 stats): ${stat_time}s" >> "$RESULTS_DIR/summary.txt"
        else
            # Extract bandwidth and IOPS from FIO JSON output
            # Try write first, then read (use proper null check)
            write_bw=$(jq -r '.jobs[0].write.bw_bytes // 0' "$result_file" 2>/dev/null || echo "0")
            read_bw=$(jq -r '.jobs[0].read.bw_bytes // 0' "$result_file" 2>/dev/null || echo "0")
            
            # Use write stats if > 0, otherwise use read stats
            if [ "$write_bw" != "0" ] && [ "$write_bw" != "null" ]; then
                bw_bytes=$write_bw
                iops=$(jq -r '.jobs[0].write.iops // 0' "$result_file" 2>/dev/null || echo "0")
            else
                bw_bytes=$read_bw
                iops=$(jq -r '.jobs[0].read.iops // 0' "$result_file" 2>/dev/null || echo "0")
            fi
            
            if [ "$bw_bytes" != "0" ] && [ "$bw_bytes" != "null" ]; then
                bw_mb=$(echo "scale=2; $bw_bytes / 1024 / 1024" | bc)
                printf "%-30s BW: %8.2f MB/s, IOPS: %8.0f\n" "$test_name:" "$bw_mb" "$iops" >> "$RESULTS_DIR/summary.txt"
            fi
        fi
    fi
done

# Display summary
echo ""
cat "$RESULTS_DIR/summary.txt"

echo -e "\n${GREEN}=========================================${NC}"
echo -e "${GREEN}✓ Benchmark Complete!${NC}"
echo -e "${GREEN}=========================================${NC}"
echo -e "Results saved to: ${BLUE}$RESULTS_DIR${NC}"
echo ""
echo "View detailed results:"
echo "  cat $RESULTS_DIR/summary.txt"
echo "  cat $RESULTS_DIR/*.json"
echo ""
echo "Compare with cache disabled:"
echo "  $0 $BUCKET --disable-stat-cache --disable-file-cache"
