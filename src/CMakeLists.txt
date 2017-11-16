add_library(simppl SHARED
   #basicinterface.cpp
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
   serverrequestdescriptor.cpp
   pendingcall.cpp)


install(TARGETS simppl DESTINATION lib)
