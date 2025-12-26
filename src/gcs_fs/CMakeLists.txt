# GCS FUSE Filesystem CMake Build Configuration
# 
# PREREQUISITES:
# 1. Install system dependencies:
#    sudo apt-get update && sudo apt-get install -y \
#        build-essential cmake pkg-config git \
#        libfuse3-dev curl zip unzip tar
#
# 2. Install vcpkg (package manager for C++ dependencies):
#    git clone https://github.com/microsoft/vcpkg $HOME/vcpkg
#    $HOME/vcpkg/bootstrap-vcpkg.sh
#
# 3. Install google-cloud-cpp via vcpkg (first time takes 15-20 minutes):
#    $HOME/vcpkg/vcpkg install google-cloud-cpp[core,storage]
#
# BUILD INSTRUCTIONS:
#    cd /path/to/gcscfuse/src/gcs_fs
#    rm -rf build && mkdir build && cd build
#    cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
#    make -j$(nproc)
#
# RUN INSTRUCTIONS:
#    export GOOGLE_APPLICATION_CREDENTIALS=/path/to/service-account-key.json
#    mkdir -p ~/gcs
#    ./gcs_fs <bucket-name> ~/gcs
#
# UNMOUNT:
#    umount ~/gcs
#
cmake_minimum_required(VERSION 3.22)
project(gcs_fs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FUSE3
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

# Find google-cloud-cpp (vcpkg will provide this)
find_package(google_cloud_cpp_storage REQUIRED)

include_directories(.)
include_directories(../..)
include_directories(${FUSE_INCLUDE_DIRS})

link_directories(${FUSE_LIBRARY_DIRS})
set(LIBS ${LIBS} ${FUSE_LIBRARIES})

add_executable(gcs_fs
        main.cpp
        gcs_fs.cpp
        gcs_fs.hpp
)

target_link_libraries(gcs_fs 
    ${LIBS} 
    google-cloud-cpp::storage
)
