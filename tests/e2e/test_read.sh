#!/bin/bash

# E2E test for basic read operations.

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== E2E Read Test ===${NC}"

# --- Configuration ---
BUCKET="${BUCKET:-princer-working-dirs}"
MOUNT_POINT=$(mktemp -d)
BINARY="./gcscfuse"
TEST_FILE="gcscfuse-read-test-$(date +%s).txt"
TEST_CONTENT="Hello GCSFS! This is a read test. Random data: $(head -c 32 /dev/urandom | base64)"

echo -e "${YELLOW}Using bucket:${NC} $BUCKET"
echo -e "${YELLOW}Mount point:${NC} $MOUNT_POINT"
echo -e "${YELLOW}Test file:${NC} $TEST_FILE"

# --- Cleanup function ---
cleanup() {
    echo -e "\n${YELLOW}--- Cleaning up ---${NC}"
    # Unmount
    if mount | grep -q "$MOUNT_POINT"; then
        fusermount -u "$MOUNT_POINT" 2>/dev/null || true
    fi
    
    # Remove mount point directory
    if [ -d "$MOUNT_POINT" ]; then
        rmdir "$MOUNT_POINT"
    fi

    # Delete test file from GCS
    echo "Deleting gs://$BUCKET/$TEST_FILE from GCS..."
    gcloud storage rm "gs://$BUCKET/$TEST_FILE" --quiet 2>/dev/null || echo "GCS file deletion failed."
    
    echo -e "${YELLOW}--- Cleanup complete ---${NC}"
}

# Trap EXIT signal to ensure cleanup runs
trap cleanup EXIT

# --- Pre-Test Setup ---

# Check for gcloud
if ! command -v gcloud &> /dev/null; then
    echo -e "${RED}Error: gcloud command not found. Please install and configure Google Cloud SDK.${NC}"
    exit 1
fi

# Create and upload test file
echo -e "\n${YELLOW}Creating and uploading test file to GCS...${NC}"
echo -n "$TEST_CONTENT" | gcloud storage cp - "gs://$BUCKET/$TEST_FILE"

# --- Test 1: With Cache Enabled ---
echo -e "\n${GREEN}=== Test 1: With Cache Enabled ===${NC}"
echo "Mounting gcscfuse with default cache settings..."
$BINARY "$BUCKET" "$MOUNT_POINT" --debug -f &
GCSFUSE_PID=$!
sleep 5 # Give it a moment to mount

# Read the file and verify content
echo "Reading file from mount point (with cache)..."
READ_CONTENT=$(cat "$MOUNT_POINT/$TEST_FILE")

if [ "$READ_CONTENT" == "$TEST_CONTENT" ]; then
    echo -e "${GREEN}✅ Success: Read content matches original content.${NC}"
else
    echo -e "${RED}❌ Failure: Read content does not match.${NC}"
    echo "Original: '$TEST_CONTENT'"
    echo "Read:     '$READ_CONTENT'"
    exit 1
fi

# Unmount
fusermount -u "$MOUNT_POINT"
wait $GCSFUSE_PID 2>/dev/null || true

# --- Test 2: With Cache Disabled ---
echo -e "\n${GREEN}=== Test 2: With Cache Disabled ===${NC}"
echo "Mounting gcscfuse with caches disabled..."
# Mount with caches disabled
$BINARY "$BUCKET" "$MOUNT_POINT" --debug --disable-stat-cache --disable-file-content-cache -f &
GCSFUSE_PID=$!
sleep 5

echo "Reading file from mount point (no cache)..."
READ_CONTENT_NOCACHE=$(cat "$MOUNT_POINT/$TEST_FILE")

if [ "$READ_CONTENT_NOCACHE" == "$TEST_CONTENT" ]; then
    echo -e "${GREEN}✅ Success: Read content matches original content (no cache).${NC}"
else
    echo -e "${RED}❌ Failure: Read content does not match (no cache).${NC}"
    echo "Original: '$TEST_CONTENT'"
    echo "Read:     '$READ_CONTENT_NOCACHE'"
    exit 1
fi

echo -e "\n${GREEN}--- E2E Read Test Passed ---${NC}"