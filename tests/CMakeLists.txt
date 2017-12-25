#cmake_minimum_required(VERSION 3.0)

include_directories(. ../include)

add_subdirectory(${GTEST_ROOT} gtest)

enable_testing()

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})


add_executable(unittests
   timeout.cpp
   simple.cpp
   errors.cpp
   async_server.cpp
   properties.cpp
   variant.cpp
   buffer.cpp
   serialization.cpp
)

# This is so you can do 'make test' to see all your tests run, instead of
# manually running the executable runUnitTests to see those specific tests.
#add_test(name "simpple unit tests" command unittests)

target_link_libraries(unittests gtest gtest_main simppl rt)


# not a test, just an introspection example  
add_executable(introserver
   introserver.cpp
)


target_link_libraries(introserver simppl rt)
