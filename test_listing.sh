#!/bin/bash
cd /home/princer_google_com/dev/gcscfuse/build
./gcscfuse --debug princer-working-dirs ~/gcs -f > /tmp/gcsfuse_debug.log 2>&1 &
FUSE_PID=$!
sleep 4
echo "=== Attempting ls ==="
ls ~/gcs/ 2>&1 | tee /tmp/ls_output.log
sleep 2
echo "=== Checking debug log ==="
tail -50 /tmp/gcsfuse_debug.log
kill $FUSE_PID 2>/dev/null || true
fusermount -u ~/gcs 2>/dev/null || true
