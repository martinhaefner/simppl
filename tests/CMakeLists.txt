# Download and unpack googletest at configure time
configure_file(${PROJECT_SOURCE_DIR}/cmake/GoogleTest-CMakeLists.txt.in
               ${PROJECT_BINARY_DIR}/googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download)
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download)
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${PROJECT_BINARY_DIR}/googletest-src
                 ${PROJECT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)


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
if(BOOST_FOUND)
   target_compile_definitions(unittests PRIVATE SIMPPL_HAVE_BOOST_FUSION=1)
endif()
target_link_libraries(unittests gtest gtest_main simppl rt)

# This is so you can do 'make test' to see all your tests run, instead of
# manually running the executable runUnitTests to see those specific tests.
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
   add_test(NAME simppl_unittests
      COMMAND ${DBUS_RUN_SESSION} ${CMAKE_CURRENT_BINARY_DIR}/unittests
   )
endif()


# not a test, just an introspection example  
add_executable(introserver
   introserver.cpp
)
if(BOOST_FOUND)
   target_compile_definitions(introserver PRIVATE SIMPPL_HAVE_BOOST_FUSION=1)
endif()
target_link_libraries(introserver simppl rt)
