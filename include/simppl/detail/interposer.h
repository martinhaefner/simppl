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
         typename Ancestor0T,
         template<typename...> class Method,
         template<typename...> class Signal,
         template<typename, int> class Property,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class... Is>
struct Interposer : I<Id, AncestorT, Method, Signal, Property, Interposer<Id-1, AncestorT, Ancestor0T, Method, Signal, Property, Is...>> {};

template<typename AncestorT,
         typename Ancestor0T,
         template<typename...> class Method,
         template<typename...> class Signal,
         template<typename, int> class Property,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I>
struct Interposer<0, AncestorT, Ancestor0T, Method, Signal, Property, I> : I<0, Ancestor0T, Method, Signal, Property, Ancestor0T> {};


}   // detail

}   // dbus

}   // simppl

#endif   // SIMPPL_DETAIL_INTERPOSER_H
