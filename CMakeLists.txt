# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(tawny_top_suburb CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(tawny_top_suburb main.cpp)
target_link_libraries(tawny_top_suburb PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
