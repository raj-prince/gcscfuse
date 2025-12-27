#!/usr/bin/env python3
"""
Test script to verify stat cache TTL functionality and measure performance.
Usage: python3 test_cache_ttl.py <bucket_name>
"""

import sys
import os
import time
import subprocess
import tempfile
from pathlib import Path

def run_cmd(cmd, check=True, capture=True):
    """Run a command and optionally capture output."""
    if capture:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return result.returncode, result.stdout, result.stderr
    else:
        return subprocess.run(cmd, shell=True, check=check).returncode, "", ""

def cleanup(mount_point, test_file, bucket):
    """Cleanup resources."""
    run_cmd(f"fusermount -u {mount_point}", check=False)
    run_cmd(f"rmdir {mount_point}", check=False)
    run_cmd(f"gsutil rm gs://{bucket}/{test_file}", check=False)

def test_cache_ttl(bucket):
    """Test cache TTL functionality."""
    mount_point = tempfile.mkdtemp(prefix="gcs_cache_test_")
    test_file = "cache_ttl_test.txt"
    
    # Find the binary
    if os.path.exists("./gcs_fs"):
        binary = "./gcs_fs"
    elif os.path.exists("../build/gcs_fs"):
        binary = "../build/gcs_fs"
    elif os.path.exists("./src/gcs_fs/build/gcs_fs"):
        binary = "./src/gcs_fs/build/gcs_fs"
    elif os.path.exists("/home/princer_google_com/dev/gcscfuse/src/gcs_fs/build/gcs_fs"):
        binary = "/home/princer_google_com/dev/gcscfuse/src/gcs_fs/build/gcs_fs"
    else:
        raise FileNotFoundError("Cannot find gcs_fs binary")
    
    print("=" * 60)
    print("STAT CACHE TTL TEST")
    print("=" * 60)
    
    # Create test file
    print("\n📝 Creating test file in GCS...")
    run_cmd(f'echo "Test content" | gsutil cp - gs://{bucket}/{test_file}')
    print(f"✓ Created gs://{bucket}/{test_file}")
    
    try:
        # Test 1: With 3-second TTL
        print("\n" + "=" * 60)
        print("TEST 1: Cache with 3-second TTL")
        print("=" * 60)
        
        print(f"\n🚀 Starting filesystem (cache TTL=3s)...")
        proc = subprocess.Popen(
            [binary, bucket, mount_point, "--stat-cache-ttl=3", "--debug", "-f"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        time.sleep(2)  # Wait for mount
        
        file_path = f"{mount_point}/{test_file}"
        
        # First stat - should be cache MISS
        print("\n[1] First stat (expect cache MISS):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        time.sleep(0.2)
        
        # Second stat - should be cache HIT
        print("\n[2] Second stat (expect cache HIT - fast):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        time.sleep(0.2)
        
        # Third stat - should be cache HIT
        print("\n[3] Third stat (expect cache HIT - fast):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        # Wait for TTL to expire
        print("\n⏰ Waiting 3.5 seconds for TTL to expire...")
        time.sleep(3.5)
        
        # Fourth stat - should be cache MISS (expired)
        print("\n[4] Fourth stat after expiration (expect cache MISS):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        time.sleep(0.2)
        
        # Fifth stat - should be cache HIT again
        print("\n[5] Fifth stat (expect cache HIT):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        proc.terminate()
        proc.wait(timeout=3)
        run_cmd(f"fusermount -u {mount_point}", check=False)
        time.sleep(1)
        
        # Test 2: Without cache
        print("\n" + "=" * 60)
        print("TEST 2: Without cache (--disable-stat-cache)")
        print("=" * 60)
        
        print(f"\n🚀 Starting filesystem (cache disabled)...")
        proc = subprocess.Popen(
            [binary, bucket, mount_point, "--disable-stat-cache", "--debug", "-f"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        time.sleep(2)
        
        # All stats should go to GCS
        print("\n[1] First stat (no cache):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        time.sleep(0.2)
        
        print("\n[2] Second stat (no cache):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        time.sleep(0.2)
        
        print("\n[3] Third stat (no cache):")
        start = time.time()
        os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Time: {elapsed:.4f}s")
        
        proc.terminate()
        proc.wait(timeout=3)
        run_cmd(f"fusermount -u {mount_point}", check=False)
        time.sleep(1)
        
        # Test 3: Multiple consecutive stats benchmark
        print("\n" + "=" * 60)
        print("TEST 3: Performance benchmark (10 consecutive stats)")
        print("=" * 60)
        
        print(f"\n🚀 Starting filesystem (cache enabled)...")
        proc = subprocess.Popen(
            [binary, bucket, mount_point, "--stat-cache-ttl=60", "-f"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        time.sleep(2)
        
        print("\nWarming up cache...")
        os.stat(file_path)
        time.sleep(0.2)
        
        print("Running 10 consecutive stats (all should be cached):")
        start = time.time()
        for i in range(10):
            os.stat(file_path)
        elapsed = time.time() - start
        print(f"    Total time: {elapsed:.4f}s")
        print(f"    Avg per stat: {elapsed/10:.4f}s")
        
        proc.terminate()
        proc.wait(timeout=3)
        
        print("\n" + "=" * 60)
        print("✓ ALL TESTS COMPLETE")
        print("=" * 60)
        print("\n📊 Expected behavior:")
        print("  • Test 1: Stats #2-3 and #5 should be very fast (<0.001s)")
        print("           Stats #1 and #4 should be slower (GCS fetch)")
        print("  • Test 2: All stats should be slow (no cache)")
        print("  • Test 3: Cached stats should be <0.01s total for 10 operations")
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        cleanup(mount_point, test_file, bucket)
        print(f"\n🧹 Cleanup complete")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 test_cache_ttl.py <bucket_name>")
        sys.exit(1)
    
    bucket = sys.argv[1]
    test_cache_ttl(bucket)
