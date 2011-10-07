#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>

template<bool, typename TrueT, typename FalseT> 
struct if_
{
   typedef TrueT type;
};

template<typename TrueT, typename FalseT> 
struct if_<false, TrueT, FalseT>
{
   typedef FalseT type;
};

// Policies: do not loop on EINTR on blocking functions, but loop on asynchronous function calls since there data
// should be ready to read so we do not block infinitely if we return to the action due to an EINTR. 

// unix sockets
#include <sys/un.h>

// ip sockets
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>

#define DEFAULT_BACKLOG 10

  
// forward decl
struct Dispatcher;


struct NormalDescriptorTraits
{
   typedef int fd_type;
   enum { Invalid = -1 };
};


namespace IPv4
{

struct EndpointTraits
{
   typedef struct sockaddr_in addr_type;
   
   enum { 
      Domain = PF_INET,
      AddressFamily = AF_INET
   };
};

}   // namespace IPv4


namespace Unix
{
  
struct EndpointTraits
{
   typedef struct sockaddr_un addr_type;
   
   enum { 
      Domain = PF_UNIX,
      AddressFamily = AF_UNIX
   };
};

}   // namespace Unix


struct StreamTraits
{
   enum { socket_type = SOCK_STREAM };
};


struct DatagramTraits
{
   enum { socket_type = SOCK_DGRAM };
};


template<typename IOCompletionRoutineT>
struct ReadEventTraits
{
   typedef void* buffer_type;
   typedef IOCompletionRoutineT completion_routine_type;
   
   enum { poll_mask = POLLIN };   
};


template<typename IOCompletionRoutineT>
struct WriteEventTraits
{
   typedef const void* buffer_type;
   typedef IOCompletionRoutineT completion_routine_type;
   
   enum { poll_mask = POLLOUT };
};


struct AcceptEventTraits
{
   enum { poll_mask = POLLIN };
};


template<typename TraitsT>
struct Device : TraitsT
{
   explicit
   Device(Dispatcher& dispatcher)
    : dispatcher_(&dispatcher)
    , fd_(TraitsT::Invalid)
   {
      // NOOP
   }

   Device()
    : dispatcher_(0)
    , fd_(TraitsT::Invalid)
   {
      // NOOP
   }
      
   Dispatcher* dispatcher_;
   typename TraitsT::fd_type fd_;
};


struct EventBase
{
  virtual ~EventBase() { /* NOOP */ }
  virtual void eval(int fd) = 0;
};


template<typename TraitsT>
struct IOEvent : EventBase, TraitsT
{
  IOEvent(typename TraitsT::buffer_type buf, size_t len,  typename TraitsT::completion_routine_type func)
   : buf_(buf)
   , len_(len)
   , func_(func)
  {
     // NOOP
  }
   
  typedef TraitsT traits_type;
  
  typename traits_type::buffer_type buf_;
  size_t len_;

  typename traits_type::completion_routine_type func_;
};


template<typename IOCompletionRoutineT>
struct ReadEvent : IOEvent<ReadEventTraits<IOCompletionRoutineT> >
{
  typedef IOEvent<ReadEventTraits<IOCompletionRoutineT> > base_type;

  ReadEvent(typename base_type::buffer_type buf, size_t len, IOCompletionRoutineT func)
   : base_type(buf, len, func)
  {
     // NOOP
  }

  void eval(int fd)
  {
     ssize_t len = 0;

     do
     {
        len = ::recv(fd, this->buf_, this->len_, MSG_NOSIGNAL);  
     }
     while (len < 0 && errno == EINTR);
     
     // FIXME better return enums (errnos) here instead of bool
     this->func_(len, len >= 0);
  }
};


template<typename IOCompletionRoutineT>
struct WriteEvent : IOEvent<WriteEventTraits<IOCompletionRoutineT> >
{
  typedef IOEvent<WriteEventTraits<IOCompletionRoutineT> > base_type;
  
  WriteEvent(typename base_type::buffer_type buf, size_t len, IOCompletionRoutineT func)
   : base_type(buf, len, func)
  {
     // NOOP
  }

  int getPollMask() const
  {
     return POLLOUT | POLLHUP;
  }

  void eval(int fd)
  {
     ssize_t len = 0;

     do
     {
        len = ::send(fd, base_type::buf_, base_type::len_, MSG_NOSIGNAL);   
     }
     while (len < 0 && errno == EINTR);
  
     // FIXME better return enums (errnos) here instead of bool
     this->func_(len, len >= 0);
  }
};


