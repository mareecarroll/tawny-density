cmake_minimum_required(VERSION 3.14)
project(tawny_density CXX)

set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ------------------------------------------------------------------------------
# Main executable
# ------------------------------------------------------------------------------
add_executable(tawny_density
    tawny_density/main.cpp
    tawny_density/observations.cpp
    tawny_density/suburb.cpp
)

target_link_libraries(tawny_density
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# ------------------------------------------------------------------------------
# Tests (doctest)
# ------------------------------------------------------------------------------

# Enable testing
include(CTest)

# Add the test executable
add_executable(tawny_density_tests
    tests/test_observations.cpp
    tawny_density/observations.cpp
    tawny_density/suburb.cpp
)

target_include_directories(tawny_density_tests
    PRIVATE
        tests        # so <doctest/doctest.h> resolves
)

# Link dependencies into the test binary
target_link_libraries(tawny_density_tests
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# Register the test with CTest
add_test(NAME tawny_density_tests COMMAND tawny_density_tests)
