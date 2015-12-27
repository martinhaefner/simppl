#include "simppl/dispatcher.h"

#include "simppl/detail/util.h"
#include "simppl/timeout.h"

#define SIMPPL_DISPATCHER_CPP
#include "simppl/serverside.h"
#undef SIMPPL_DISPATCHER_CPP


using namespace std::placeholders;
using namespace std::literals::chrono_literals;


namespace
{

inline
std::chrono::steady_clock::time_point
get_lookup_duetime()
{
   return std::chrono::steady_clock::now() + 5s;
}   


struct BlockingResponseHandler0
{
   inline
   BlockingResponseHandler0(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<>& r)
    : disp_(disp)
    , r_(r)
   {
      r_.handledBy(std::ref(*this));
   }
   
   inline
   void operator()(const simppl::ipc::CallState& state)
   {
      disp_.stop();
      r_.handledBy(std::nullptr_t());
            
      if (!state)
         state.throw_exception();
   }
   
   simppl::ipc::Dispatcher& disp_;
   simppl::ipc::ClientResponse<>& r_;
};

}   // namespace


// ---------------------------------------------------------------------------------------


namespace simppl
{

namespace ipc
{


Dispatcher::~Dispatcher()
{
   dbus_connection_close(conn_);
}


void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp)
{
   assert(resp.r_);
   assert(!isRunning());
   
   ClientResponse<>* r = safe_cast<ClientResponse<>*>(resp.r_);
   assert(r);
   
   BlockingResponseHandler0 handler(*this, *r);
   
//   loop();
}


void Dispatcher::stop()
{
   running_.store(false);
}


bool Dispatcher::isRunning() const
{
   return running_.load();
}


void Dispatcher::addClient(StubBase& clnt)
{
   assert(!clnt.disp_);   // don't add it twice
   
   clnt.disp_ = this;
   /*FIXME clients_.insert(std::make_pair(::fullQualifiedName(clnt), &clnt)); 
   
   if (!strcmp(clnt.boundname_, "auto:"))
   {
      assert(broker_);
      broker_->waitForService(::fullQualifiedName(clnt), std::bind(&Dispatcher::serviceReady, this, &clnt, _1, _2));
   }
   else
   {
      if (isRunning())
         connect(clnt);
   }*/
}


void Dispatcher::removeClient(StubBase& clnt)
{
/*   for(auto iter = clients_.begin(); iter != clients_.end(); ++iter)
   {
      if (&clnt == iter->second)
      {
         clients_.erase(iter);
         break;
      } 
   }*/
}


Dispatcher::Dispatcher(const char* bus_name)
 : running_(false)
 , conn_(nullptr)
 , request_timeout_(std::chrono::milliseconds::max())    // TODO move this to a config header, also other timeout defaults
{
   DBusError err;
   int ret;
   
   dbus_error_init(&err);

   conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
   assert(!dbus_error_is_set(&err));
   
   ret = dbus_bus_request_name(conn_, bus_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
   assert(!dbus_error_is_set(&err));
}


int Dispatcher::run()
{
   running_.store(true);
   
   while(dbus_connection_read_write_dispatch(conn_, -1) && running_.load());
}

}   // namespace ipc

}   // namespace simppl
