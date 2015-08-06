add_library(simppl dispatcher.cpp 
   error.cpp 
   serverresponseholder.cpp 
   serversignalbase.cpp 
   sessiondata.cpp 
   signalrecipient.cpp 
   skeletonbase.cpp 
   stubbase.cpp
   serialization.cpp
   clientresponseholder.cpp
   timeout.cpp
   brokerclient.cpp
   serverrequestdescriptor.cpp)

add_executable(scat scat.cpp)
target_link_libraries(scat simppl)

add_executable(sbroker sbroker.cpp)
target_link_libraries(sbroker simppl)
