#ifndef SIMPPL_SKELETONBASE_H
#define SIMPPL_SKELETONBASE_H


#include <tuple>

#include "simppl/exception.h"
#include "simppl/serverside.h"


// forward decl
struct Dispatcher;


struct SkeletonBase
{
   friend struct Dispatcher;
 
   SkeletonBase(const char* role);
   
   virtual ~SkeletonBase();
   
   Dispatcher& disp();
   
   /// only valid within request handler - must be called in order to respond to the request later in time
   ServerRequestDescriptor deferResponse();
   
   /// only valid to call within request handler
   void respondWith(ServerResponseHolder response);
   
   /// send deferred response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, ServerResponseHolder response);
   
   /// send error response - only valid to call within request handler
   void respondWith(const RuntimeError& err);
   
   /// send deferred error response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, const RuntimeError& err);
   
   const ServerRequestDescriptor& currentRequest() const;
   
   void* currentSessionData();
   void registerSession(void* data, void(*destructor)(void*));
   
   
protected:
   
   virtual bool find(uint32_t funcid, std::map<uint32_t, ServerRequestBase*>::iterator& iter) = 0;
   
   void handleRequest(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t length);
   
   /// return a session pointer and destruction function if adequate
   virtual std::tuple<void*,void(*)(void*)> clientAttached();
   
   
   const char* role_;
   Dispatcher* disp_;
   ServerRequestDescriptor current_request_;
};



#endif   // SIMPPL_SKELETONBASE_H