template<typename DeviceT, typename AcceptCompletionRoutineT>
struct AcceptEvent : public EventBase
{     
  typedef AcceptEventTraits traits_type;
  
  inline
  AcceptEvent(DeviceT& dev, AcceptCompletionRoutineT func)
   : dev_(dev)
   , func_(func)
  {
     // NOOP
  }

  void eval(int fd)
  {
     typename DeviceT::endpoint_type address;
     socklen_t len = sizeof(typename DeviceT::endpoint_type::traits_type::addr_type);
     do
     {
        dev_.fd_ = ::accept(fd, (struct sockaddr*)&address.address_, &len);
     }
     while(dev_.fd_ < 0 && errno == EINTR);
     
     func_(address, dev_.fd_ >= 0);
  }

  DeviceT& dev_;
  AcceptCompletionRoutineT func_;
};


struct Dispatcher   // mixin the event loop via mixin interface...
{
   Dispatcher(size_t capa = 128)
   : max_(0)
   , capa_(capa)
   , fds_table_(new pollfd[capa_])
   , events_table_(new EventBase*[capa_])
   , finished_(false)
   , rc_(0)
   {
      for (int i=0; i<capa_; ++i)
      {
         fds_table_[i].fd = -1;
      }
   }
   
   // FIXME allow read and write simultaneously on same fdset entry, but no multiple reads/writes 
   // FIXME therefore a fd should always exist only once in the fdset
   // FIXME use internal pool for events, which could perhaps be rebalanced if a large event is added.
   template<typename EventT>
   void enqueueEvent(int fd, EventT* evt)
   {
      int pos = -1;
      
      for(int i=0; i<max_; ++i)
      {
         if (fds_table_[i].fd == -1)
         {
            pos = i;
            break;
         }
      }
      
      if (pos == -1)
      {
         assert (capa_ >= max_);
         pos = max_;
         ++max_;       
      }
      
      fds_table_[pos].fd = fd;
      fds_table_[pos].events = EventT::traits_type::poll_mask;
      events_table_[pos] = evt;
   }
   
   
   template<typename EventTraitsT>
   void removeEvent(int fd)
   {
      for (int i=0; i<max_; ++i)
      {
	 if (fds_table_[i].fd == fd && (fds_table_[i].events & EventTraitsT::poll_mask))
	 {
	   clearSlot(i);
	   break;
	 }
      }
   }
   
   void removeAll(int fd)
   {
     for (int i=0; i<max_; ++i)
     {
	if (fds_table_[i].fd == fd)
	{
	  clearSlot(i);
	  break;
	}
     }
   }
   
  private:
    
    void clearSlot(int i)
    {
      fds_table_[i].fd = -1;
      delete events_table_[i];   
    }
    
  public:
   
   int poll(int timeout_ms)
   {    
      int ret = ::poll(fds_table_, max_, 1000);
      
      if (ret > 0)
      {
         for(int i=0; i<max_ && !finished_; ++i)
         {
            if (fds_table_[i].fd >= 0 && (fds_table_[i].revents & (POLLIN|POLLOUT)))
            {
               events_table_[i]->eval(fds_table_[i].fd);
               
	       clearSlot(i);           
            }
         }
      }
      else if (ret < 0)
      {
         if (errno == EINTR)
            ret = 0;       
      }
      
      return ret;
   }
   
   int run()  
   {
      int ret = 0;
      
      while(!finished_ && ret >= 0)
      {
         ret = this->poll(1000);
      }
      
      if (!finished_ && ret < 0)
         rc_ = -1;
      
      return rc_;
   }
   
   void stop(int rc = 0)
   {
      finished_ = true;
      rc_ = rc;
   }
   
   size_t max_;
   size_t capa_;
   
   pollfd* fds_table_;
   EventBase** events_table_;
   
   bool finished_;
   int rc_;
};


typedef struct sockaddr sockaddr_t;

  
template<typename InheriterT, typename TraitsT>
struct Endpoint
{
  template<typename DeviceT, typename AcceptCompletionRoutineT>
  friend struct AcceptEvent; 

  typedef TraitsT traits_type;

  // do not initialize the endpoint
  Endpoint()   
  {
     // NOOP
  }

