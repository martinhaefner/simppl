#include <gtest/gtest.h>

#include "simppl/serialization.h"
#include "simppl/any.h"
#include "simppl/buffer.h"
#include "simppl/filedescriptor.h"
#include "simppl/map.h"
#include "simppl/objectpath.h"
#include "simppl/string.h"
#include "simppl/struct.h"
#include "simppl/tuple.h"
#include "simppl/variant.h"
#include "simppl/vector.h"
#include "simppl/wstring.h"

#include <memory>
#include <string_view>


using simppl::dbus::Codec;
using simppl::dbus::signature_of;


namespace test
{
namespace signature
{

enum Color { Red, Green };


struct Serialized
{
   typedef simppl::dbus::make_serializer<int32_t, std::string, std::vector<double>>::type serializer_type;

   int32_t i;
   std::string str;
   std::vector<double> d;
};


struct Legacy
{
   int32_t i;
   std::string str;
};


#if SIMPPL_HAVE_BOOST_FUSION
typedef std::map<std::string, bool> flags_type;

struct Fusion
{
   int32_t i;
   std::string str;
   flags_type m;
};
#endif

}   // namespace signature
}   // namespace test


#if SIMPPL_HAVE_BOOST_FUSION
BOOST_FUSION_ADAPT_STRUCT(
   test::signature::Fusion,
   (int32_t, i)
   (std::string, str)
   (test::signature::flags_type, m)
)
#endif


namespace simppl {
namespace dbus {

// old-style user-provided codec, only provides make_type_signature
template<>
struct Codec<test::signature::Legacy>
{
   static
   void encode(DBusMessageIter& iter, const test::signature::Legacy& l)
   {
      DBusMessageIter _iter;
      dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &_iter);
      simppl::dbus::encode(_iter, l.i, l.str);
      dbus_message_iter_close_container(&iter, &_iter);
   }

   static
   void decode(DBusMessageIter& iter, test::signature::Legacy& l)
   {
      DBusMessageIter _iter;
      simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_STRUCT);
      simppl::dbus::decode(_iter, l.i, l.str);
      dbus_message_iter_next(&iter);
   }

   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << "(is)";
   }
};

}   // namespace dbus
}   // namespace simppl


using namespace test::signature;


namespace {

template<typename T>
constexpr
std::string_view sig()
{
   return Codec<T>::signature;
}


typedef std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> message_type;

message_type make_message()
{
   return message_type(dbus_message_new_method_call("a.b", "/a/b", "a.b", "f"), &dbus_message_unref);
}

}   // namespace


// all of these are checked at compile time
static_assert(sig<uint8_t>() == "y");
static_assert(sig<int16_t>() == "n");
static_assert(sig<uint32_t>() == "u");
static_assert(sig<int64_t>() == "x");
static_assert(sig<double>() == "d");
static_assert(sig<float>() == "d");   // D-Bus has no float
static_assert(sig<Color>() == "i");
static_assert(sig<bool>() == "b");
static_assert(sig<std::string>() == "s");
static_assert(sig<std::wstring>() == "au");
static_assert(sig<simppl::dbus::ObjectPath>() == "o");
static_assert(sig<simppl::dbus::FileDescriptor>() == "h");
static_assert(sig<simppl::dbus::FixedSizeBuffer<16>>() == "ay");
static_assert(sig<simppl::dbus::Any>() == "v");
static_assert(sig<std::variant<int32_t, std::string>>() == "v");
static_assert(sig<std::vector<int32_t>>() == "ai");
static_assert(sig<std::map<std::string, simppl::dbus::Any>>() == "a{sv}");
static_assert(sig<std::tuple<int32_t, double, std::vector<std::string>>>() == "(idas)");
static_assert(sig<std::vector<std::map<std::string, std::tuple<int32_t, double, std::vector<std::string>>>>>() == "aa{s(idas)}");
static_assert(sig<Serialized>() == "(isad)");
static_assert(sig<std::vector<Serialized>>() == "a(isad)");
#if SIMPPL_HAVE_BOOST_FUSION
static_assert(sig<Fusion>() == "(isa{sb})");
#endif

static_assert(Codec<std::vector<int32_t>>::signature.size() == 2);


