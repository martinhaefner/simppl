#include "simppl/timeout.h"


namespace simppl
{

namespace dbus
{

__thread request_specific_timeout_helper timeout = {};

   
namespace detail
{
   
__thread std::chrono::milliseconds request_specific_timeout = std::chrono::milliseconds(0);

}   // namespace detail

}   // namespace simppl

}   // namespace dbus
