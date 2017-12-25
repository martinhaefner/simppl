
include_directories(. ../include)


add_executable(echo
    echo/main.cpp
)

target_link_libraries(echo simppl)


add_executable(mixed 
	mixed/mixed.cpp
)

target_link_libraries(mixed simppl)
