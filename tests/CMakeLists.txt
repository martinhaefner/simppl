cmake_minimum_required(VERSION 2.8)

include_directories(. ../include)

  
add_subdirectory(${GTEST_ROOT} gtest)

enable_testing()

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})


add_executable(unittests 
   ipc2_timeout.cpp 
   ipc2_simple.cpp
   ipc2_errors.cpp
   ipc2_async_server.cpp
)

target_link_libraries(unittests gtest gtest_main simppl rt dbus-1)

# This is so you can do 'make test' to see all your tests run, instead of
# manually running the executable runUnitTests to see those specific tests.
add_test(name "simpple unit tests" command unittests)
