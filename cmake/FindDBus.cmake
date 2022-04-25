# - Try to find DBus
# Once done, this will define
#
#  DBUS_FOUND - system has DBus
#  DBUS_INCLUDE_DIRS - the DBus include directories
#  DBUS_LIBRARIES - link these to use DBus
#
#  DBUS::DBUS - IMPORTED target for cmake
#
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)
pkg_check_modules(PC_DBUS QUIET dbus-1)

find_library(DBUS_LIBRARY
    NAMES dbus-1
    PATHS /usr/lib /usr/lib/* /usr/local/lib /usr/local/lib/*
)

find_path(DBUS_INCLUDE_DIR
    dbus/dbus-bus.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES dbus-1.0
    ONLY_CMAKE_FIND_ROOT_PATH
    NO_DEFAULT_PATH
)

find_path(DBUS_ARCH_INCLUDE_DIR
    dbus/dbus-arch-deps.h
    PATHS /usr/lib /usr/lib/* /usr/local/lib /usr/local/lib/*
    PATH_SUFFIXES dbus-1.0/include
    ONLY_CMAKE_FIND_ROOT_PATH
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DBUS REQUIRED_VARS
    DBUS_INCLUDE_DIR
    DBUS_ARCH_INCLUDE_DIR
    DBUS_LIBRARY
)

# Copy the results to the output variables and target
if(DBUS_FOUND)
    set(DBUS_LIBRARIES ${DBUS_LIBRARY})
    set(DBUS_INCLUDE_DIRS ${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})

    if(NOT TARGET DBUS::DBUS)
        add_library(DBUS::DBUS UNKNOWN IMPORTED)
        set_target_properties(DBUS::DBUS PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${DBUS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${DBUS_INCLUDE_DIRS}")
    endif()
endif()

mark_as_advanced(DBUS_INCLUDE_DIR DBUS_LIBRARY)
