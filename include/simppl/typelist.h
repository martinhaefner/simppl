#ifndef SIMPPL_TYPELIST_H
#define SIMPPL_TYPELIST_H


#include <type_traits>

#include "simppl/type_wrappers.h"


namespace simppl
{

struct NilType {};

template<typename HeadT, typename TailT>
struct TypeList
{
    typedef HeadT head_type;
    typedef TailT tail_type;
};

// ---------------------------------- size ------------------------------------------

/// calculate the size of a given typelist
template<typename ListT>
struct Size;

template<typename HeadT, typename TailT>
struct Size<TypeList<HeadT, TailT> >
{
    enum { value = Size<TailT>::value + 1 };
};

template<typename HeadT>
struct Size<TypeList<HeadT, NilType> >
{
    enum { value = 1 };
};

// -------------------------------popfrontN ------------------------------------

template<int N, typename ListT>
struct PopFrontN;

template<int N, typename HeadT, typename TailT>
struct PopFrontN<N, TypeList<HeadT, TailT> >
{
   typedef typename PopFrontN<N-1, TailT>::type type;
};

template<typename HeadT, typename TailT>
struct PopFrontN<0, TypeList<HeadT, TailT> >
{
   typedef TypeList<HeadT, TailT> type;
};

template<int N>
struct PopFrontN<N, NilType>
{
   typedef NilType type;
};

// ------------------------------ push front -------------------------------------

template<typename ListT, typename InsertT>
struct PushFront
{
   typedef TypeList<InsertT, ListT> type;
};

// a NOOP
template<typename ListT>
struct PushFront<ListT, NilType>
{
   typedef ListT type;
};

// ------------------------------- pop back ------------------------------------

template<typename ListT>
struct PopBack;

template<typename HeadT, typename TailT>
struct PopBack<TypeList<HeadT, TailT> >
{
   typedef TypeList<HeadT, typename PopBack<TailT>::type> type;
};

template<typename HeadT>
struct PopBack<TypeList<HeadT, NilType> >
{
   typedef NilType type;
};

template<>
struct PopBack<NilType>
{
   typedef NilType type;
};

// --------------------------------------- back ------------------------------------------------

template<typename ListT>
struct Back;

template<typename T, typename ListT>
struct Back<TypeList<T, ListT> >
{
   typedef typename Back<ListT>::type type;
};

template<typename T>
struct Back<TypeList<T, NilType> >
{
   typedef T type;
};

// --------------------------------------- find ------------------------------------------------

// forward decl
template<typename SearchT, typename ListT, int N=0>
struct Find;

template<typename SearchT, typename HeadT, typename TailT, int N>
struct Find<SearchT, TypeList<HeadT, TailT>, N>
{
    typedef typename std::conditional<(int)std::is_same<SearchT, HeadT>::value, std::integral_constant<int, N>, Find<SearchT, TailT, N+1> >::type type_;
    static const int value = type_::value;
};

template<typename SearchT, typename HeadT, int N>
struct Find<SearchT, TypeList<HeadT, NilType>, N>
{
    typedef typename std::conditional<(int)std::is_same<SearchT, HeadT>::value, std::integral_constant<int, N>, std::integral_constant<int, -1> >::type type_;
    static const int value = type_::value;
};

template<typename SearchT, int N>
struct Find<SearchT, NilType, N>
{
    enum { value = -1 };
};

// -------------------------------- type at -------------------------------------------

/// extract type at given position in typelist; position must be within bounds
template<int N, typename ListT>
struct TypeAt;

template<int N, typename HeadT, typename TailT>
struct TypeAt<N, TypeList<HeadT, TailT> >
{
    typedef typename TypeAt<N-1, TailT>::type type;
    typedef const typename TypeAt<N-1, TailT>::type const_type;
};

template<typename HeadT, typename TailT>
struct TypeAt<0, TypeList<HeadT, TailT> >
{
    typedef HeadT type;
    typedef const HeadT const_type;
};

// ------------------- type at (relaxed, does not matter if boundary was crossed) ---------------------

/// extract type at given position in typelist; if position is out of
/// typelist bounds it always returns the type NilType
template<int N, typename ListT>
struct RelaxedTypeAt;

template<int N>
struct RelaxedTypeAt<N, NilType>
{
    typedef NilType type;   ///< save access over end of typelist
    typedef const NilType const_type;
};

template<int N, typename HeadT, typename TailT>
struct RelaxedTypeAt<N, TypeList<HeadT, TailT> >
{
    typedef typename RelaxedTypeAt<N-1, TailT>::type type;
    typedef const typename RelaxedTypeAt<N-1, TailT>::type const_type;
};

template<typename HeadT, typename TailT>
struct RelaxedTypeAt<0, TypeList<HeadT, TailT> >
{
    typedef HeadT type;
    typedef const HeadT const_type;
};

// --------------------------- reverse the sequence ------------------------------------

namespace detail
{

template<typename ListT, int N>
struct ReverseHelper
{
   typedef TypeList<typename TypeAt<N - 1, ListT>::type, typename ReverseHelper<ListT, N - 1>::type> type;
};

template<typename ListT>
struct ReverseHelper<ListT, 1>
{
   typedef TypeList<typename TypeAt<0, ListT>::type, NilType> type;
};

}   // namespace detail


/// reverse the sequence
template<typename ListT>
struct Reverse
{
   typedef typename detail::ReverseHelper<ListT, Size<ListT>::value>::type type;
};

// ------------------------ Count a certain type within the typelist ---------------------------------

template<typename SearchT, typename ListT>
struct Count;

template<typename SearchT, typename HeadT, typename TailT>
struct Count<SearchT, TypeList<HeadT, TailT> >
{
   enum { value = std::is_same<SearchT, HeadT>::value + Count<SearchT, TailT>::value };
};

template<typename SearchT>
struct Count<SearchT, NilType>
{
   enum { value = 0 };
};

// --------------------------- make a typelist (C++11) ------------------------------------

template<typename... T>
struct make_typelist;

template<typename T1, typename... T>
struct make_typelist<T1, T...>
{
   typedef TypeList<T1, typename make_typelist<T...>::type> type;
};

template<>
struct make_typelist<>
{
   typedef TypeList<NilType, NilType> type;
};

template<typename T>
struct make_typelist<T>
{
   typedef TypeList<T, NilType> type;
};

// ---------------------------- all of ---------------------------------

template<typename TL, typename FuncT>
struct AllOf;

template<typename HeadT, typename TailT, typename FuncT>
struct AllOf<TypeList<HeadT, TailT>, FuncT>
{
    enum { value = FuncT::template apply_<HeadT>::value && AllOf<TailT, FuncT>::value };
};

template<typename HeadT, typename FuncT>
struct AllOf<TypeList<HeadT, NilType>, FuncT>
{
    enum { value = FuncT::template apply_<HeadT>::value };
};

template<typename FuncT>
struct AllOf<TypeList<NilType, NilType>, FuncT>
{
    enum { value = true };
};

// ------------------ transform to other type --------------------------

template<typename ListT, typename FuncT>
struct transform;

template<typename HeadT, typename TailT, typename FuncT>
struct transform<TypeList<HeadT, TailT>, FuncT>
{
   typedef TypeList<typename FuncT::template apply_<HeadT>::type, typename transform<TailT, FuncT>::type> type;
};

template<typename HeadT, typename FuncT>
struct transform<TypeList<HeadT, NilType>, FuncT>
{
   typedef TypeList<typename FuncT::template apply_<HeadT>::type, NilType> type;
};


}   // namespace simppl


#endif // SIMPPL_TYPELIST_H