  // zero out the endpoint
  explicit
  inline
  Endpoint(bool /*zero*/)
  {     
     ::memset(&address_, 0, sizeof(address_));
  }

  inline
  operator const sockaddr_t*() const
  {
     return (const struct sockaddr*)&address_;
  }

  inline
  operator sockaddr_t*()
  {
     return (struct sockaddr*)&address_;
  }

  inline
  InheriterT& operator=(const typename traits_type::addr_type& rhs)
  {
     address_ = rhs;
     return *(InheriterT*)this;
  }

  inline
  socklen_t getSockAddrSize() const
  {
     return sizeof(address_);
  }
  
protected:
  typename traits_type::addr_type address_;
};


namespace IPv4
{
  
struct Endpoint : public ::Endpoint<Endpoint, EndpointTraits>, EndpointTraits
{
   // construct an uninitialized (invalid) endpoint  
   inline
   Endpoint()
    : ::Endpoint<Endpoint, EndpointTraits>(true)
   {
      // NOOP
   }
   
public:   

   Endpoint(const char* addr, int port)
    : ::Endpoint<Endpoint, EndpointTraits>()
   {
      address_.sin_family = EndpointTraits::AddressFamily;
      inet_aton(addr, &address_.sin_addr);
      address_.sin_port = htons(port);
   }

   // any address
   explicit
   Endpoint(int port)
    : ::Endpoint<Endpoint, EndpointTraits>()
   {
      address_.sin_family = EndpointTraits::AddressFamily;
      address_.sin_addr.s_addr = INADDR_ANY;
      address_.sin_port = htons(port);
   }

   inline
   int getPort() const
   {
      return ntohs(address_.sin_port);
   }

   using ::Endpoint<Endpoint, EndpointTraits>::operator=;
};

}   // namespace IPv4


namespace Unix
{
  
struct Endpoint : public ::Endpoint<Endpoint, EndpointTraits>, EndpointTraits
{
   // construct an uninitialized (invalid) endpoint
   inline
   Endpoint()
    : ::Endpoint<Endpoint, EndpointTraits>(true)
   {
      // NOOP
   }
    
   Endpoint(const char* name)
    : ::Endpoint<Endpoint, EndpointTraits>()
   {
      address_.sun_family = EndpointTraits::AddressFamily;
      memset(address_.sun_path, 0, sizeof(address_.sun_path));
      strncpy(address_.sun_path, name, sizeof(address_.sun_path) - 1);
   }
};

}   // namespace Unix


template<typename AddressT, typename InheriterT>
struct DatagramSocketMixin
{
   ssize_t write_some_to(const AddressT& to, const void* buf, size_t len)
   {
      return ::sendto(((InheriterT*)this)->fd_, buf, len, MSG_NOSIGNAL, to, to.getSockAddrSize());
   }
   
   ssize_t read_some_from(AddressT& from, void* buf, size_t len)
   {
      socklen_t slen = from.getSockAddrSize();
      return ::recvfrom(((InheriterT*)this)->fd_, buf, len, MSG_NOSIGNAL, from, &slen);
   }
};


struct EmptySocketMixin
{
};


