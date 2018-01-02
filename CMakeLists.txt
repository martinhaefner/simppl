cmake_minimum_required (VERSION 2.8)

project (simppl)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

#set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -pedantic -std=c++14 -pthread")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -Wall -pedantic -std=c++14 -pthread")

# enable threading
#set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
#set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#find_package(Threads REQUIRED)


# enable introspection by default
OPTION(HAVE_INTROSPECTION "Have introspection" ON)

if(HAVE_INTROSPECTION)
   add_definitions(-DSIMPPL_HAVE_INTROSPECTION=1)
else()
   add_definitions(-DSIMPPL_HAVE_INTROSPECTION=0)
endif()


include_directories(include)

# boost fusion available?
find_path(BOOST_INCLUDE_DIR
     boost/fusion/adapted.hpp
     PATHS /usr/include /usr/local/include
     ONLY_CMAKE_FIND_ROOT_PATH
     NO_DEFAULT_PATH
)

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

find_path(DBUS_LIBDIR
    libdbus-1.so
    PATHS /usr/lib /usr/lib/* /usr/local/lib /usr/local/lib/*
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

if(DBUS_LIBDIR)
    message("Found DBus library in '${DBUS_LIBDIR}'")
    link_directories(${DBUS_LIBDIR})
else()
    message(FATAL_ERROR "Didn't find dbus-1 shared library")
endif()


# gtest environment
if(NOT GTEST_ROOT)
   message("Searching for gtest installation...")

   find_path(GTEST_ROOT
       include/gtest/gtest.h
       PATHS $ENV{HOME}/*
       PATHS /usr/share/*
       ONLY_CMAKE_FIND_ROOT_PATH
       NO_DEFAULT_PATH
   )
endif()


if(BOOST_INCLUDE_DIR AND GTEST_ROOT)
   message("Building fusion tests")
   add_definitions(-DSIMPPL_HAVE_BOOST_FUSION=1)
else()
   add_definitions(-DSIMPPL_HAVE_BOOST_FUSION=0)
endif()


add_subdirectory(src)
add_subdirectory(examples)


# having tests?
if(GTEST_ROOT)
    message("Building tests")
    add_subdirectory(tests)
else()
    message("Omitting tests!")
endif()


install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/simppl DESTINATION include)
