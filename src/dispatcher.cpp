#include "simppl/dispatcher.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#define SIMPPL_DISPATCHER_CPP
#include "simppl/serverside.h"
#undef SIMPPL_DISPATCHER_CPP

#include "simppl/brokerclient.h"


namespace
{
   
void makeInetAddress(struct sockaddr_in& addr, const char* endpoint)
{
   assert(strlen(endpoint) < 64);
   
   char tmp[64];
   ::strcpy(tmp, endpoint);
   char* port = tmp + 4;
   while(*++port != ':');
   *port = '\0';
   ++port;
   //std::cout << "'" << tmp << " : '" << port << "'" << std::endl;
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
   for(servermap_type::iterator iter = servers_.begin(); iter != servers_.end(); ++iter)
   {
      delete iter->second;
   }
   
   if (broker_)
   {
      delete broker_;
      broker_ = nullptr;
   }
   
   while(::close(selfpipe_[0]) && errno == EINTR);
   while(::close(selfpipe_[1]) && errno == EINTR);
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


bool Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp)
{
   assert(resp.r_);
   assert(!running_);
   
   char* data = nullptr;
   size_t len = 0;
   
   int rc = loopUntil(resp.sequence_nr_, &data, &len);
   std::unique_ptr<char> raii(data);
   
   if (rc == 0)
   {
      ClientResponse<>* r = safe_cast<ClientResponse<>*>(resp.r_);
      assert(r);
   }
   
   return rc == 0;
}


bool Dispatcher::connect(StubBase& stub, bool blockUntilResponse, const char* location)
{
   const char* the_location = location ? location : stub.boundname_;
   
   assert(strcmp(the_location, "auto:"));
   
   // 1. connect the socket physically - if not yet done
   socketsmap_type::iterator iter = socks_.find(the_location);
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
         return false;
   
      if (rc == 0)
      {
         fds_[stub.fd_].fd = stub.fd_;
         fds_[stub.fd_].events = POLLIN;
         
         socks_[the_location] = stub.fd_;
      }
      else
      {
         while(::close(stub.fd_) < 0 && errno == EINTR);
         stub.fd_ = -1;
      }
   }
   
   // 2. initialize interface resolution
   if (stub.fd_ > 0)
   {
      // don't cache anything here, a sessionid will be 
      // created no the server for any resolve request
      detail::InterfaceResolveFrame f(42);
      char buf[128];
      
      assert(strlen(stub.iface_) + 2 + strlen(stub.role_) < sizeof(buf));
      ::fullQualifiedName(buf, stub.iface_, stub.role_);
                              
      f.payloadsize_ = strlen(buf)+1;
      f.sequence_nr_ = generateSequenceNr();

      dangling_interface_resolves_[f.sequence_nr_] = &stub;
      genericSend(stub.fd_, f, buf);
      
      if (blockUntilResponse)
         loopUntil(f.sequence_nr_);
   }
      
   return stub.fd_ > 0;
}


void Dispatcher::serviceReady(StubBase* stub, const std::string& fullName, const std::string& location)
{
   assert(::fullQualifiedName(*stub) == fullName);
   connect(*stub, false, location.c_str());
}


void Dispatcher::addClient(StubBase& clnt)
{
   assert(!clnt.disp_);   // don't add it twice
   
   clnt.disp_ = this;
   clients_.insert(std::make_pair(::fullQualifiedName(clnt), &clnt)); 
   
   if (!strcmp(clnt.boundname_, "auto:"))
   {
      assert(broker_);
      broker_->waitForService(::fullQualifiedName(clnt), std::bind(&Dispatcher::serviceReady, this, &clnt, std::placeholders::_1, std::placeholders::_2));
   }
   else
   {
      if (isRunning())
         connect(clnt);
   }
}


bool Dispatcher::addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr)
{
   bool alreadyAttached = std::find_if(sighandlers_.begin(), sighandlers_.end(), 
         [&s](const sighandlers_type::value_type& v){ return v.second == &s; 
      }) != sighandlers_.end();
   
   if (!alreadyAttached)
   {
      for (outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.begin(); iter != outstanding_sig_registrs_.end(); ++iter)
      {
         if (iter->second == &s)
         {
            alreadyAttached = true;
            break;
         }
      }
      
      if (!alreadyAttached)
         outstanding_sig_registrs_[sequence_nr] = &s;
   }
   
   return !alreadyAttached;
}


int Dispatcher::loopUntil(uint32_t sequence_nr, char** argData, size_t* argLen, unsigned int timeoutMs)
{
   int retval = 0;
   running_ = true;
   
   do
   {
      retval = once_(sequence_nr, argData, argLen, timeoutMs);
   }
   while(running_);
   
   return retval;
}


