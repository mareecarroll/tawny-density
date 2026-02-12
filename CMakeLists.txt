cmake_minimum_required(VERSION 3.14)
project(tawny_density CXX)

set(CMAKE_CXX_STANDARD 17)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ------------------------------------------------------------------------------
# Library target (shared code)
# ------------------------------------------------------------------------------
add_library(tawny_density_lib
    tawny_density/observations.cpp
    tawny_density/suburb.cpp
)

target_include_directories(tawny_density_lib
    PUBLIC
        tawny_density
)

target_link_libraries(tawny_density_lib
    PUBLIC
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# ------------------------------------------------------------------------------
# Main executable
# ------------------------------------------------------------------------------
add_executable(tawny_density
    tawny_density/main.cpp
)

target_link_libraries(tawny_density
    PRIVATE
        tawny_density_lib
)

# ------------------------------------------------------------------------------
# Testing (doctest)
# ------------------------------------------------------------------------------
include(CTest)

add_executable(tawny_density_tests
    tests/test_observations.cpp
    tests/fake_http_client.hpp
)

target_include_directories(tawny_density_tests
    PRIVATE
        tests
        tawny_density
)

target_link_libraries(tawny_density_tests
    PRIVATE
        tawny_density_lib
)

add_test(NAME tawny_density_tests COMMAND tawny_density_tests)

# ------------------------------------------------------------------------------
# Coverage support
# ------------------------------------------------------------------------------
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Building with code coverage enabled")

    # GCC/Clang coverage flags
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)

    # Coverage target using gcovr
    find_program(GCOVR gcovr)

    if(GCOVR)
        add_custom_target(coverage
            COMMAND ${GCOVR}
                -r ${CMAKE_SOURCE_DIR}
                --html-details coverage.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report"
        )
    else()
        message(WARNING "gcovr not found — coverage target will not be available")
    endif()
endif()
