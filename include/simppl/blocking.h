#ifndef SIMPPL_BLOCKING_H
#define SIMPPL_BLOCKING_H


namespace simppl
{
   
namespace dbus
{
   
struct Dispatcher;

   
namespace detail
{


template<typename T>
struct BlockingResponseHandler
{
   BlockingResponseHandler(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T>& r, T& t);
   
   void operator()(const simppl::dbus::CallState& state, typename CallTraits<T>::param_type t);
   
private:

   T& t_;
   simppl::dbus::Dispatcher& disp_;
   simppl::dbus::ClientResponse<T>& r_;
};


template<typename... T>
struct BlockingResponseHandlerN
{
   BlockingResponseHandlerN(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T...>& r, std::tuple<T...>& t);
   
   void operator()(const simppl::dbus::CallState& state, typename CallTraits<T>::param_type... t);
   
private:

   std::tuple<T...>& t_;
   simppl::dbus::Dispatcher& disp_;
   simppl::dbus::ClientResponse<T...>& r_;
};

}   // namespace detail 

}   // namespace dbus

}   // namespace simppl


#include "simppl/dispatcher.h"


namespace simppl
{
   
namespace dbus
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
BlockingResponseHandler<T>::BlockingResponseHandler(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T>& r, T& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
{
   r_.handledBy(std::ref(*this));
}


template<typename T>
void BlockingResponseHandler<T>::operator()(const simppl::dbus::CallState& state, typename CallTraits<T>::param_type t)
{
   disp_.stop();
   r_.handledBy(std::nullptr_t());
   
   // do not allow to propagate the exception through the dbus C library, otherwise we could have severe 
   // trouble if libdbus is not compiled with exception support.
   if (!state)
      disp_.propagate(state);

   t_ = t;
}


template<typename... T>
inline
BlockingResponseHandlerN<T...>::BlockingResponseHandlerN(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T...>& r, std::tuple<T...>& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
{
   r_.handledBy(std::ref(*this));
}


template<typename... T>
void BlockingResponseHandlerN<T...>::operator()(const simppl::dbus::CallState& state, typename CallTraits<T>::param_type... t)
{
   disp_.stop();
   r_.handledBy(std::nullptr_t());
   
   if (!state)
      disp_.propagate(state);
   
   t_ = std::make_tuple(t...);
}


}   // namespace detail 

}   // namespace dbus

}   // namespace simppl


// ---------------------------------------------------------------------
   

/**
 * Call semantics for blocking calls:
 * 
 * int ret;
 * bool rc = stub.func() >> ret;
 */
inline
void operator>>(simppl::dbus::detail::ClientResponseHolder holder, std::nullptr_t)
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
void operator>>(simppl::dbus::detail::ClientResponseHolder holder, T& rArg)
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
void operator>>(simppl::dbus::detail::ClientResponseHolder holder, std::tuple<T...>& rArgs)
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
void operator>>(simppl::dbus::detail::ClientResponseHolder holder, std::tuple<T&...> rArgs)
{
   std::tuple<T...> t;
   holder.dispatcher_.waitForResponse(holder, t);
   rArgs = t;
}


#endif   // SIMPPL_BLOCKING_H
