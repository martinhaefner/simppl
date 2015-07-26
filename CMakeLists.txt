cmake_minimum_required (VERSION 2.6)
project (simppl)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_FLAGS "-std=c++0x -pthread")
include_directories(include)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
