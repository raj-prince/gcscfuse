cmake_minimum_required(VERSION 3.22)
project(hello_fs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(../..)
include_directories(${FUSE_INCLUDE_DIRS})

link_directories(${FUSE_LIBRARY_DIRS})
set(LIBS ${LIBS} ${FUSE_LIBRARIES})

add_executable(hello_fs
        hello.cpp
        hello_fs.cpp
        hello_fs.hpp
)

target_link_libraries(hello_fs ${LIBS})
