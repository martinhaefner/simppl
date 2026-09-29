#ifndef SIMPPL_DETAIL_TIMEOUT_H
#define SIMPPL_DETAIL_TIMEOUT_H


#include <chrono>


namespace simppl
{

namespace dbus
{

/**
 * Options for a single request.
 */
struct RequestOptions
{
   /// 0: use the dispatcher's request timeout
   std::chrono::milliseconds timeout_ = std::chrono::milliseconds(0);
};


namespace detail
{

struct TimeoutTag
{
   template<typename RepT, typename PeriodT>
   constexpr
   RequestOptions operator=(std::chrono::duration<RepT, PeriodT> duration) const
   {
      return RequestOptions{ std::chrono::duration_cast<std::chrono::milliseconds>(duration) };
   }
};

}   // namespace detail


/**
 * Request specific timeout, overrides the dispatcher's request timeout:
 *
 *    stub.method[simppl::dbus::timeout = 700ms](args...);
 */
inline constexpr detail::TimeoutTag timeout{};

}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_DETAIL_TIMEOUT_H
