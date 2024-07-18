
# echo example
add_executable(echo
    echo/main.cpp
)

target_link_libraries(echo simppl)


# multi interface example
add_executable(mixed
    mixed/mixed.cpp
)

target_link_libraries(mixed simppl)


# networkmanager example
add_executable(networkmgr
       networkmanager/networkmanager.cpp
)

target_link_libraries(networkmgr simppl)


# wpa supplicant example
add_executable(wpa
       wpa_supplicant/wpa.cpp
)

target_link_libraries(wpa simppl)


# boost::asio event loop integration example
add_executable(boost_executor
    asio/dbusexecutor.cpp
    asio/main.cpp
)

target_link_libraries(boost_executor simppl)
