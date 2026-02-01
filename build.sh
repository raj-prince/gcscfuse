#!/bin/bash
# Build script for gcscfuse with gRPC support
# Handles OpenTelemetry workaround automatically

set -e

cd "$(dirname "$0")"
BUILD_DIR="build"

echo "=== Building gcscfuse with gRPC support ==="
echo ""

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with vcpkg
echo "→ Configuring CMake with vcpkg toolchain..."
cmake -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake .. 

# Fix OpenTelemetry dependency issue
CONFIG_FILE="vcpkg_installed/x64-linux/share/google_cloud_cpp_storage_grpc/google_cloud_cpp_storage_grpc-config.cmake"
if [ -f "$CONFIG_FILE" ]; then
    if grep -q "find_dependency(google_cloud_cpp_opentelemetry)" "$CONFIG_FILE"; then
        echo "→ Removing OpenTelemetry dependency from config..."
        sed -i '/find_dependency(google_cloud_cpp_opentelemetry)/d' "$CONFIG_FILE"
        echo "✓ OpenTelemetry dependency removed"
        
        # Reconfigure after fix
        echo "→ Reconfiguring CMake..."
        cmake -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake ..
    else
        echo "✓ No OpenTelemetry dependency found"
    fi
fi

# Build
echo "→ Building gcscfuse..."
make -j$(nproc) gcscfuse

echo ""
echo "✓ Build complete!"
echo ""
echo "Binary: $PWD/gcscfuse"
echo ""
echo "Usage:"
echo "  # JSON/REST protocol (default):"
echo "  ./build/gcscfuse princer-working-dirs ~/gcs -f"
echo ""
echo "  # gRPC protocol:"
echo "  ./build/gcscfuse --protocol=grpc princer-working-dirs ~/gcs -f"
