
add_executable(calculator_cpu calculator_cpu.cpp)
target_link_libraries(calculator_cpu simppl)

add_executable(calculator_clnt calculator_clnt.cpp)
target_link_libraries(calculator_clnt simppl)