template<typename AddressT, typename TraitsT>
struct Socket 
 : Device<NormalDescriptorTraits>
 , TraitsT
 , if_< TraitsT::socket_type == (int)SOCK_DGRAM, 
        DatagramSocketMixin<AddressT, Socket<AddressT, TraitsT> >
      , EmptySocketMixin>::type
{ 
 typedef Device<NormalDescriptorTraits> base_type;
 typedef AddressT endpoint_type;

 explicit
 Socket(Dispatcher& dispatcher)
  : base_type(dispatcher)
 {
    // NOOP
 }

 Socket()
  : base_type()
 {
    // NOOP
 }
 
 ~Socket()
 {
   (void)close();
 }

 bool open()
 {
    fd_ = ::socket(AddressT::Domain, TraitsT::socket_type, 0);
 }

 bool close()
 {
    int rc = 0;

    if (fd_)
    {
      // TODO keep flag if something is registered on the event loop and remove the events only if necessary
      if (this->dispatcher_)
         this->dispatcher_->removeAll(fd_);
   
      do
      {
	rc = ::close(fd_);
      }
      while(rc < 0 && errno == EINTR);
    }
    
    return rc == 0;
 }
 
 bool is_open() const
 {
    return fd_ != Invalid;
 }

 operator const void*() const
 {
    return is_open() ? this : 0;
 }

 bool connect(const AddressT& addr)
 {
    struct sockaddr* tmp = addr;
    return connect(fd_, tmp, addr.getSockAddrSize());
 }

#if FIXME
 template<typename ConnectCompletionRoutineT>
 void async_connect(const AddressT& addr, ConnectCompletionRoutineT func)
 {
    assert(this->dispatcher_);
    // set socket nonblocking
    // call connect
    // evaluate return state and probably enqueue event
    this->dispatcher_->enqueueEvent(fd_, new ConnectEvent<ConnectCompletionRoutineT>(addr, func));
 }
#endif 
 
 template<typename IOCompletionRoutineT>
 void async_read_some(void* buf, size_t len, IOCompletionRoutineT func)
 {
    // FIXME use member pointer to store that can hold a readEvent. This store should
    // keep in mind if the event is already enqueued and return false then...
    assert(this->dispatcher_);
    this->dispatcher_->enqueueEvent(fd_, new ReadEvent<IOCompletionRoutineT>(buf, len, func));
 }

 template<typename IOCompletionRoutineT>
 void async_write_some(const void* buf, size_t len, IOCompletionRoutineT func)
 {
    assert(this->dispatcher_);
    this->dispatcher_->enqueueEvent(fd_, new WriteEvent<IOCompletionRoutineT>(buf, len, func));
 }

 void cancel_read()
 {
   assert(this->dispatcher_);
   this->dispatcher_->template removeEvent<ReadEventTraits<void> >(fd_);
 }

 void cancel_write()
 {
   assert(this->dispatcher_);
   this->dispatcher_->template removeEvent<WriteEventTraits<void> >(fd_);
 }

 ssize_t read_some(void* buf, size_t len)
 {
   return ::recv(fd_, buf, len, MSG_NOSIGNAL);
 }

 ssize_t write_some(const void* buf, size_t len)
 {
   return ::send(fd_, buf, len, MSG_NOSIGNAL);
 }
};


template<typename AddressT>
struct StreamSocket : public Socket<AddressT, StreamTraits>
{
   explicit
   StreamSocket(Dispatcher& dispatcher)
    : Socket<AddressT, StreamTraits>(dispatcher)
   {
      // NOOP
   }

   StreamSocket()
    : Socket<AddressT, StreamTraits>()
   {
      // NOOP
   } 
};


template<typename AddressT>
struct DatagramSocket : public Socket<AddressT, DatagramTraits>
{
   explicit
   DatagramSocket(Dispatcher& dispatcher)
    : Socket<AddressT, DatagramTraits>(dispatcher)
   {
      // NOOP
   }

   DatagramSocket()
    : Socket<AddressT, DatagramTraits>()
   {
      // NOOP
   }
};


namespace IPv4
{

typedef ::StreamSocket<Endpoint> StreamSocket;
typedef ::DatagramSocket<Endpoint> DatagramSocket;

}   // namespace IPv4


namespace Unix
{
  
typedef ::StreamSocket<Unix::Endpoint> StreamSocket;
typedef ::DatagramSocket<Unix::Endpoint> DatagramSocket;

}   // namespace Unix

template<typename AddressT>
struct Acceptor : private Socket<AddressT, StreamTraits>
{  
  typedef Socket<AddressT, StreamTraits> base_type;

  using Socket<AddressT, StreamTraits>::fd_;

  Acceptor()
   : base_type()
  {
     base_type::open();
  }

  explicit
  Acceptor(Dispatcher& dispatcher)
   : base_type(dispatcher)
  {
     base_type::open();
  }

  bool accept(Socket<AddressT, StreamTraits>& sock, AddressT* addr)
  {
     typename AddressT::traits_type::addr_type tmp;
     socklen_t len = sizeof(tmp);

     sock.fd_ = ::accept(fd_, (struct sockaddr*)&tmp, &len);
     
     if (sock.fd_ >= 0 && addr)
        (*addr) = tmp;

     return (sock.fd_ >= 0);
  }

  template<typename DeviceT, typename AcceptCompletionRoutineT>
  void async_accept(DeviceT& dev, AcceptCompletionRoutineT func)
  {
     assert(this->dispatcher_);
     this->dispatcher_->enqueueEvent(fd_, new AcceptEvent<DeviceT, AcceptCompletionRoutineT>(dev, func));
  }
  
