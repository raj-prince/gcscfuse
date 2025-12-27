cmake_minimum_required(VERSION 3.10)
project(gcsfs_unit_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Google Test - use pkg-config if find_package doesn't work
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(GTEST gtest_main gtest)
endif()

if(NOT GTEST_FOUND)
    # Fallback to manual include
    set(GTEST_INCLUDE_DIRS /usr/include)
    set(GTEST_LIBRARIES gtest gtest_main)
endif()

include_directories(${GTEST_INCLUDE_DIRS})

# Include parent directories for source files
include_directories(../..)

# Add stat_cache_test executable
add_executable(stat_cache_test
    stat_cache_test.cpp
    ../../stat_cache.cpp
)

# Link Google Test
target_link_libraries(stat_cache_test
    ${GTEST_LIBRARIES}
    pthread
)

# Enable testing
enable_testing()

# Add tests to CTest
add_test(NAME StatCacheTest COMMAND stat_cache_test)

# Custom target to run tests
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS stat_cache_test
)

# Print success message
message(STATUS "Unit tests configured successfully")
message(STATUS "Build with: make")
message(STATUS "Run tests with: ctest or ./stat_cache_test")
