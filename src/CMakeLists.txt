add_library(simppl SHARED
   callstate.cpp
   dispatcher.cpp 
   error.cpp 
   serverresponseholder.cpp 
   skeletonbase.cpp 
   serverside.cpp
   stubbase.cpp
   timeout.cpp
   util.cpp
   serverrequestdescriptor.cpp
   pendingcall.cpp
   string.cpp
   wstring.cpp
   objectpath.cpp
   filedescriptor.cpp
   clientside.cpp
   serialization.cpp
   bool.cpp
)

target_link_libraries(simppl dbus-1)

install(TARGETS simppl DESTINATION lib)
