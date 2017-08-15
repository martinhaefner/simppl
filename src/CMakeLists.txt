add_definitions(-DSIMPPL_HAVE_INTROSPECTION=1)


add_library(simppl SHARED
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

target_link_libraries(simppl dbus-1)

install(TARGETS simppl DESTINATION lib)
