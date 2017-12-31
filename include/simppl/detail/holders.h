#ifndef SIMPPL_DETAIL_HOLDERS_H
#define SIMPPL_DETAIL_HOLDERS_H


#include "callinterface.h"


namespace simppl
{

namespace dbus
{

namespace detail
{

template<typename HolderT>
struct InterimCallbackHolder
{
   typedef HolderT holder_type;

   InterimCallbackHolder(const InterimCallbackHolder& rhs)
   : pc_(std::move(rhs.pc_))
   {
      // NOOP
   }
   
   InterimCallbackHolder& operator=(const InterimCallbackHolder&) = delete;

   explicit inline
   InterimCallbackHolder(const PendingCall& pc)
    : pc_(std::move(pc))
   {
      // NOOP
   }
   
   PendingCall pc_;
};


template<typename FuncT, typename ReturnT>
struct CallbackHolder
{
   CallbackHolder(const CallbackHolder&) = delete;
   CallbackHolder& operator=(const CallbackHolder&) = delete;


   explicit inline
   CallbackHolder(const FuncT& f)
    : f_(std::move(f))
   {
      // NOOP
   }
   
   static inline
   void _delete(void* p)
   {
      auto that = (CallbackHolder*)p;
      delete that;
   }

   static
   void pending_notify(DBusPendingCall* pc, void* data)
   {
       auto msg = simppl::dbus::make_message(dbus_pending_call_steal_reply(pc));

       auto that = (CallbackHolder*)data;
       assert(that->f_);

       CallState cs(*msg);
       
       DBusMessageIter iter;
       dbus_message_iter_init(msg.get(), &iter);
       
       GetCaller<ReturnT>::type::template evalResponse(iter, that->f_, cs);
   }

   FuncT f_;
};


template<typename FuncT, typename DataT>
struct PropertyCallbackHolder
{
   PropertyCallbackHolder(const PropertyCallbackHolder&) = delete;
   PropertyCallbackHolder& operator=(const PropertyCallbackHolder&) = delete;


   explicit inline
   PropertyCallbackHolder(const FuncT& f)
    : f_(f)
   {
      // NOOP
   }

   static inline
   void _delete(void* p)
   {
      auto that = (PropertyCallbackHolder*)p;
      delete that;
   }

   static
   void pending_notify(DBusPendingCall* pc, void* data)
   {
      auto msg = simppl::dbus::make_message(dbus_pending_call_steal_reply(pc));

       auto that = (PropertyCallbackHolder*)data;
       assert(that->f_);

       simppl::dbus::CallState cs(*msg);
       if (cs)
       {
          DBusMessageIter iter;
          dbus_message_iter_init(msg.get(), &iter);

          simppl::Variant<DataT> v;
          decode(iter, v);

          that->f_(cs, *v.template get<DataT>());
       }
       else
          that->f_(cs, DataT());
   }

   FuncT f_;
};

}   // detail

}   // dbus

}   // simppl


#endif   // SIMPPL_DETAIL_HOLDERS_H
