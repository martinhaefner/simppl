#ifndef SIMPPL_DETAIL_INTERPOSER_H
#define SIMPPL_DETAIL_INTERPOSER_H

namespace simppl
{

namespace dbus
{

namespace detail
{


/// Make inheritence structure so inherit recursively from all interfaces.
template<int Id,
         typename AncestorT,
         typename Ancestor0T,    ///< here the objectmanager could be interposed
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class... Is>
struct Interposer : I<Id, AncestorT, ServerMethod, ServerSignal, ServerProperty, Interposer<Id-1, AncestorT, Ancestor0T, Is...>> {};

/**
 * Specialization for last interface: inherit from base class.
 */
template<typename AncestorT,
         typename Ancestor0T,
         template<int, typename, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I>
struct Interposer<0, AncestorT, Ancestor0T, I> : I<0, Ancestor0T, ServerMethod, ServerSignal, ServerProperty, Ancestor0T> {};


}   // detail

}   // dbus

}   // simppl

#endif   // SIMPPL_DETAIL_INTERPOSER_H
