cmake_minimum_required (VERSION 3.0)

project (simppl)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic -pthread")

OPTION(HAVE_INTROSPECTION "Have introspection" ON) # Enabled by default

IF(HAVE_INTROSPECTION)
    ADD_DEFINITIONS(-DSIMPPL_HAVE_INTROSPECTION)
ENDIF(HAVE_INTROSPECTION)

include_directories(include)

# DBus includes
find_path(DBUS_INCLUDE_DIR
    dbus/dbus-bus.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES dbus-1.0
    ONLY_CMAKE_FIND_ROOT_PATH
    NO_DEFAULT_PATH
)

find_path(DBUS_ARCHDEPS_INCLUDE_DIR
    dbus/dbus-arch-deps.h
    PATHS /usr/lib /usr/lib/* /usr/local/lib /usr/local/lib/*
    PATH_SUFFIXES dbus-1.0/include
    ONLY_CMAKE_FIND_ROOT_PATH
    NO_DEFAULT_PATH
)

if(DBUS_INCLUDE_DIR)
    message("Found DBus include dir under '${DBUS_INCLUDE_DIR}'")
    include_directories(${DBUS_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Didn't find dbus directory")
endif()

if(DBUS_ARCHDEPS_INCLUDE_DIR)
    message("Found DBus arch-deps include dir under '${DBUS_ARCHDEPS_INCLUDE_DIR}'")
    include_directories(${DBUS_ARCHDEPS_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Didn't find dbus arch-deps directory")
endif()

add_subdirectory(src)
add_subdirectory(example)

# gtest environment 
find_package(GTest)

# having tests?
if(GTEST_FOUND)
    message("Building tests")
    add_subdirectory(tests)
else()
    message("Omitting tests!")
endif()


install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/simppl DESTINATION include)
