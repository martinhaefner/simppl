#ifndef SIMPPL_DETAIL_VALIDATION_H
#define SIMPPL_DETAIL_VALIDATION_H


#include "simppl/detail/parameter_deduction.h"
#include "simppl/parameter_deduction.h"
namespace simppl
{

namespace dbus
{

namespace detail
{


struct InOutThrowOrOneway
{
    template<typename T>
    struct apply_
    {
        static_assert(is_in<T>::value || is_out<T>::value || std::is_same<T, oneway>::value || is_throw<T>::value, "neither in, out nor oneway parameter and no throw directive");
        enum { value = is_in<T>::value || is_out<T>::value || std::is_same<T, oneway>::value || is_throw<T>::value };
    };
};


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_VALIDATION_H
