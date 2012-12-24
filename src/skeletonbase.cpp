#include "simppl/skeletonbase.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/util.h"
#include "simppl/detail/frames.h"


namespace simppl
{
   
namespace ipc
{

SkeletonBase::SkeletonBase(const char* role)
 : role_(role)
 , disp_(0)
{
   assert(role_);
}


SkeletonBase::~SkeletonBase()
{
   // NOOP
}


Dispatcher& SkeletonBase::disp()
{
   assert(disp_);
   return *disp_;
}


ServerRequestDescriptor SkeletonBase::deferResponse()
{
   assert(current_request_);
   assert(current_request_.requestor_->hasResponse());  
   
   return current_request_;   // invalidates the current request!
}


void SkeletonBase::respondWith(detail::ServerResponseHolder response)
{
   assert(current_request_);
   assert(response.responder_->allowedRequests_.find(current_request_.requestor_) != response.responder_->allowedRequests_.end());
   
   detail::ResponseFrame r(0);
   r.payloadsize_ = response.size_;
   r.sequence_nr_ = current_request_.sequence_nr_;
   
   detail::genericSend(current_request_.fd_, r, response.payload_);
   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, detail::ServerResponseHolder response)
{
   assert(req);
   assert(response.responder_->allowedRequests_.find(req.requestor_) != response.responder_->allowedRequests_.end());
   
   detail::ResponseFrame r(0);
   r.payloadsize_ = response.size_;
   r.sequence_nr_ = req.sequence_nr_;
   
   genericSend(req.fd_, r, response.payload_);
   req.clear();
}


void SkeletonBase::respondWith(const RuntimeError& err)
{
   assert(current_request_);
   assert(current_request_.requestor_->hasResponse());
   
   detail::ResponseFrame r(err.error());
   r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
   r.sequence_nr_ = current_request_.sequence_nr_;
   
   genericSend(current_request_.fd_, r, err.what());
   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, const RuntimeError& err)
{
   assert(req);
   assert(req.requestor_->hasResponse());
   
   detail::ResponseFrame r(err.error());
   r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
   r.sequence_nr_ = req.sequence_nr_;
   
   genericSend(req.fd_, r, err.what());
   req.clear();
}


const ServerRequestDescriptor& SkeletonBase::currentRequest() const
{
   assert(current_request_);
   return current_request_;
}


void* SkeletonBase::currentSessionData()
{
   return disp().getSessionData(currentRequest().sessionid_);
}


void SkeletonBase::registerSession(void* data, void(*destructor)(void*))
{
   disp().registerSession(currentRequest().fd_, currentRequest().sessionid_, data, destructor);
}


void SkeletonBase::handleRequest(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t length)
{
   //std::cout << "Skeleton::handleRequest '" << funcid << "'" << std::endl;
   std::map<uint32_t, ServerRequestBase*>::iterator iter;
   
   if (find(funcid, iter))
   {
      try
      {
         current_request_.set(iter->second, fd, sequence_nr, sessionid);
         iter->second->eval(payload, length);
         
         // current_request_ is only valid if no response handler was called
         if (current_request_)
         {
             // in that case the request must not have a reponse
            assert(!current_request_.requestor_->hasResponse());  
            current_request_.clear();
         }
      }
      catch(const RuntimeError& err)
      {
         if (current_request_.requestor_->hasResponse())
         {
            respondWith(err);
            current_request_.clear();
         }
      }
      catch(...)
      {
         if (current_request_.requestor_->hasResponse())
         {
            RuntimeError err(-1, "Unknown unhandled exception occured on server");
            respondWith(err);
         }
            
         current_request_.clear();
         throw;
      }
   }
   else
      std::cerr << "Unknown request '" << funcid << "' with payload size=" << length << std::endl;
}


std::tuple<void*,void(*)(void*)> SkeletonBase::clientAttached()
{
   return std::tuple<void*,void(*)(void*)>(nullptr, nullptr);   // the default does not create any session data
}

}   // namespace ipc

}   // namespace simppl
