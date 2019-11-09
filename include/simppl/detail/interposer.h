#ifndef SIMPPL_DETAIL_INTERPOSER_H
#define SIMPPL_DETAIL_INTERPOSER_H

namespace simppl
{

namespace dbus
{

namespace detail
{


template<int Id,
         typename AncestorT,
         template<typename...> class Method,
         template<typename...> class Signal,
         template<typename, int> class Property,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class... Is>
struct Interposer : I<Id, AncestorT, Method, Signal, Property, Interposer<Id-1, AncestorT, Method, Signal, Property, Is...>> {};

template<typename AncestorT,
         template<typename...> class Method,
         template<typename...> class Signal,
         template<typename, int> class Property,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I>
struct Interposer<0, AncestorT, Method, Signal, Property, I> : I<0, AncestorT, Method, Signal, Property, AncestorT> {};


}   // detail

}   // dbus

}   // simppl

#endif   // SIMPPL_DETAIL_INTERPOSER_H
