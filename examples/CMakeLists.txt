add_executable(echo
    echo/main.cpp
)

target_link_libraries(echo simppl)


add_executable(mixed 
	mixed/mixed.cpp
)

target_link_libraries(mixed simppl)


add_executable(networkmgr
       networkmanager/networkmanager.cpp
)

target_link_libraries(networkmgr simppl)

add_executable(wpa
       wpa_supplicant/wpa.cpp
)

target_link_libraries(wpa simppl)
