#ifndef SIMPPL_CONNECTIONSTATE_H
#define SIMPPL_CONNECTIONSTATE_H


namespace simppl
{

namespace dbus
{


enum struct ConnectionState
{
   Connected,
   Disconnected,
   NotAvailable,    // probably not necessary
   Timeout          // probably not necessary
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_CONNECTIONSTATE_H
