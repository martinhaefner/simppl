add_library(simppl
   callstate.cpp
   dispatcher.cpp 
   error.cpp 
   serverresponseholder.cpp 
   skeletonbase.cpp 
   serverside.cpp
   stubbase.cpp
   serialization.cpp
   #clientresponseholder.cpp
   timeout.cpp
   util.cpp
   serverrequestdescriptor.cpp)


install(TARGETS simppl DESTINATION lib)
