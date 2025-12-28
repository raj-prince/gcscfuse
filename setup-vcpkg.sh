#!/bin/bash
set -e

VCPKG_ROOT="${HOME}/vcpkg"

echo "=== Setting up vcpkg and dependencies ==="
echo ""

# Clone and bootstrap vcpkg if needed
if [ ! -d "${VCPKG_ROOT}" ]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg "${VCPKG_ROOT}"
    echo "Bootstrapping vcpkg..."
    "${VCPKG_ROOT}/bootstrap-vcpkg.sh"
else
    echo "vcpkg already installed at ${VCPKG_ROOT}"
fi

echo ""
echo "Installing dependencies via vcpkg (this may take 30-60 minutes on first run)..."
"${VCPKG_ROOT}/vcpkg" install google-cloud-cpp[core,storage]:x64-linux gtest:x64-linux

echo ""
echo "=== Setup complete! ==="
echo "Now run:"
echo "  cmake .. -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=${VCPKG_ROOT}/installed/x64-linux"
echo "  make build"
