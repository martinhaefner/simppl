#
# Setup the cmake file to be added with a find_package
# or an add_subdirectory.
#
cmake_minimum_required(VERSION 3.5)
project(simppl VERSION 0.4.0)

# Configuration specific to being the main project
if(PROJECT_NAME STREQUAL CMAKE_PROJECT_NAME)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SIMPPL_NOT_SUBPROJECT ON)
endif()

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# enable introspection by default
option(HAVE_INTROSPECTION "Have introspection" ON)
include(CMakeDependentOption)

option(SIMPPL_HAVE_OBJECTMANAGER "Have native objectmanager support in server objects" ON)

cmake_dependent_option(SIMPPL_BUILD_TESTS "Build tests"
    ON "SIMPPL_NOT_SUBPROJECT" OFF
)

cmake_dependent_option(SIMPPL_BUILD_EXAMPLES "Build examples"
    ON "SIMPPL_NOT_SUBPROJECT" OFF
)

if(SIMPPL_BUILD_TESTS)
    include(CTest)
endif()

# enable threading
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
find_package(Boost)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

add_library(simppl SHARED
    src/dispatcher.cpp
    src/error.cpp
    src/serverresponseholder.cpp
    src/skeletonbase.cpp
    src/serverside.cpp
    src/stubbase.cpp
    src/timeout.cpp
    src/util.cpp
    src/serverrequestdescriptor.cpp
    src/pendingcall.cpp
    src/string.cpp
    src/wstring.cpp
    src/objectpath.cpp
    src/filedescriptor.cpp
    src/clientside.cpp
    src/serialization.cpp
    src/bool.cpp
    src/holders.cpp
    src/properties.cpp
    src/objectmanagermixin.cpp
)
# Provide a namespaced alias
add_library(Simppl::simppl ALIAS simppl)

# DBus introspection support
if(HAVE_INTROSPECTION)
    target_compile_definitions(simppl PUBLIC SIMPPL_HAVE_INTROSPECTION=1)
else()
    target_compile_definitions(simppl PUBLIC SIMPPL_HAVE_INTROSPECTION=0)
endif()

# Native objectmanager support on server side. Add or remove objects to/from
# a skeleton implementing the standard DBus ObjectManager interface. The
# standard API is automatically activated just like with properties.
if(SIMPPL_HAVE_OBJECTMANAGER)
    target_compile_definitions(simppl PUBLIC SIMPPL_HAVE_OBJECTMANAGER=1)
endif()

target_include_directories(simppl
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${DBUS_INCLUDE_DIRS}
)
target_link_directories(simppl
    PUBLIC
        ${DBUS_LIBRARY_DIRS}
)
target_compile_options(simppl PRIVATE -Werror -Wall -Wno-error=deprecated-declarations)
target_link_libraries(simppl PUBLIC
    ${DBUS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Build a set of examples
if(SIMPPL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SIMPPL_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)
endif()


#
# Installation of stuff
#
if(SIMPPL_NOT_SUBPROJECT)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS simppl
        EXPORT simppl-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT simppl-targets
        FILE SimpplTargets.cmake
        NAMESPACE Simppl::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simppl
    )
    install(DIRECTORY include/simppl
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    configure_package_config_file(cmake/SimpplConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/SimpplConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simppl
    )
    write_basic_package_version_file(SimpplConfigVersion.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY
            SameMajorVersion
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SimpplConfig.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/SimpplConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simppl
    )

endif()
