#ifndef SIMPPL_PROPERTY_H
#define SIMPPL_PROPERTY_H


namespace simppl
{
   
namespace dbus
{

enum PropertyFlags
{
   ReadOnly     = 0,
   ReadWrite    = (1<<0),
   Notifying    = (1<<1)
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_PROPERTY_H
