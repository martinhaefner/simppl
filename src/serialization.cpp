#include "simppl/serialization.h"

#include <exception>


void simppl_dbus_message_iter_recurse(DBusMessageIter* iter, DBusMessageIter* nested, int expected_type)
{
   if (dbus_message_iter_get_arg_type(iter) != expected_type)
      throw simppl::dbus::DecoderError();
      
   dbus_message_iter_recurse(iter, nested);
}


void simppl_dbus_message_iter_get_basic(DBusMessageIter* iter, void* p, int expected_type)
{
   if (dbus_message_iter_get_arg_type(iter) != expected_type)
      throw simppl::dbus::DecoderError();
   
   dbus_message_iter_get_basic(iter, p);
   dbus_message_iter_next(iter);
}


namespace simppl
{

namespace dbus
{


Encoder::Encoder(DBusMessage* msg)
 : iter_(&own_)
 , parent_(nullptr)
 , uncaught_(0)
{
   dbus_message_iter_init_append(msg, &own_);
}


Encoder::Encoder(DBusMessageIter& iter)
 : iter_(&iter)
 , parent_(nullptr)
 , uncaught_(0)
{
   // NOOP
}


Encoder::Encoder(DBusMessageIter& parent, int type, const char* contained_signature)
 : iter_(&own_)
 , parent_(&parent)
 , uncaught_(std::uncaught_exceptions())
{
   dbus_message_iter_open_container(parent_, type, contained_signature, &own_);
}


Encoder::~Encoder()
{
   if (parent_)
   {
      if (std::uncaught_exceptions() > uncaught_)
      {
         dbus_message_iter_abandon_container(parent_, &own_);
      }
      else
         dbus_message_iter_close_container(parent_, &own_);
   }
}


void Encoder::append_basic(int type, const void* value)
{
   dbus_message_iter_append_basic(iter_, type, value);
}


void Encoder::append_fixed_array(int element_type, const void* data, int n)
{
   dbus_message_iter_append_fixed_array(iter_, element_type, &data, n);
}


Encoder Encoder::open_container(int type, const char* contained_signature)
{
   return Encoder(*iter_, type, contained_signature);
}


Decoder::Decoder(DBusMessage* msg)
 : iter_(&own_)
 , msg_(msg)
{
   dbus_message_iter_init(msg, &own_);
}


Decoder::Decoder(DBusMessageIter& iter, DBusMessage* msg)
 : iter_(&iter)
 , msg_(msg)
{
   // NOOP
}


Decoder::Decoder(const Decoder& rhs)
 : own_(*rhs.iter_)
 , iter_(&own_)
 , msg_(rhs.msg_)
{
   // NOOP
}


Decoder::Decoder(Decoder& parent, recurse_tag)
 : iter_(&own_)
 , msg_(parent.msg_)
{
   dbus_message_iter_recurse(parent.iter_, &own_);
}


void Decoder::get_basic(void* p, int expected_type)
{
   simppl_dbus_message_iter_get_basic(iter_, p, expected_type);
}


Decoder Decoder::recurse(int expected_type)
{
   if (arg_type() != expected_type)
      throw DecoderError();

   return Decoder(*this, recurse_tag{});
}


}   // namespace dbus

}   // namespace simppl
