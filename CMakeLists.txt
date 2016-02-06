cmake_minimum_required (VERSION 2.6)
project (simppl)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_FLAGS "-std=c++1y -g -pthread")
include_directories(include)
include_directories(/usr/include/dbus-1.0)
include_directories(/usr/lib/arm-linux-gnueabihf/dbus-1.0/include)

add_subdirectory(src)
add_subdirectory(tests)
