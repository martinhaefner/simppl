#ifndef SIMPPL_DETAIL_VALIDATION_H
#define SIMPPL_DETAIL_VALIDATION_H


namespace simppl
{

namespace dbus
{

namespace detail
{


struct InOutOrOneway
{
    template<typename T>
    struct apply_
    {
        static_assert(is_in<T>::value || is_out<T>::value || std::is_same<T, oneway>::value, "neither in, out nor oneway parameter");
        enum { value = is_in<T>::value || is_out<T>::value || std::is_same<T, oneway>::value };
    };
};


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_VALIDATION_H
