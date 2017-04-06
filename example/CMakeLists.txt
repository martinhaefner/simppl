cmake_minimum_required(VERSION 3.0)

include_directories(. ../include)


add_executable(echoExample
        EchoExample/EchoExample.cpp
        )
target_link_libraries(echoExample simppl dbus-1)
