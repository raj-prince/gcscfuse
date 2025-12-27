# GCSFS - Google Cloud Storage FUSE Filesystem
# 
# PREREQUISITES:
# 1. Install system dependencies:
#    sudo apt-get update && sudo apt-get install -y \
#        build-essential cmake pkg-config git \
#        libfuse3-dev libgtest-dev curl zip unzip tar
#
# 2. Install vcpkg (package manager for C++ dependencies):
#    git clone https://github.com/microsoft/vcpkg $HOME/vcpkg
#    $HOME/vcpkg/bootstrap-vcpkg.sh
#
# 3. Install google-cloud-cpp via vcpkg (first time takes 15-20 minutes):
#    $HOME/vcpkg/vcpkg install google-cloud-cpp[core,storage]
#
# BUILD INSTRUCTIONS:
#    cd /path/to/gcscfuse
#    mkdir -p build && cd build
#    cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
#    make build      # Build the binary
#    make test       # Run unit tests
#
# RUN INSTRUCTIONS:
#    export GOOGLE_APPLICATION_CREDENTIALS=/path/to/service-account-key.json
#    mkdir -p ~/gcs
#    ./build/gcscfuse <bucket-name> ~/gcs
#
# UNMOUNT:
#    umount ~/gcs
#
cmake_minimum_required(VERSION 3.22)
project(gcscfuse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find FUSE3
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

# Find google-cloud-cpp (vcpkg will provide this)
find_package(google_cloud_cpp_storage REQUIRED)

# Find Google Test and GMock
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Try pkg-config as fallback
    pkg_check_modules(GTEST gtest)
    pkg_check_modules(GTEST_MAIN gtest_main)
    pkg_check_modules(GMOCK gmock)
endif()

# Include directories
include_directories(src)
include_directories(${FUSE_INCLUDE_DIRS})

link_directories(${FUSE_LIBRARY_DIRS})
set(LIBS ${LIBS} ${FUSE_LIBRARIES})

# Main executable
add_executable(gcscfuse
        src/main.cpp
        src/gcs_fs.cpp
        src/gcs_fs.hpp
        src/stat_cache.cpp
        src/stat_cache.hpp
        src/config.cpp
        src/config.hpp
        src/fuse_cpp_wrapper.hpp
        src/gcs/gcs_client.cpp
        src/gcs/gcs_client.hpp
        src/gcs/gcs_sdk_interface.cpp
        src/gcs/gcs_sdk_interface.hpp
)

target_link_libraries(gcscfuse 
    ${LIBS} 
    google-cloud-cpp::storage
)

# Create 'build' target as alias for main binary
add_custom_target(build DEPENDS gcscfuse)

# Unit tests
if(GTest_FOUND OR GTEST_FOUND)
    add_executable(run_tests
        src/stat_cache_test.cpp
        src/stat_cache.cpp
        src/stat_cache.hpp
    )
    
    add_executable(run_gcs_client_tests
        src/gcs/gcs_client_test.cpp
        src/gcs/gcs_client.cpp
        src/gcs/gcs_client.hpp
        src/gcs/gcs_sdk_interface.cpp
        src/gcs/gcs_sdk_interface.hpp
    )
    
    if(GTest_FOUND)
        target_link_libraries(run_tests 
            GTest::gtest 
            GTest::gtest_main
            pthread
        )
        target_link_libraries(run_gcs_client_tests
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            google-cloud-cpp::storage
            pthread
        )
    else()
        target_include_directories(run_tests PRIVATE ${GTEST_INCLUDE_DIRS})
        target_link_libraries(run_tests 
            ${GTEST_LIBRARIES}
            ${GTEST_MAIN_LIBRARIES}
            pthread
        )
        target_include_directories(run_gcs_client_tests PRIVATE ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})
        target_link_libraries(run_gcs_client_tests
            ${GTEST_LIBRARIES}
            ${GTEST_MAIN_LIBRARIES}
            ${GMOCK_LIBRARIES}
            google-cloud-cpp::storage
            pthread
        )
    endif()
    
    # Add tests to CTest
    add_test(NAME stat_cache_tests COMMAND run_tests)
    add_test(NAME gcs_client_tests COMMAND run_gcs_client_tests)
    
    # Make sure tests are built before running 'make test'
    add_custom_target(check 
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS run_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running tests..."
    )
    
    message(STATUS "Google Test found - unit tests enabled")
    message(STATUS "Use 'make build' to build the binary")
    message(STATUS "Use 'make test' to run tests (builds tests automatically)")
    message(STATUS "Use 'make check' for verbose test output")
else()
    message(WARNING "Google Test not found - unit tests disabled")
    message(WARNING "Install with: sudo apt-get install -y libgtest-dev")
endif()

# E2E Tests
add_custom_target(e2e
    COMMAND ${CMAKE_COMMAND} -E env BUCKET=${BUCKET} bash ${CMAKE_SOURCE_DIR}/tests/e2e/test_stat_cache_ttl.sh
    DEPENDS gcscfuse
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running E2E tests..."
)

# Performance Tests
add_custom_target(perf
    COMMAND ${CMAKE_COMMAND} -E env BUCKET=${BUCKET} bash ${CMAKE_SOURCE_DIR}/tests/perf/fio_benchmark.sh
    DEPENDS gcscfuse
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running performance tests..."
)

message(STATUS "Use 'make e2e' to run E2E tests (default: princer-working-dirs)")
message(STATUS "Use 'make e2e BUCKET=<name>' to test with a different bucket")
message(STATUS "Use 'make perf' to run performance tests (default: princer-working-dirs)")
message(STATUS "Use 'make perf BUCKET=<name>' to test with a different bucket")
