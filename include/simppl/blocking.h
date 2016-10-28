#ifndef SIMPPL_BLOCKING_H
#define SIMPPL_BLOCKING_H


namespace simppl
{

namespace dbus
{

struct Dispatcher;


namespace detail
{


template<typename AttributeT>
struct BlockingAttributeResponseHandler
{
   BlockingAttributeResponseHandler(simppl::dbus::Dispatcher& disp, AttributeT& a, typename AttributeT::data_type& t);

   void operator()(simppl::dbus::CallState state, typename AttributeT::arg_type t);

private:

   typename AttributeT::data_type& t_;
   simppl::dbus::Dispatcher& disp_;
   AttributeT& a_;
};


template<typename T>
struct BlockingResponseHandler
{
   BlockingResponseHandler(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T>& r, uint32_t serial, T& t);

   void operator()(simppl::dbus::CallState state, typename CallTraits<T>::param_type t);

private:

   T& t_;
   simppl::dbus::Dispatcher& disp_;
   simppl::dbus::ClientResponse<T>& r_;
   uint32_t serial_;
};


template<typename... T>
struct BlockingResponseHandlerN
{
   BlockingResponseHandlerN(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T...>& r, uint32_t serial, std::tuple<T...>& t);

   void operator()(simppl::dbus::CallState state, typename CallTraits<T>::param_type... t);

private:

   std::tuple<T...>& t_;
   simppl::dbus::Dispatcher& disp_;
   simppl::dbus::ClientResponse<T...>& r_;
   uint32_t serial_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#include "simppl/dispatcher.h"
#include "simppl/stubbase.h"


namespace simppl
{

namespace dbus
{


template<typename T, int Flags>
void Dispatcher::waitForResponse(ClientAttribute<T, Flags>& attr, T& t)
{
   assert(!isRunning());

   detail::BlockingAttributeResponseHandler<ClientAttribute<T, Flags>> handler(*this, attr, t);
   loop();
}


template<typename T>
void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp, T& t)
{
   assert(resp.r_);
   assert(!isRunning());

   ClientResponse<T>* r = safe_cast<ClientResponse<T>*>(resp.r_);
   assert(r);

   detail::BlockingResponseHandler<T> handler(*this, *r, resp.serial_, t);
   loop();
}


template<typename... T>
void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp, std::tuple<T...>& t)
{
   assert(resp.r_);
   assert(!running_.load());

   ClientResponse<T...>* r = safe_cast<ClientResponse<T...>*>(resp.r_);
   assert(r);

   detail::BlockingResponseHandlerN<T...> handler(*this, *r, resp.serial_, t);
   loop();
}


namespace detail
{


template<typename AttributeT>
inline
BlockingAttributeResponseHandler<AttributeT>::BlockingAttributeResponseHandler(simppl::dbus::Dispatcher& disp, AttributeT& a, typename AttributeT::data_type& t)
 : t_(t)
 , disp_(disp)
 , a_(a)
{
   a_.handledBy(std::ref(*this));
}


template<typename AttributeT>
inline
void BlockingAttributeResponseHandler<AttributeT>::operator()(CallState state, typename AttributeT::arg_type t)
{
   disp_.stop();
   a_.handledBy(std::nullptr_t());

   if (!state)
      disp_.propagate(state);

   t_ = t;
}


template<typename T>
inline
BlockingResponseHandler<T>::BlockingResponseHandler(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T>& r, uint32_t serial, T& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
 , serial_(serial)
{
   r_.handledBy(std::ref(*this));
}


template<typename T>
void BlockingResponseHandler<T>::operator()(simppl::dbus::CallState state, typename CallTraits<T>::param_type t)
{
   if (state.sequenceNr() == serial_)
   {
      disp_.stop();
      r_.handledBy(std::nullptr_t());

      // do not allow to propagate the exception through the dbus C library, otherwise we could have severe
      // trouble if libdbus is not compiled with exception support.
      if (!state)
         disp_.propagate(state);

      t_ = t;
   }
}


template<typename... T>
inline
BlockingResponseHandlerN<T...>::BlockingResponseHandlerN(simppl::dbus::Dispatcher& disp, simppl::dbus::ClientResponse<T...>& r, uint32_t serial, std::tuple<T...>& t)
 : t_(t)
 , disp_(disp)
 , r_(r)
 , serial_(serial)
{
   r_.handledBy(std::ref(*this));
}


template<typename... T>
void BlockingResponseHandlerN<T...>::operator()(simppl::dbus::CallState state, typename CallTraits<T>::param_type... t)
{
   if (state.sequenceNr() == serial_)
   {
      disp_.stop();
      r_.handledBy(std::nullptr_t());

      if (!state)
         disp_.propagate(state);

      t_ = std::make_tuple(t...);
   }
}


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


// ---------------------------------------------------------------------


/**
 * Call semantics for blocking calls without return value:
 *
 * bool rc = stub.func() >> std::nullptr_t();
 */
inline
void operator>>(simppl::dbus::detail::ClientResponseHolder holder, std::nullptr_t)
{
   holder.dispatcher_.waitForResponse(holder);
}


/**
 * Call semantics for blocking calls with exactly one return value:
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
 * same for attributes get
 */
template<typename T, int Flags>
inline
void operator>>(simppl::dbus::ClientAttribute<T, Flags>& attr, T& rArg)
{
   attr.stub().disp().waitForResponse(attr, rArg);
}


/**
 * Call semantics for blocking calls with multiple return values:
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
 * Alternative call semantics for blocking calls with multiple return values:
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
