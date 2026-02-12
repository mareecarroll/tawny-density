# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(tawny_density CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(tawny_density main.cpp)
target_link_libraries(tawny_density PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
