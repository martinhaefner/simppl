#ifndef SIMPPL_DETAIL_TIMEOUT_H
#define SIMPPL_DETAIL_TIMEOUT_H


#include <chrono>


namespace simppl
{
   
namespace dbus
{
 
struct request_specific_timeout_helper
{
   template<typename RepT, typename PeriodT>
   int operator=(std::chrono::duration<RepT, PeriodT> duration)
   {
      timeout_ = duration;
      return (1<<0);
   }
   
   std::chrono::milliseconds timeout_;
};


extern __thread request_specific_timeout_helper timeout;


namespace detail
{

extern __thread std::chrono::milliseconds request_specific_timeout;

}   // namespace detail

}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_DETAIL_TIMEOUT_H
