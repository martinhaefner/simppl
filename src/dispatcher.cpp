#include "simppl/dispatcher.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#define SIMPPL_DISPATCHER_CPP
#include "simppl/serverside.h"
#undef SIMPPL_DISPATCHER_CPP

#include "simppl/brokerclient.h"


using namespace std::placeholders;


namespace
{

inline
std::chrono::steady_clock::time_point
get_lookup_duetime()
{
   return std::chrono::steady_clock::now() + std::chrono::seconds(5);
}   

   
void makeInetAddress(struct sockaddr_in& addr, const char* endpoint)
{
   assert(strlen(endpoint) < 64);
   
   char tmp[64];
   ::strcpy(tmp, endpoint);
   char* port = tmp + 4;
   while(*++port != ':');
   *port = '\0';
   ++port;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(::atoi(port));
   addr.sin_addr.s_addr = ::inet_addr(tmp + 4);
}


template<typename StubT>
std::string fullQualifiedName(const StubT& stub)
{
   std::string ret(stub.iface());
   ret += "::";
   ret += stub.role() ;

   return ret;
}


const char* fullQualifiedName(char* buf, const char* ifname, const char* rolename)
{
   sprintf(buf, "%s::%s", ifname, rolename);
   return buf;
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

// global
std::unique_ptr<char> NullUniquePtr;


std::string Dispatcher::fullQualifiedName(const char* ifname, const char* rolename)
{
   std::string ret(ifname);
   ret += "::";
   ret += rolename;

   return ret;
}


Dispatcher::~Dispatcher()
{
   for(auto iter = servers_.begin(); iter != servers_.end(); ++iter)
   {
      delete iter->second;
   }
   
   std::for_each(endpoints_.begin(), endpoints_.end(), [this](const std::string& ep){
      if (ep.find("unix:") == 0)
         (void)::unlink(ep.c_str()+5);
   });
   
   if (broker_)
   {
      delete broker_;
      broker_ = nullptr;
   }

   for(unsigned int i=0; i<sizeof(fds_)/sizeof(fds_[0]); ++i)
   {
      // FIXME can't check against >= 0 because then there are undesired blockings
      //       so change it all over the software before changing one position only 
      if (fds_[i].fd > 0)
         while(::close(fds_[i].fd) && errno == EINTR);
   }   
}


void Dispatcher::registerAtBroker(const std::string& service, const std::string& endpoint)
{
   if (broker_)
   {
      // FIXME must register service for multiple endpoints
      // FIXME must resolve INADDR_ANY to senseful address list for registration
      broker_->registerService(service, endpoint);
   }
}


void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp)
{
   assert(resp.r_);
   assert(!isRunning());
   
   ClientResponse<>* r = safe_cast<ClientResponse<>*>(resp.r_);
   assert(r);
   
   BlockingResponseHandler0 handler(*this, *r);
   
   loop();
}


void Dispatcher::connect(StubBase& stub, const char* location)
{
   const char* the_location = location ? location : stub.boundname_;
   
   assert(strcmp(the_location, "auto:"));

   // 1. connect the socket physically - if not yet done
   auto iter = socks_.find(the_location);
   if (iter != socks_.end())
   {
      stub.fd_ = iter->second;
   }
   else
   {
      int rc = -1;

      if (!strncmp(the_location, "unix:", 5))
      {
         stub.fd_ = ::socket(PF_UNIX, SOCK_STREAM, 0);
   
         struct sockaddr_un addr;
         addr.sun_family = AF_UNIX;
         sprintf(addr.sun_path, "/tmp/dispatcher/%s", the_location + 5);
         
         rc = ::connect(stub.fd_, (struct sockaddr*)&addr, sizeof(addr));
         if (rc < 0)
            add_inotify_location(stub, the_location + 5);
      }
      else if (!strncmp(the_location, "tcp:", 4))
      {
         stub.fd_ = ::socket(PF_INET, SOCK_STREAM, 0);
   
         struct sockaddr_in addr;
         makeInetAddress(addr, the_location);
         
         rc = ::connect(stub.fd_, (struct sockaddr*)&addr, sizeof(addr));
         
         if (rc == 0)
         {
            int on = 1;
            rc = ::setsockopt(stub.fd_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
         }
      }
      else
         return;
   
      if (rc == 0)
      {
         fds_[stub.fd_].fd = stub.fd_;
         fds_[stub.fd_].events = POLLIN;
         
         fctn_[stub.fd_] = std::bind(&Dispatcher::handle_data, this, _1, _2);
         
         socks_[the_location] = stub.fd_;
      }
      else
      {
         while(::close(stub.fd_) < 0 && errno == EINTR);
         stub.fd_ = -1;
      }
   }
   
   uint32_t seqnr;
   
   // 2. initialize interface resolution
   if (stub.fd_ >= 0)
       seqnr = send_resolve_interface(stub);
   
   if (!isRunning())
   {
      stub.connected >> std::bind(&Dispatcher::handle_blocking_connect, this, _1, &stub, seqnr);
      loop();
   }
}


void Dispatcher::handle_blocking_connect(ConnectionState s, StubBase* stub, uint32_t seqNr)
{
   stop();
   stub->connected = std::nullptr_t();
   
   if (s == ConnectionState::Timeout)
      throw TransportError(ETIMEDOUT, seqNr);
}


void Dispatcher::add_inotify_location(StubBase& stub, const char* socketpath)
{
   pending_lookups_.insert(std::make_pair(socketpath, std::make_tuple(&stub, get_lookup_duetime())));
}


void Dispatcher::handle_inotify_event(struct inotify_event* evt)
{
   auto pair = pending_lookups_.equal_range(evt->name);
   
   if (pair.first != pending_lookups_.end())
   {
      std::string location("unix:");
      location += pair.first->first;
      
      for(auto iter = pair.first; iter != pair.second; ++iter)
      {
         (void)connect(*std::get<0>(iter->second), location.c_str());
      }
   }
}
   

uint32_t Dispatcher::send_resolve_interface(StubBase& stub)
{
   // don't cache anything here, a sessionid will be 
   // created no the server for any resolve request
   detail::InterfaceResolveFrame f(42);
   char buf[128];
   
   assert(strlen(stub.iface_) + 2 + strlen(stub.role_) < sizeof(buf));
   ::fullQualifiedName(buf, stub.iface_, stub.role_);
                           
   f.payloadsize_ = strlen(buf)+1;
   f.sequence_nr_ = generateSequenceNr();

   pending_interface_resolves_[f.sequence_nr_] = std::make_tuple(&stub, get_lookup_duetime());
   genericSend(stub.fd_, f, buf);
   
   return f.sequence_nr_;
}


void Dispatcher::serviceReady(StubBase* stub, const std::string& fullName, const std::string& location)
{
   assert(::fullQualifiedName(*stub) == fullName);
   connect(*stub, location.c_str());
}


void Dispatcher::addClient(StubBase& clnt)
{
   assert(!clnt.disp_);   // don't add it twice
   
   clnt.disp_ = this;
   clients_.insert(std::make_pair(::fullQualifiedName(clnt), &clnt)); 
   
   if (!strcmp(clnt.boundname_, "auto:"))
   {
      assert(broker_);
      broker_->waitForService(::fullQualifiedName(clnt), std::bind(&Dispatcher::serviceReady, this, &clnt, _1, _2));
   }
   else
   {
      if (isRunning())
         connect(clnt);
   }
}


bool Dispatcher::isSignalRegistered(ClientSignalBase& s) const
{
   return std::find_if(sighandlers_.begin(), sighandlers_.end(), 
         [&s](const sighandlers_type::value_type& v){ return v.second == &s; 
      }) != sighandlers_.end();
}
   

bool Dispatcher::addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr)
{
   bool alreadyAttached = isSignalRegistered(s);
   
   if (!alreadyAttached)
   {
      alreadyAttached = std::find_if(outstanding_sig_registrs_.begin(), outstanding_sig_registrs_.end(), [&s](decltype(*outstanding_sig_registrs_.begin())& e){
         return e.second == &s;
      }) != outstanding_sig_registrs_.end();
      
      if (!alreadyAttached)
         outstanding_sig_registrs_[sequence_nr] = &s;
   }
   
   return !alreadyAttached;
}


int Dispatcher::loop(unsigned int timeoutMs)
{
   int retval = 0;
   running_.store(true);
   
   do
   {
      retval = once(timeoutMs);
   }
   while(running_.load());
   
   return retval;
}


uint32_t Dispatcher::removeSignalRegistration(ClientSignalBase& s)
{
   uint32_t rc = 0;
   
   for (auto iter = sighandlers_.begin(); iter != sighandlers_.end(); ++iter)
   {
      if (iter->second == &s)
      {
         rc = iter->first;
         sighandlers_.erase(iter);
         break;
      }
   }
   
   // maybe still outstanding?
   for (auto iter = outstanding_sig_registrs_.begin(); iter != outstanding_sig_registrs_.end(); ++iter)
   {
      if (iter->second == &s)
      {
         outstanding_sig_registrs_.erase(iter);
         break;
      }
   }
   
   return rc;
}


void Dispatcher::enableBrokerage()
{
   broker_ = new BrokerClient(*this);
}


int Dispatcher::accept_socket(int acceptor, short /*pollmask*/)
{
   // doesn't matter which type of socket here
   union
   {
      struct sockaddr_un u_addr;
      struct sockaddr_in i_addr;
   } u;
   
   socklen_t len = sizeof(u);

   int fd = ::accept(acceptor, (struct sockaddr*)&u, &len);
   if (fd > 0)
   {
      fds_[fd].fd = fd;
      fds_[fd].events = POLLIN;
      
      fctn_[fd] = std::bind(&Dispatcher::handle_data, this, _1, _2);
   }

   if (fd >= 0)
      socketConnected(fd);
   
   return fd;
}


int Dispatcher::handle_data(int fd, short /*pollmask*/)
{
   // ordinary stream socket
   detail::FrameHeader f;
   
   ssize_t len = ::recv(fd, &f, sizeof(f), MSG_NOSIGNAL|MSG_WAITALL|MSG_PEEK);
   
   if (len == sizeof(f) && f)
   {  
      len = 0;
      std::unique_ptr<char> buf;   // it's safe since POD array though scoped_array would be better here!
      
      union Hdr 
      {
         Hdr()
         {
            // NOOP
         }
         
         detail::RequestFrame rqf;
         detail::ResponseFrame rf;
         detail::InterfaceResolveFrame irf;
         detail::InterfaceResolveResponseFrame irrf;
         detail::RegisterSignalFrame rsf;
         detail::UnregisterSignalFrame usf;
         detail::SignalEmitFrame sef;
         detail::SignalResponseFrame srf;
         detail::TransportErrorFrame tef;
      } hdr; 

      struct msghdr msg;
      memset(&msg, 0, sizeof(msg));
      
      struct iovec v[2] = { { &hdr, detail::headersize[f.type_] }, { 0, 0 } };
      msg.msg_iov = v;
      
      if (f.payloadsize_ > 0)
      {
         msg.msg_iovlen = 2;
         
         buf.reset(new char[f.payloadsize_]);        
         
         v[1].iov_base = buf.get();
         v[1].iov_len = f.payloadsize_;
      }
      else
         msg.msg_iovlen = 1;
                                 
      len = ::recvmsg(fd, &msg, MSG_NOSIGNAL|MSG_WAITALL);

      // all data read?
      if (len == v[0].iov_len + v[1].iov_len)
      {
         switch(f.type_)
         {
         case FRAME_TYPE_REQUEST:
            { 
               auto iter = servers_by_id_.find(hdr.rqf.serverid_);
               if (iter != servers_by_id_.end())
               {
                  iter->second->eval(hdr.rqf.func_, hdr.rqf.sequence_nr_, hdr.rqf.sessionid_, fd, buf.get(), hdr.rqf.payloadsize_);
               }
               else
                  std::cerr << "No service with id=" << hdr.rqf.serverid_ << " found." << std::endl;
            }
            break;
            
         case FRAME_TYPE_TRANSPORT_ERROR:
         
            ((ClientResponseBase*)hdr.tef.handler_)->eval(CallState((TransportError*)(hdr.tef.err_)), 0, 0);
         
            break;
            
         case FRAME_TYPE_RESPONSE:
            {  
               auto iter = pendings_.find(hdr.rf.sequence_nr_);
               if (iter != pendings_.end())
               {
                  if (hdr.rf.result_ == 0)   // normal response
                  {
                     CallState cs(hdr.rf.sequence_nr_);
                     std::get<1>(iter->second)->eval(cs, buf.get(), hdr.rf.payloadsize_);
                  }
                  else   // error response
                  {
                     CallState cs(new RuntimeError(hdr.rf.result_, (const char*)buf.get(), hdr.rf.sequence_nr_));
                     std::get<1>(iter->second)->eval(cs, 0, 0);
                  }
                  
                  pendings_.erase(iter);
               }
            }
            break;
         
         case FRAME_TYPE_REGISTER_SIGNAL:
            {
               auto iter = servers_by_id_.find(hdr.rsf.serverid_);
               if (iter != servers_by_id_.end())
               {
                  uint32_t registrationid = generateId();
                  
                  detail::ServerSignalBase* sig = iter->second->addSignalRecipient(hdr.rsf.sig_, fd, registrationid, hdr.rsf.id_);
                  
                  if (sig)
                     server_sighandlers_[registrationid] = sig;
                  
                  // FIXME error handling, what if sig is 0???
                  detail::SignalResponseFrame rf(registrationid, hdr.rsf.id_);
                  rf.sequence_nr_ = hdr.rsf.sequence_nr_;
                  
                  genericSend(fd, rf, 0);
                  
                  if (sig)
                     sig->onAttach(registrationid);
               }
               else
                  std::cerr << "No server with id=" << hdr.rsf.serverid_ << " found." << std::endl;
            }
            break;
            
         case FRAME_TYPE_UNREGISTER_SIGNAL:
            {
               auto iter = server_sighandlers_.find(hdr.usf.registrationid_);
               if (iter != server_sighandlers_.end())
               {
                  iter->second->removeRecipient(hdr.usf.registrationid_);
               }
               else
                  std::cerr << "No registered signal '" << hdr.usf.registrationid_ << "' found." << std::endl;
            }
            break;
         
         case FRAME_TYPE_REGISTER_SIGNAL_RESPONSE:
            {
               auto iter = outstanding_sig_registrs_.find(hdr.srf.sequence_nr_);
               if (iter != outstanding_sig_registrs_.end())
               {
                  sighandlers_[hdr.srf.id_] = iter->second;
                  outstanding_sig_registrs_.erase(iter);
               }
               else
                  std::cerr << "No such signal registration found." << std::endl;
            }
            break;

         case FRAME_TYPE_SIGNAL:
            {
               auto iter = sighandlers_.find(hdr.sef.id_);
               if (iter != sighandlers_.end())
               {
                  iter->second->eval(buf.get(), hdr.sef.payloadsize_);
               }
               else
                  std::cerr << "No such signal handler found." << std::endl;
            }
            break;
       
         case FRAME_TYPE_RESOLVE_INTERFACE:
            {
               detail::InterfaceResolveResponseFrame rf(0, generateId());
               rf.sequence_nr_ = hdr.irf.sequence_nr_;
                  
               auto iter = servers_.find(std::string((char*)buf.get()));
               if (iter != servers_.end())
               {
                  for(auto iditer = servers_by_id_.begin(); iditer != servers_by_id_.end(); ++iditer)
                  {
                     if (iter->second == iditer->second)
                     {
                        rf.id_ = iditer->first;
                        break;
                     }
                  }
               }
               
               genericSend(fd, rf, 0);
            }
            break;
         
         case FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE:
            {
               auto iter = pending_interface_resolves_.find(hdr.irrf.sequence_nr_);
               if (iter != pending_interface_resolves_.end())
               {
                  StubBase* stub = std::get<0>(iter->second);
                  stub->id_ = hdr.irrf.id_;
                  stub->current_sessionid_ = hdr.irrf.sessionid_;
                  pending_interface_resolves_.erase(iter);
                  
                  if (!stub->connected)
                  {
                      std::cerr << "'connected' hook not implemented. Client will probably hang..." << std::endl;
                  }
                  else
                      stub->connected(hdr.irrf.id_ == INVALID_SERVER_ID ? 
                        ConnectionState::NotAvailable : ConnectionState::Connected);
               }
            }
            break;
         
         default:
            std::cerr << "Unimplemented frame type=" << f.type_ << std::endl;
            break;
         }
      }
      else
         clearSlot(fd);
   }
   else
      clearSlot(fd);                      
   
   return 0;
}


int Dispatcher::handle_inotify(int fd, short /*pollmask*/)
{
   // TODO arbitrary size '64'
   std::aligned_storage<sizeof(inotify_event) + 64, std::alignment_of<inotify_event>::value>::type buf;
   
   int rc = ::read(fd, &buf, sizeof(inotify_event) + 64);
   if (rc > 0)
   {   
      struct inotify_event* evt = reinterpret_cast<inotify_event*>(&buf);
      handle_inotify_event(evt);
   }
   
   return -1;
}


void Dispatcher::register_fd(int fd, short pollmask, std::function<int(int, short)> cb)
{
   fds_[fd].fd = fd;
   fds_[fd].events = pollmask;
      
   fctn_[fd] = cb;
}


void Dispatcher::unregister_fd(int fd)
{
   fds_[fd].fd = -1;
   fds_[fd].events = 0;
   
   fctn_.erase(fd);
}


void Dispatcher::removeClient(StubBase& clnt)
{
   for(auto iter = clients_.begin(); iter != clients_.end(); ++iter)
   {
      if (&clnt == iter->second)
      {
         clients_.erase(iter);
         break;
      } 
   }
      
   for (auto iter = pending_lookups_.begin(); iter != pending_lookups_.end(); ++iter)
   {
      if (&clnt == std::get<0>(iter->second))
      {
         pending_lookups_.erase(iter);
         break;
      } 
   }
   
   for (auto iter = pending_interface_resolves_.begin(); iter != pending_interface_resolves_.end(); ++iter)
   {
      if (&clnt == std::get<0>(iter->second))
      {
         pending_interface_resolves_.erase(iter);
         break;
      } 
   }
}


int Dispatcher::once(unsigned int timeoutMs)
{
   int retval = 0;
   
   int rc = ::poll(fds_, sizeof(fds_)/sizeof(fds_[0]), timeoutMs);
         
   if (rc > 0)
   {
      for(unsigned int i=1; i<sizeof(fds_)/sizeof(fds_[0]); ++i)
      {
         if (fds_[i].revents & POLLIN)
         {
            fctn_[fds_[i].fd](fds_[i].fd, fds_[i].revents);
            break;  // TODO need better eventloop handling here 
         }                  
      }
   }
   else if (rc < 0)
   {
      if (errno != EINTR)
      {
         retval = -1;
         running_.store(false);
      }
   }
   
   checkPendings();
   
   return retval;
}


std::chrono::steady_clock::time_point Dispatcher::dueTime() const
{
   if (request_timeout_ == std::chrono::milliseconds::max())
      return std::chrono::steady_clock::time_point::max(); 
      
   return std::chrono::steady_clock::now() + request_timeout_;
}


void Dispatcher::checkPendings()
{
   auto now = std::chrono::steady_clock::now();
      
   for (auto iter = pendings_.begin(); iter != pendings_.end(); /*NOOP*/)
   {
      auto duetime = std::get<2>(iter->second);
      
      if (duetime <= now)
      {
         std::get<1>(iter->second)->eval(CallState(new TransportError(ETIMEDOUT, iter->first)), 0, 0);
         iter = pendings_.erase(iter);
      }
      else
         ++iter;
   }
   
   for(auto iter = pending_lookups_.begin(); iter != pending_lookups_.end(); /*NOOP*/)
   {
      if (std::get<1>(iter->second) <= now)
      {
         auto stub = std::get<0>(iter->second);
            
         if (stub->connected)
            stub->connected(ConnectionState::Timeout);
         
         iter = pending_lookups_.erase(iter);
      }
      else
         ++iter;
   }
   
   for (auto iter = pending_interface_resolves_.begin(); iter != pending_interface_resolves_.end(); /*NOOP*/)
   {
      if (std::get<1>(iter->second) <= now)
      {
         auto stub = std::get<0>(iter->second);
            
         if (stub->connected)
               stub->connected(ConnectionState::Timeout);
               
         iter = pending_interface_resolves_.erase(iter);
      }
      else
         ++iter;
   }
}


void Dispatcher::removePendingsForFd(int fd)
{
   auto iter = pendings_.begin();
   
   while(iter != pendings_.end())
   {
      if (std::get<3>(iter->second) == fd)
      {
         std::get<1>(iter->second)->eval(CallState(new TransportError(ECONNABORTED, iter->first)), 0, 0);
         iter = pendings_.erase(iter);
      }
      else
         ++iter;
   }
}


/// boundname may be 0 for handling clients without any servers
Dispatcher::Dispatcher(const char* boundname)
 : running_(false)
 , sequence_(0)
 , nextid_(0)
 , broker_(nullptr)
 , request_timeout_(std::chrono::milliseconds::max())    // FIXME move this to a config header, also other timeout defaults
{
   ::memset(fds_, 0, sizeof(fds_));
   
   if (boundname)
      assert(attach(boundname));
   
   inotify_fd_ = inotify_init();
   inotify_add_watch(inotify_fd_, "/tmp/dispatcher", IN_CREATE);
   
   fds_[inotify_fd_].fd = inotify_fd_;
   fds_[inotify_fd_].events = POLLIN;
   fctn_[inotify_fd_] = std::bind(&Dispatcher::handle_inotify, this, _1, _2);
}


void Dispatcher::socketConnected(int /*fd*/)
{
   // NOOP
}


void Dispatcher::socketDisconnected(int /*fd*/)
{
   // NOOP
}


int Dispatcher::run()
{
   running_.store(true);
   
   for(auto iter = clients_.begin(); iter != clients_.end(); ++iter)
   {
      if (strcmp(iter->second->boundname_, "auto:"))
      {
         if (iter->second->fd_ <= 0)
         {
            connect(*iter->second);
            
            // FIXME handle connect errors
            //if (!strncmp(iter->second->boundname_, "tcp:", 4))
              // return EXIT_FAILURE;
         }
      }
   }
   
   // now enter infinite eventloop
   loop();
   
   return EXIT_SUCCESS;
}


bool Dispatcher::attach(const char* endpoint)
{
   bool rc = false;
   
   if (!strncmp(endpoint, "unix:", 5) && strlen(endpoint) > 5)
   {
      // unix acceptor
      ::mkdir("/tmp/dispatcher", 0777);
      
      int acceptor = ::socket(PF_UNIX, SOCK_STREAM, 0);
      
      struct sockaddr_un addr;
      addr.sun_family = AF_UNIX;
      sprintf(addr.sun_path, "/tmp/dispatcher/%s", endpoint + 5);
      
      ::unlink(addr.sun_path);         
      ::bind(acceptor, (struct sockaddr*)&addr, sizeof(addr));
      
      ::listen(acceptor, 16);
      
      fds_[acceptor].fd = acceptor;
      fds_[acceptor].events = POLLIN;
      
      fctn_[acceptor] = std::bind(&Dispatcher::accept_socket, this, _1, _2);
      rc = true;
   }
   else if (!strncmp(endpoint, "tcp:", 4) && strlen(endpoint) > 4)
   {
      // tcp acceptor
      int acceptor = ::socket(PF_INET, SOCK_STREAM, 0);
    
      struct sockaddr_in addr;
      makeInetAddress(addr, endpoint);
      ::bind(acceptor, (struct sockaddr*)&addr, sizeof(addr));
      
      ::listen(acceptor, 16);
      
      fds_[acceptor].fd = acceptor;
      fds_[acceptor].events = POLLIN;
      
      fctn_[acceptor] = std::bind(&Dispatcher::accept_socket, this, _1, _2);
      rc = true;
   }
   
   if (rc)
      endpoints_.push_back(endpoint);
      
   return rc;
}


void* Dispatcher::getSessionData(uint32_t sessionid)
{
   auto iter = sessions_.find(sessionid);
   
   if (iter != sessions_.end())
      return iter->second.data_;
   
   return 0;
}


void Dispatcher::clearSlot(int idx)
{
   removePendingsForFd(fds_[idx].fd);
   
   while(::close(fds_[idx].fd) && errno == EINTR);
   
   for (auto iter = server_sighandlers_.begin(); iter != server_sighandlers_.end(); ++iter)
   {
      iter->second->removeAllWithFd(fds_[idx].fd);
   }
   
   for (auto iter = sessions_.begin(); iter != sessions_.end();)
   {
      if (iter->second.fd_ == fds_[idx].fd)
      {
         (*iter->second.destructor_)(iter->second.data_);
         sessions_.erase(iter);
         iter = sessions_.begin();
      }
      else
         ++iter;
   }
   
   // disconnect stubs
   for (auto iter = clients_.begin(); iter != clients_.end(); ++iter)
   {
      if (iter->second->fd_ == fds_[idx].fd)
      {
         if (iter->second->connected)
            iter->second->connected(ConnectionState::Disconnected);
         
         iter->second->fd_ = -1;
      }
   }
   
   socketDisconnected(fds_[idx].fd);
                 
   // FIXME 0 is no good value here
   fds_[idx].fd = 0;
   fds_[idx].events = 0;
}

}   // namespace ipc

}   // namespace simppl
