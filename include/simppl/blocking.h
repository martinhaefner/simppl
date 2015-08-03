#ifndef SIMPPL_BLOCKING_H
#define SIMPPL_BLOCKING_H


namespace simppl
{
   
namespace ipc
{
   
struct Dispatcher;

   
namespace detail
{


template<typename T>
struct BlockingResponseHandler
{
   BlockingResponseHandler(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<T>& r, T& t);
   
   void operator()(const simppl::ipc::CallState& state, typename CallTraits<T>::param_type t);
   
private:

   T& t_;
   simppl::ipc::Dispatcher& disp_;
   simppl::ipc::ClientResponse<T>& r_;
};


template<typename... T>
struct BlockingResponseHandlerN
{
   BlockingResponseHandlerN(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<T...>& r, std::tuple<T...>& t);
   
   void operator()(const simppl::ipc::CallState& state, typename CallTraits<T>::param_type... t);
   
private:

   std::tuple<T...>& t_;
   simppl::ipc::Dispatcher& disp_;
   simppl::ipc::ClientResponse<T...>& r_;
};

}   // namespace detail 

}   // namespace ipc

}   // namespace simppl


#include "simppl/dispatcher.h"


namespace simppl
{
   
namespace ipc
{
   
   
template<typename T>
void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp, T& t)
{
   assert(resp.r_);
   assert(!isRunning());
   
   ClientResponse<T>* r = safe_cast<ClientResponse<T>*>(resp.r_);
   assert(r);
   
   detail::BlockingResponseHandler<T> handler(*this, *r, t);
   loop();
}

template<typename... T>
void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp, std::tuple<T...>& t)
{
   assert(resp.r_);
   assert(!running_.load());
   
   ClientResponse<T...>* r = safe_cast<ClientResponse<T...>*>(resp.r_);
   assert(r);
   
   detail::BlockingResponseHandlerN<T...> handler(*this, *r, t);
   loop();
}

   
namespace detail
{

template<typename T>
inline
BlockingResponseHandler<T>::BlockingResponseHandler(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<T>& r, T& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
{
   r_.handledBy(std::ref(*this));
}


template<typename T>
void BlockingResponseHandler<T>::operator()(const simppl::ipc::CallState& state, typename CallTraits<T>::param_type t)
{
   disp_.stop();
   r_.handledBy(std::nullptr_t());
   
   if (!state)
      state.throw_exception();
   
   t_ = t;
}


template<typename... T>
inline
BlockingResponseHandlerN<T...>::BlockingResponseHandlerN(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<T...>& r, std::tuple<T...>& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
{
   r_.handledBy(std::ref(*this));
}


template<typename... T>
void BlockingResponseHandlerN<T...>::operator()(const simppl::ipc::CallState& state, typename CallTraits<T>::param_type... t)
{
   disp_.stop();
   r_.handledBy(std::nullptr_t());
   
   if (!state)
      state.throw_exception();
   
   t_ = std::make_tuple(t...);
}


}   // namespace detail 

}   // namespace ipc

}   // namespace simppl


// ---------------------------------------------------------------------
   

/**
 * Call semantics for blocking calls:
 * 
 * int ret;
 * bool rc = stub.func() >> ret;
 */
inline
void operator>>(simppl::ipc::detail::ClientResponseHolder holder, std::nullptr_t)
{
   holder.dispatcher_.waitForResponse(holder);
}


/**
 * Call semantics for blocking calls:
 * 
 * int ret;
 * bool rc = stub.func() >> ret;
 */
template<typename T>
inline
void operator>>(simppl::ipc::detail::ClientResponseHolder holder, T& rArg)
{
   holder.dispatcher_.waitForResponse(holder, rArg);
}


/**
 * Call semantics for blocking calls:
 * 
 * std::tuple<int> ret;
 * bool rc = stub.func() >> ret;
 */
template<typename... T>
inline
void operator>>(simppl::ipc::detail::ClientResponseHolder holder, std::tuple<T...>& rArgs)
{
   holder.dispatcher_.waitForResponse(holder, rArgs);
}


/**
 * Call semantics for blocking calls:
 * 
 * int i;
 * double d;
 * bool rc = stub.func() >> std::tie(i, d);
 */
template<typename... T>
inline
void operator>>(simppl::ipc::detail::ClientResponseHolder holder, std::tuple<T&...> rArgs)
{
   std::tuple<T...> t;
   holder.dispatcher_.waitForResponse(holder, t);
   rArgs = t;
}


#endif   // SIMPPL_BLOCKING_H