uint32_t Dispatcher::removeSignalRegistration(ClientSignalBase& s)
{
   uint32_t rc = 0;
   
   for (sighandlers_type::iterator iter = sighandlers_.begin(); iter != sighandlers_.end(); ++iter)
   {
      if (iter->second == &s)
      {
         rc = iter->first;
         sighandlers_.erase(iter);
         break;
      }
   }
   
   // maybe still outstanding?
   for (outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.begin(); iter != outstanding_sig_registrs_.end(); ++iter)
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


int Dispatcher::accept_socket(int acceptor)
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
   }
   
   return fd;
}


int Dispatcher::once_(uint32_t sequence_nr, char** argData, size_t* argLen, unsigned int timeoutMs)
{
   int retval = 0;
   
   int rc = ::poll(fds_, sizeof(fds_)/sizeof(fds_[0]), timeoutMs);
         
   if (rc > 0)
   {
      for(unsigned int i=1; i<sizeof(fds_)/sizeof(fds_[0]); ++i)
      {
         if (fds_[i].revents & POLLIN)
         {
            if (af_[i] != 0)
            {
               // acceptor socket
               int new_fd = (this->*af_[i])(fds_[i].fd);
               
               if (new_fd > 0)
                  socketConnected(new_fd);
            }
            else
            {
               // ordinary stream socket
               detail::FrameHeader f;
                  
               ssize_t len = ::recv(fds_[i].fd, &f, sizeof(f), MSG_NOSIGNAL|MSG_WAITALL|MSG_PEEK);
               
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
                                             
                  len = ::recvmsg(fds_[i].fd, &msg, MSG_NOSIGNAL|MSG_WAITALL);

                  // all data read?
                  if (len == v[0].iov_len + v[1].iov_len)
                  {
                     switch(f.type_)
                     {
                     case FRAME_TYPE_REQUEST:
                        { 
                           servermapid_type::iterator iter = servers_by_id_.find(hdr.rqf.serverid_);
                           if (iter != servers_by_id_.end())
                           {
                              iter->second->eval(hdr.rqf.func_, hdr.rqf.sequence_nr_, hdr.rqf.sessionid_, fds_[i].fd, buf.get(), hdr.rqf.payloadsize_);
                           }
                           else
                              std::cerr << "No service with id=" << hdr.rqf.serverid_ << " found." << std::endl;
                        }
                        break;
                        
                     case FRAME_TYPE_TRANSPORT_ERROR:
                        if (sequence_nr == INVALID_SEQUENCE_NR || sequence_nr != hdr.tef.sequence_nr_)
                        {
                           CallState cs((TransportError*)(hdr.tef.err_));
                           ((ClientResponseBase*)hdr.tef.handler_)->eval(cs, 0, 0);
                        }
                        else
                        {
                           std::unique_ptr<TransportError> var((TransportError*)(hdr.tef.err_));
                           throw *var;
                        }
                     
                        break;
                        
                     case FRAME_TYPE_RESPONSE:
                        {  
                           outstanding_requests_type::iterator iter;
                           if ((iter = outstandings_.find(hdr.rf.sequence_nr_)) != outstandings_.end())
                           {
                              if (sequence_nr == INVALID_SEQUENCE_NR || hdr.rf.sequence_nr_ != sequence_nr)
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
                              }
                              else
                              {
                                 assert(argLen);
                                 running_ = false;
                                 
                                 if (hdr.rf.result_ == 0)
                                 {
                                    if (buf.get() == 0)
                                    {
                                       // must copy payload in this case
                                       *argData = new char[hdr.rf.payloadsize_];
                                       ::memcpy(*argData, buf.get(), hdr.rf.payloadsize_);
                                    }
                                    else
                                       *argData = buf.release();
                                    
                                    *argLen = hdr.rf.payloadsize_;
                                 }
                                 else
                                    throw RuntimeError(hdr.rf.result_, (const char*)buf.get(), hdr.rf.sequence_nr_);
                              }
                              
                              outstandings_.erase(iter);
                           }
                           else
                              std::cerr << "Got response for request never sent..." << std::endl;
                        }
                        break;
                     
                     case FRAME_TYPE_REGISTER_SIGNAL:
                        {
                           servermapid_type::iterator iter = servers_by_id_.find(hdr.rsf.serverid_);
                           if (iter != servers_by_id_.end())
                           {
                              uint32_t registrationid = generateId();
                              
                              detail::ServerSignalBase* sig = iter->second->addSignalRecipient(hdr.rsf.sig_, fds_[i].fd, registrationid, hdr.rsf.id_);
                              
                              if (sig)
                                 server_sighandlers_[registrationid] = sig;
                              
                              // FIXME error handling, what if sig is 0???
                              detail::SignalResponseFrame rf(registrationid, hdr.rsf.id_);
                              rf.sequence_nr_ = hdr.rsf.sequence_nr_;
                              
                              genericSend(fds_[i].fd, rf, 0);
                              
                              if (sig)
                                 sig->onAttach(registrationid);
                           }
                           else
                              std::cerr << "No server with id=" << hdr.rsf.serverid_ << " found." << std::endl;
                        }
                        break;
                        
                     case FRAME_TYPE_UNREGISTER_SIGNAL:
                        {
                           serversignalers_type::iterator iter = server_sighandlers_.find(hdr.usf.registrationid_);
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
                           outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.find(hdr.srf.sequence_nr_);
                           if (iter != outstanding_sig_registrs_.end())
                           {
                              sighandlers_[hdr.srf.id_] = iter->second;
                              outstanding_sig_registrs_.erase(iter);
                           }
                           else
                              std::cerr << "No such signal registration found." << std::endl;
                           
                           if (sequence_nr == hdr.srf.sequence_nr_)
                              running_ = false;
                        }
                        break;

                     case FRAME_TYPE_SIGNAL:
                        {
                           sighandlers_type::iterator iter = sighandlers_.find(hdr.sef.id_);
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
                              
                           servermap_type::iterator iter = servers_.find(std::string((char*)buf.get()));
                           if (iter != servers_.end())
                           {
                              for(servermapid_type::iterator iditer = servers_by_id_.begin(); iditer != servers_by_id_.end(); ++iditer)
                              {
                                 if (iter->second == iditer->second)
                                 {
                                    rf.id_ = iditer->first;
                                    break;
                                 }
                              }
                           }
                           else
                              std::cerr << "No such server found." << std::endl;
                           
                           genericSend(fds_[i].fd, rf, 0);
                        }
                        break;
                     
                     case FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE:
                        {
                           outstanding_interface_resolves_type::iterator iter = dangling_interface_resolves_.find(hdr.irrf.sequence_nr_);
                           if (iter != dangling_interface_resolves_.end())
                           {
                              StubBase* stub = iter->second;
                              stub->id_ = hdr.irrf.id_;
                              stub->current_sessionid_ = hdr.irrf.sessionid_;
                              dangling_interface_resolves_.erase(iter);
                              
                              if (sequence_nr == INVALID_SEQUENCE_NR)
                              {
                                 // eventloop driven
                                 if (!iter->second->connected)
                                 {
                                     std::cerr << "'connected' hook not implemented. Client will probably hang..." << std::endl;
                                 }
                                 else
                                     iter->second->connected();
                              }
                              else if (sequence_nr == hdr.irrf.sequence_nr_)
                                 running_ = false;
                           }
                        }
                        break;
                     
                     default:
                        std::cerr << "Unimplemented frame type=" << f.type_ << std::endl;
                        break;
                     }
                  }
                  else
                     clearSlot(i);
               }
               else
                  clearSlot(i);                      
               
               break;
            }
         }                  
      }
   }
   else if (rc < 0)
   {
      if (errno != EINTR)
      {
         retval = -1;
         running_ = false;
      }
   }
   
   return retval;
}


