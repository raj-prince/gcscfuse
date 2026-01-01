#!/bin/bash
set -e

MOUNT_POINT=$(mktemp -d)
echo "Mount point: $MOUNT_POINT"

# Start gcscfuse with dummy reader
cd /home/princer_google_com/dev/gcscfuse/build
./gcscfuse --enable-dummy-reader --debug --disable-stat-cache princer-working-dirs $MOUNT_POINT
sleep 5

# List directory first
echo "Directory listing:"
ls -lh $MOUNT_POINT/ | head -5

# Write a test file (to create it in write buffer)
echo "Creating a test file..."
echo "test data" > $MOUNT_POINT/dummy-test-file.txt

# Read it back
echo "Reading back the file..."
cat $MOUNT_POINT/dummy-test-file.txt

# Cleanup
fusermount -u $MOUNT_POINT
rmdir $MOUNT_POINT
echo "✅ Test complete - dummy reader is working!"
