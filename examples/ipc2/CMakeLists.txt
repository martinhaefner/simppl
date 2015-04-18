add_executable(ipc2 ipc2.cpp)
target_link_libraries(ipc2 simppl)

add_executable(ipc2_e ipc2_e.cpp)
target_link_libraries(ipc2_e simppl)

add_executable(session session.cpp)
target_link_libraries(session simppl)

add_executable(attributes attributes.cpp)
target_link_libraries(attributes simppl)

add_executable(except except.cpp)
target_link_libraries(except simppl)

add_executable(calculator_cpu calculator_cpu.cpp)
target_link_libraries(calculator_cpu simppl)

add_executable(calculator_clnt calculator_clnt.cpp)
target_link_libraries(calculator_clnt simppl)

add_executable(calculator_clnt1 calculator_clnt1.cpp)
target_link_libraries(calculator_clnt1 simppl)

add_executable(scat scat.cpp)
target_link_libraries(scat simppl)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark simppl)

add_executable(sbroker sbroker.cpp)
target_link_libraries(sbroker simppl)