TEST(Signature, signature_of)
{
   EXPECT_STREQ("aa{s(idas)}", (signature_of<std::vector<std::map<std::string, std::tuple<int32_t, double, std::vector<std::string>>>>>()));

   // compile-time signatures are handed out without any copy
   EXPECT_EQ(Codec<std::vector<Serialized>>::signature.c_str(), signature_of<std::vector<Serialized>>());
}


TEST(Signature, stream)
{
   std::ostringstream os;
   os << Codec<std::map<std::string, simppl::dbus::Any>>::signature;

   EXPECT_EQ("a{sv}", os.str());
}


TEST(Signature, make_signature)
{
   constexpr auto s = simppl::dbus::make_signature("(isi)");

   static_assert(s.size() == 5);
   static_assert(std::string_view(s) == "(isi)");
}


// deprecation warnings are switched off for this file, see CMakeLists.txt
TEST(Signature, legacy)
{
   static_assert(!simppl::dbus::detail::has_signature<Codec<Legacy>>::value);

   EXPECT_STREQ("(is)", signature_of<Legacy>());

   // a legacy codec nested in compile-time composites falls back to runtime, once
   typedef std::map<std::string, std::vector<Legacy>> nested_type;

   static_assert(!simppl::dbus::detail::has_signature<Codec<nested_type>>::value);

   EXPECT_STREQ("a{sa(is)}", signature_of<nested_type>());
   EXPECT_EQ(signature_of<nested_type>(), signature_of<nested_type>());

   // encode into a real message and let libdbus validate the signature
   auto msg = make_message();

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   nested_type in{ { "a", { { 1, "one" }, { 2, "two" } } } };
   simppl::dbus::encode(iter, in);

   EXPECT_STREQ("a{sa(is)}", dbus_message_get_signature(msg.get()));

   dbus_message_iter_init(msg.get(), &iter);

   nested_type out;
   simppl::dbus::decode(iter, out);

   ASSERT_EQ(1u, out.size());
   ASSERT_EQ(2u, out["a"].size());
   EXPECT_EQ(2, out["a"][1].i);
   EXPECT_EQ("two", out["a"][1].str);
}


TEST(Signature, buffer_in_container)
{
   // FixedSizeBuffer used to report "a" instead of "ay"
   unsigned char data[4] = { 1, 2, 3, 4 };

   std::vector<simppl::dbus::FixedSizeBuffer<4>> in;
   in.emplace_back(data);
   in.emplace_back(data);

   auto msg = make_message();

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   simppl::dbus::encode(iter, in);

   EXPECT_STREQ("aay", dbus_message_get_signature(msg.get()));

   dbus_message_iter_init(msg.get(), &iter);

   std::vector<simppl::dbus::FixedSizeBuffer<4>> out;
   simppl::dbus::decode(iter, out);

   ASSERT_EQ(2u, out.size());
   EXPECT_EQ(0, memcmp(data, out[1].ptr(), sizeof(data)));
}


TEST(Signature, float_as_double)
{
   auto msg = make_message();

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   simppl::dbus::encode(iter, 1.5f, std::vector<float>{ 0.25f, -2.0f });

   EXPECT_STREQ("dad", dbus_message_get_signature(msg.get()));

   // a float can be received as float or double
   dbus_message_iter_init(msg.get(), &iter);

   float f = 0;
   std::vector<float> vf;
   simppl::dbus::decode(iter, f, vf);

   EXPECT_EQ(1.5f, f);
   EXPECT_EQ((std::vector<float>{ 0.25f, -2.0f }), vf);

   dbus_message_iter_init(msg.get(), &iter);

   double d = 0;
   std::vector<double> vd;
   simppl::dbus::decode(iter, d, vd);

   EXPECT_EQ(1.5, d);
   EXPECT_EQ((std::vector<double>{ 0.25, -2.0 }), vd);
}


TEST(Signature, variant_decode)
{
   typedef std::variant<int32_t, std::string, std::vector<int32_t>> variant_type;

   auto msg = make_message();

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   simppl::dbus::encode(iter, variant_type(std::vector<int32_t>{ 1, 2, 3 }), variant_type(std::string("Hallo")));

   dbus_message_iter_init(msg.get(), &iter);

   variant_type v1, v2;
   simppl::dbus::decode(iter, v1, v2);

   ASSERT_EQ(2u, v1.index());
   EXPECT_EQ((std::vector<int32_t>{ 1, 2, 3 }), std::get<2>(v1));

   ASSERT_EQ(1u, v2.index());
   EXPECT_EQ("Hallo", std::get<1>(v2));
}
