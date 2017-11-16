add_library(simppl SHARED
   callstate.cpp
   dispatcher.cpp 
   error.cpp 
   serverresponseholder.cpp 
   skeletonbase.cpp 
   serverside.cpp
   stubbase.cpp
   serialization.cpp
   timeout.cpp
   util.cpp
   serverrequestdescriptor.cpp
   pendingcall.cpp)


install(TARGETS simppl DESTINATION lib)