/// boundname may be 0 for handling clients without any servers
Dispatcher::Dispatcher(const char* boundname)
 : running_(false)
 , sequence_(0)
 , nextid_(0)
 , broker_(nullptr)
{
   ::memset(fds_, 0, sizeof(fds_));
   ::memset(af_, 0, sizeof(af_));
   
   if (boundname)
   {
      assert(attach(boundname));
   }
   
   // can't use pipe since then the reading code would have to be changed...
   int rc = ::socketpair(PF_UNIX, SOCK_STREAM, 0, selfpipe_);
   assert(!rc);
   
   fds_[selfpipe_[0]].fd = selfpipe_[0];
   fds_[selfpipe_[0]].events = POLLIN;
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
   running_ = true;
   
   for(clientmap_type::iterator iter = clients_.begin(); iter != clients_.end(); ++iter)
   {
      if (strcmp(iter->second->boundname_, "auto:"))
      {
         if (iter->second->fd_ <= 0)
            assert(connect(*iter->second));
      }
   }
   
   // now enter infinite eventloop
   loopUntil();
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
      
      af_[acceptor] = &Dispatcher::accept_socket;
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
      
      af_[acceptor] = &Dispatcher::accept_socket;
      rc = true;
   }
   
   if (rc)
      endpoints_.push_back(endpoint);
      
   return rc;
}

void* Dispatcher::getSessionData(uint32_t sessionid)
{
   sessionmap_type::iterator iter = sessions_.find(sessionid);
   if (iter != sessions_.end())
      return iter->second.data_;
   
   return 0;
}


void Dispatcher::clearSlot(int idx)
{
   while(::close(fds_[idx].fd) && errno == EINTR);
   
   for (serversignalers_type::iterator iter = server_sighandlers_.begin(); iter != server_sighandlers_.end(); ++iter)
   {
      iter->second->removeAllWithFd(fds_[idx].fd);
   }
   
   for (sessionmap_type::iterator iter = sessions_.begin(); iter != sessions_.end();)
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
   
   socketDisconnected(fds_[idx].fd);
                     
   fds_[idx].fd = 0;
   fds_[idx].events = 0;
}

}   // namespace ipc

}   // namespace simppl
