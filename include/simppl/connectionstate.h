#ifndef SIMPPL_CONNECTIONSTATE_H
#define SIMPPL_CONNECTIONSTATE_H


namespace simppl
{

namespace ipc
{


enum struct ConnectionState
{
   Connected,
   Disconnected,
   NotAvailable,    // probably not necessary
   Timeout          // probably not necessary
};


}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_CONNECTIONSTATE_H
