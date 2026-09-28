#ifndef SIMPPL_SIGNATURE_H
#define SIMPPL_SIGNATURE_H


#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <dbus/dbus-protocol.h>


namespace simppl
{

namespace dbus
{

/**
 * A D-Bus type signature of compile-time known length N, null-terminated.
 */
template<std::size_t N>
struct signature_string
{
   constexpr
   const char* c_str() const
   {
      return buf_;
   }

   constexpr
   std::size_t size() const
   {
      return N;
   }

   constexpr
   operator std::string_view() const
   {
      return { buf_, N };
   }

   char buf_[N + 1];
};


template<std::size_t N>
inline
std::ostream& operator<<(std::ostream& os, const signature_string<N>& sig)
{
   return os.write(sig.c_str(), N);
}


template<std::size_t... N>
constexpr
signature_string<(N + ... + 0)> concat(const signature_string<N>&... sigs)
{
   signature_string<(N + ... + 0)> rc{};
   std::size_t pos = 0;

   auto append = [&rc, &pos](const char* s, std::size_t len)
   {
      for (std::size_t i = 0; i < len; ++i)
         rc.buf_[pos++] = s[i];
   };

   (append(sigs.c_str(), sigs.size()), ...);
   rc.buf_[pos] = '\0';

   return rc;
}


/**
 * For user-provided codecs:
 *
 *    static constexpr auto signature = make_signature("(isi)");
 */
template<std::size_t N>
constexpr
signature_string<N - 1> make_signature(const char (&str)[N])
{
   signature_string<N - 1> rc{};

   for (std::size_t i = 0; i < N; ++i)
      rc.buf_[i] = str[i];

   return rc;
}


/**
 * Signature fragment made of the given type codes, e.g.
 * signature_chars<DBUS_TYPE_ARRAY> or signature_chars<DBUS_STRUCT_BEGIN_CHAR>.
 * Can be used as a part of composite_signature.
 */
template<char... C>
struct signature_chars
{
   static constexpr signature_string<sizeof...(C)> signature{ { C..., '\0' } };
};


namespace detail
{

template<typename CodecT, typename = void>
struct has_signature : std::false_type {};

template<typename CodecT>
struct has_signature<CodecT, std::void_t<decltype(CodecT::signature.c_str())>> : std::true_type {};


/// marks simppl's own runtime composites, they don't trigger deprecation warnings
struct runtime_signature_tag {};


template<typename CodecT>
[[deprecated("Codec<T>::make_type_signature(std::ostream&) is deprecated, "
             "provide 'static constexpr auto signature' instead (see simppl/signature.h)")]]
inline
std::ostream& legacy_type_signature(std::ostream& os)
{
   return CodecT::make_type_signature(os);
}


template<typename CodecT>
inline
std::ostream& write_signature(std::ostream& os)
{
   if constexpr (has_signature<CodecT>::value)
   {
      return os << CodecT::signature;
   }
   else if constexpr (std::is_base_of<runtime_signature_tag, CodecT>::value)
   {
      return CodecT::make_type_signature(os);
   }
   else
      return legacy_type_signature<CodecT>(os);
}


template<bool AllStatic, typename... PartsT>
struct composite_signature_impl
{
   static constexpr auto signature = concat(PartsT::signature...);

   static_assert(signature.size() <= DBUS_MAXIMUM_SIGNATURE_LENGTH, "D-Bus signature too long");

   [[deprecated("use simppl::dbus::signature_of<T>() or Codec<T>::signature instead")]]
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << signature;
   }
};


/// fallback if any part only provides the legacy make_type_signature()
template<typename... PartsT>
struct composite_signature_impl<false, PartsT...> : runtime_signature_tag
{
   static
   std::ostream& make_type_signature(std::ostream& os)
   {
      (write_signature<PartsT>(os), ...);
      return os;
   }
};

}   // namespace detail


/**
 * Base class for codecs: the signature is the concatenation of the
 * signatures of all parts. A part is anything with a static 'signature'
 * member, i.e. another codec or a signature_chars fragment.
 */
template<typename... PartsT>
using composite_signature = detail::composite_signature_impl<(detail::has_signature<PartsT>::value && ...), PartsT...>;


template<typename T>
struct Codec;


/**
 * @return the D-Bus signature of type T as null-terminated string.
 */
template<typename T>
inline
const char* signature_of()
{
   typedef Codec<T> codec_type;

   if constexpr (detail::has_signature<codec_type>::value)
   {
      return codec_type::signature.c_str();
   }
   else
   {
      static const std::string sig = []{
         std::ostringstream os;
         detail::write_signature<codec_type>(os);
         return os.str();
      }();

      return sig.c_str();
   }
}


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SIGNATURE_H
