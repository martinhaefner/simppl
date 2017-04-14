
include_directories(. ../include)


add_executable(echo
    echo/main.cpp
)

target_link_libraries(echo simppl dbus-1)
