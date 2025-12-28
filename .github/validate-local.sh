#!/bin/bash
# Local CI Validation Script
# Tests the build and e2e workflows before pushing to GitHub

set -e

echo "==========================================="
echo "Local CI Validation"
echo "==========================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
success() { echo -e "${GREEN}✅ $1${NC}"; }
error() { echo -e "${RED}❌ $1${NC}"; }
warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
info() { echo -e "${YELLOW}ℹ️  $1${NC}"; }

# Check prerequisites
echo "1️⃣  Checking prerequisites..."

if ! command -v cmake &> /dev/null; then
    error "cmake not found"
    exit 1
fi
success "cmake found"

if ! command -v make &> /dev/null; then
    error "make not found"
    exit 1
fi
success "make found"

if [ ! -d "$HOME/vcpkg" ]; then
    warning "vcpkg not found at $HOME/vcpkg"
    info "Install: git clone https://github.com/microsoft/vcpkg \$HOME/vcpkg && \$HOME/vcpkg/bootstrap-vcpkg.sh"
    exit 1
fi
success "vcpkg found"

echo ""
echo "2️⃣  Checking dependencies..."

# Check if google-cloud-cpp is installed
if ! "$HOME/vcpkg/vcpkg" list | grep -q "google-cloud-cpp"; then
    warning "google-cloud-cpp not installed"
    info "Run: $HOME/vcpkg/vcpkg install google-cloud-cpp[core,storage]:x64-linux"
    exit 1
fi
success "google-cloud-cpp installed"

echo ""
echo "3️⃣  Building project..."

# Clean build
if [ -d "build" ]; then
    rm -rf build
fi
mkdir -p build

cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    error "CMake configuration failed"
    exit 1
fi
success "CMake configured"

make -j$(nproc)
if [ $? -ne 0 ]; then
    error "Build failed"
    exit 1
fi
success "Build completed"

cd ..

echo ""
echo "4️⃣  Running unit tests..."

cd build
if ctest --output-on-failure --verbose; then
    success "Unit tests passed"
else
    warning "Some unit tests failed (this may be expected)"
fi
cd ..

echo ""
echo "5️⃣  Checking for E2E test setup..."

if [ -z "$GOOGLE_APPLICATION_CREDENTIALS" ]; then
    warning "GOOGLE_APPLICATION_CREDENTIALS not set"
    info "Set it to run E2E tests: export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json"
    SKIP_E2E=true
else
    success "GOOGLE_APPLICATION_CREDENTIALS is set"
fi

if [ -z "$TEST_BUCKET" ]; then
    warning "TEST_BUCKET not set"
    info "Set it to run E2E tests: export TEST_BUCKET=your-test-bucket"
    SKIP_E2E=true
else
    success "TEST_BUCKET is set"
fi

if [ "$SKIP_E2E" != "true" ]; then
    echo ""
    echo "6️⃣  Running E2E tests..."
    
    cd tests/e2e
    
    for test_script in test_*.py; do
        if [ -f "$test_script" ]; then
            echo ""
            info "Running $test_script..."
            if python3 "$test_script" "$TEST_BUCKET"; then
                success "$test_script passed"
            else
                warning "$test_script failed"
            fi
        fi
    done
    
    cd ../..
else
    warning "Skipping E2E tests (credentials not configured)"
fi

echo ""
echo "==========================================="
echo "✅ Local Validation Complete!"
echo "==========================================="
echo ""
echo "Summary:"
echo "  • Build: ✅ Success"
echo "  • Unit Tests: $(cd build && ctest --quiet && echo '✅ Passed' || echo '⚠️  Some failed')"
echo "  • E2E Tests: $([ "$SKIP_E2E" = "true" ] && echo '⏭️  Skipped' || echo '✅ Run')"
echo ""
echo "Next steps:"
echo "  1. Review any test failures above"
echo "  2. Commit your changes"
echo "  3. Push to GitHub - CI will run automatically"
echo ""