  void cancel_accept()
  {
    assert(this->dispatcher_);
    this->dispatcher_->template removeEventAcceptEventTraits>(fd_);
  }

  bool listen(size_t backlog = DEFAULT_BACKLOG)
  {
     return ::listen(fd_, backlog) == 0;
  }

  bool bind(const AddressT& address)
  {
     return ::bind(fd_, address, address.getSockAddrSize()) == 0;
  }
};


namespace IPv4
{
typedef ::Acceptor<Endpoint> Acceptor;
}
namespace Unix
{
typedef ::Acceptor<Endpoint> Acceptor;
}


// ---------------------------------------------------------------


template<typename ObjT, typename FuncT>
struct s_mem_fun_2
{
   inline
   s_mem_fun_2(ObjT* obj, FuncT func)
    : obj_(obj)
    , func_(func)
   {
      // NOOP
   }

   template<typename T1, typename T2>
   inline
   void operator()(T1 t1, T2 t2)
   {
      (obj_->*func_)(t1, t2);
   }

   ObjT* obj_;
   FuncT func_;
};

template<typename ObjT, typename FuncT>
inline
s_mem_fun_2<ObjT, FuncT>
mem_fun_2(ObjT* obj, FuncT func)
{
   return s_mem_fun_2<ObjT, FuncT>(obj, func);
}


// --- Test and other code --------------------------------------------------------------


// Echo server state machine
struct EchoSession
{
  EchoSession(Dispatcher& dispatcher)
   : sock_(dispatcher)
   , package_(0)
   , cur_(0)
  {
     // NOOP
  }

  void start()   // start the sessions internal statemachine
  {     
     sock_.async_read_some(buf, sizeof(buf), mem_fun_2(this, &EchoSession::handleRead));
  }

  void handleRead(size_t len, bool ok)
  {   
     if (ok)
     {   
        package_ = len;
        cur_ = 0;

        sock_.async_write_some(buf, package_, mem_fun_2(this, &EchoSession::handleWrite));
     }
     else
        delete this;
  }

  void handleWrite(size_t len, bool ok)
  {
     if (ok)
     {
        if (cur_ + len < package_)
        {
           sock_.async_write_some(buf + cur_, package_ - cur_, mem_fun_2(this, &EchoSession::handleWrite));
           cur_ += len;
        }
        else
           sock_.async_read_some(buf, sizeof(buf), mem_fun_2(this, &EchoSession::handleRead));
     }
     else
        delete this;
  }

  IPv4::StreamSocket sock_;
  char buf[128];
  size_t package_;
  size_t cur_;
};


// classical server socket acceptor
struct EchoServer
{
  EchoServer(int port)
   : d_()
   , a_(d_)
   , next_(0)
  {
     if (a_.bind(IPv4::Endpoint(9999)))
     {
        a_.listen();
        arm();
     }
  }

  void run()
  {
     d_.run();
  }

private:

  void arm()
  {
     // rearm acceptor
     next_ = new EchoSession(d_);
     a_.async_accept(next_->sock_, mem_fun_2(this, &EchoServer::accepthandler));
  }

public:

  void accepthandler(IPv4::Endpoint& address, bool ok)
  {
     if (ok)
     {
        next_->start();
        arm();
     }
     else
        d_.stop();
  }

  Dispatcher d_;
  IPv4::Acceptor a_;
  EchoSession* next_;
};


int main()
{
#if 1
   //UDPSocket u;
   //char buf[2];
   //EndpointIPv4 addr1;
   //u.read_some_from(addr1, buf, 2);
   //u.write_some_to(EndpointIPv4("127.0.0.1", 9999), "HallO", 5);
   
   IPv4::StreamSocket s;
   IPv4::Acceptor a;
   
   a.bind(IPv4::Endpoint("127.0.0.1", 9999));
   a.listen();
   
   IPv4::Endpoint addr;
   a.accept(s, &addr);

   if (s)
   {
      char buf[80];
      ssize_t len = 0;
      while(len = s.read_some(buf, sizeof(buf)))
      {
         s.write_some(buf, len);
         write(STDOUT_FILENO, buf, len);
      }
      //s.cancel_read();
   }
#else
   EchoServer s(7777);
   s.run();
#endif
   return 0;
}
