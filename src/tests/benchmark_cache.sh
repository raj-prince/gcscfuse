#!/bin/bash

# Timing benchmark for stat cache
# Usage: ./benchmark_cache.sh <bucket_name>

if [ $# -lt 1 ]; then
    echo "Usage: $0 <bucket_name>"
    exit 1
fi

BUCKET=$1
MOUNT_POINT="/tmp/gcs_bench_$$"
TEST_FILE="benchmark_file.txt"

echo "Setup: Creating test file in GCS..."
echo "Benchmark test content" | gsutil cp - "gs://${BUCKET}/${TEST_FILE}" 2>/dev/null

mkdir -p "$MOUNT_POINT"

cleanup() {
    fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    rmdir "$MOUNT_POINT" 2>/dev/null || true
    gsutil rm "gs://${BUCKET}/${TEST_FILE}" 2>/dev/null || true
}
trap cleanup EXIT

# Set binary path
BINARY="../build/gcs_fs"

benchmark_with_cache() {
    local ttl=$1
    echo ""
    echo "========================================="
    echo "Benchmark: WITH Cache (TTL=${ttl}s)"
    echo "========================================="
    
    $BINARY "$BUCKET" "$MOUNT_POINT" --stat-cache-ttl=$ttl -f &>/dev/null &
    local pid=$!
    sleep 2
    
    echo "First stat (cache MISS - will fetch from GCS):"
    time -p stat "$MOUNT_POINT/$TEST_FILE" &>/dev/null
    
    echo ""
    echo "10 consecutive stats (cache HITs - should be fast):"
    time -p bash -c "for i in {1..10}; do stat '$MOUNT_POINT/$TEST_FILE' &>/dev/null; done"
    
    if [ "$ttl" != "0" ]; then
        echo ""
        echo "Waiting $(($ttl + 1)) seconds for cache to expire..."
        sleep $(($ttl + 1))
        
        echo ""
        echo "Stat after expiration (cache MISS - will fetch again):"
        time -p stat "$MOUNT_POINT/$TEST_FILE" &>/dev/null
    fi
    
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    fusermount -u "$MOUNT_POINT" 2>/dev/null
    sleep 1
}

benchmark_without_cache() {
    echo ""
    echo "========================================="
    echo "Benchmark: WITHOUT Cache"
    echo "========================================="
    
    $BINARY "$BUCKET" "$MOUNT_POINT" --disable-stat-cache -f &>/dev/null &
    local pid=$!
    sleep 2
    
    echo "First stat (no cache, fetches from GCS):"
    time -p stat "$MOUNT_POINT/$TEST_FILE" &>/dev/null
    
    echo ""
    echo "10 consecutive stats (no cache, each fetches from GCS):"
    time -p bash -c "for i in {1..10}; do stat '$MOUNT_POINT/$TEST_FILE' &>/dev/null; done"
    
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    fusermount -u "$MOUNT_POINT" 2>/dev/null
    sleep 1
}

# Run benchmarks
benchmark_with_cache 5
benchmark_without_cache
benchmark_with_cache 0  # No expiration

echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo "Expected results:"
echo "  - With cache: First stat ~0.1-0.5s, 10 stats ~0.001-0.01s total"
echo "  - Without cache: All stats slow, 10 stats ~1-5s total"
echo "  - Cache expiration: Should see slow stat after TTL expires"
echo ""
echo "Cache provides significant speedup for repeated stat operations!"
