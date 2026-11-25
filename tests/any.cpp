#include <algorithm>
#include <any>
#include <cerrno>
#include <cstdint>
#include <gtest/gtest.h>

#include <chrono>
#include <fcntl.h>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

#include "simppl/interface.h"
#include "simppl/objectpath.h"
#include "simppl/serialization.h"
#include "simppl/skeleton.h"
#include "simppl/stub.h"

#include "simppl/any.h"
#include "simppl/filedescriptor.h"
#include "simppl/map.h"
#include "simppl/string.h"
#include "simppl/struct.h"
#include "simppl/vector.h"

using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::oneway;
using simppl::dbus::out;

namespace test {
namespace any {
struct complex {
  typedef typename simppl::dbus::make_serializer<double, double>::type
      serializer_type;

  complex() = default;
  complex(double re, double im) : re(re), im(im) {
    // NOOP
  }

  double re;
  double im;
};

INTERFACE(AServer) {
  Method<in<simppl::dbus::Any>> set;
  Method<out<simppl::dbus::Any>> get;

  Method<in<simppl::dbus::Any>, out<simppl::dbus::Any>> setGet;
  Method<out<simppl::dbus::Any>> getVecEmpty;

  Method<in<simppl::dbus::Any>, out<std::vector<simppl::dbus::Any>>> complex;

  Method<in<int>, in<simppl::dbus::Any>, in<std::string>, out<int>,
         out<simppl::dbus::Any>, out<std::string>>
      in_the_middle;

  Method<oneway> stop;

  AServer()
      : INIT(set), INIT(get), INIT(setGet), INIT(getVecEmpty), INIT(complex),
        INIT(in_the_middle), INIT(stop) {
    // NOOP
  }
};
} // namespace any
} // namespace test

namespace {

struct Client : simppl::dbus::Stub<test::any::AServer> {
  Client(simppl::dbus::Dispatcher &d)
      : simppl::dbus::Stub<test::any::AServer>(d, "role") {
    connected >> [this](simppl::dbus::ConnectionState s) {
      get.async() >> [this](const simppl::dbus::CallState &state,
                            const simppl::dbus::Any &a) {
        EXPECT_EQ(42, a.as<int>());

        set.async(simppl::dbus::Any(42)) >>
            [this](const simppl::dbus::CallState &state) {
              std::vector<std::string> sv;
              sv.push_back("Hello");
              sv.push_back("World");

              complex.async(sv) >>
                  [this](const simppl::dbus::CallState &state,
                         const std::vector<simppl::dbus::Any> &result) {
                    EXPECT_EQ(1, result.size());
                    EXPECT_STREQ("Hello", result[0].as<std::string>().c_str());
                    EXPECT_EQ(42, result[1].as<int>());

                    // calling twice possible?
                    EXPECT_EQ(42, result[2].as<test::any::complex>().re);
                    EXPECT_EQ(4711, result[2].as<test::any::complex>().im);

                    disp().stop();
                  };
            };
      };
    };
  }
};

struct Server : simppl::dbus::Skeleton<test::any::AServer> {
  Server(simppl::dbus::Dispatcher &d)
      : simppl::dbus::Skeleton<test::any::AServer>(d, "role") {
    get >> [this]() {
      simppl::dbus::Any a(42);

      respond_with(get(a));
    };

    set >> [this](const simppl::dbus::Any &a) {
      EXPECT_EQ(true, a.is<int>());
      EXPECT_EQ(false, a.is<double>());
      EXPECT_EQ(false, a.is<std::string>());

      EXPECT_EQ(42, a.as<int>());

      respond_with(set());
    };

    setGet >> [this](const simppl::dbus::Any &a) { respond_with(get(a)); };

    getVecEmpty >> [this]() {
      std::vector<std::string> vec;
      vec.push_back("World");
      simppl::dbus::Any a(vec);
      respond_with(getVecEmpty(a));
    };

    complex >> [this](const simppl::dbus::Any &a) {
      auto sv = a.as<std::vector<std::string>>();

      EXPECT_EQ(2, sv.size());
      EXPECT_STREQ("Hello", sv[0].c_str());
      EXPECT_STREQ("World", sv[1].c_str());

      std::vector<simppl::dbus::Any> av;
      av.push_back(std::string("Hello"));
      av.push_back(int(42));
      // av.push_back(test::any::complex(42, 4711));
      // EXPECT_TRUE(false); // TODO: Implement struct support

      respond_with(complex(av));
    };

    in_the_middle >>
        [this](int i, const simppl::dbus::Any &a, const std::string &str) {
          simppl::dbus::Any ret = a.as<std::vector<int>>();

          respond_with(in_the_middle(i, ret, str));
        };

    stop >> [this]() { this->disp().stop(); };
  }
};
} // namespace

TEST(Any, method) {
  simppl::dbus::Dispatcher d("bus:session");
  Client c(d);
  Server s(d);

  d.run();
}

TEST(Any, blocking_get) {
  simppl::dbus::Dispatcher d("bus:session");

  std::thread t([]() {
    simppl::dbus::Dispatcher d("bus:session");
    Server s(d);
    d.run();
  });

  simppl::dbus::Stub<test::any::AServer> stub(d, "role");

  // wait for server to get ready
  std::this_thread::sleep_for(200ms);

  simppl::dbus::Any a = stub.get();
  EXPECT_EQ(42, a.as<int>());

  stub.stop(); // stop server
  t.join();
}

TEST(Any, blocking_set) {
  simppl::dbus::Dispatcher d("bus:session");

  std::thread t([]() {
    simppl::dbus::Dispatcher d("bus:session");
    Server s(d);
    d.run();
  });

  simppl::dbus::Stub<test::any::AServer> stub(d, "role");

  // wait for server to get ready
  std::this_thread::sleep_for(200ms);

  stub.set(42);

  stub.stop(); // stop server
  t.join();
}

TEST(Any, blocking_complex) {
  simppl::dbus::Dispatcher d("bus:session");

  std::thread t([]() {
    simppl::dbus::Dispatcher d("bus:session");
    Server s(d);
    d.run();
  });

  simppl::dbus::Stub<test::any::AServer> stub(d, "role");

  // wait for server to get ready
  std::this_thread::sleep_for(200ms);

  std::vector<std::string> sv;
  sv.push_back("Hello");
  sv.push_back("World");

  auto result = stub.complex(sv);

  EXPECT_EQ(1, result.size());
  EXPECT_STREQ("Hello", result[0].as<std::string>().c_str());
  EXPECT_EQ(42, result[1].as<int>());

  // calling twice possible?
  EXPECT_EQ(42, result[2].as<test::any::complex>().re);
  EXPECT_EQ(4711, result[2].as<test::any::complex>().im);

  stub.stop(); // stop server
  t.join();
}

TEST(Any, blocking_in_the_middle) {
  simppl::dbus::Dispatcher d("bus:session");

  std::thread t([]() {
    simppl::dbus::Dispatcher d("bus:session");
    Server s(d);
    d.run();
  });

  simppl::dbus::Stub<test::any::AServer> stub(d, "role");

  // wait for server to get ready
  std::this_thread::sleep_for(200ms);

  std::vector<int> iv;
  iv.push_back(7777);
  iv.push_back(4711);

  int i;
  simppl::dbus::Any result;
  std::string str;

  try {
    i = 21;
    std::tie(i, result, str) = stub.in_the_middle(42, iv, "Hallo Welt");
  } catch (std::exception &ex) {
    EXPECT_TRUE(false);
  }

  EXPECT_EQ(42, i);

  // calling as<...> multiple times is ok?
  EXPECT_EQ(2, result.as<std::vector<int>>().size());
  EXPECT_EQ(7777, result.as<std::vector<int>>()[0]);
  EXPECT_EQ(4711, result.as<std::vector<int>>()[1]);

  EXPECT_STREQ("Hallo Welt", str.c_str());

  stub.stop(); // stop server
  t.join();
}

TEST(Any, types) {
  simppl::dbus::Any a(42);
  EXPECT_TRUE(a.is<int>());
  EXPECT_FALSE(a.is<double>());
  EXPECT_FALSE(a.is<std::string>());
  EXPECT_EQ(42, a.as<int>());

  simppl::dbus::Any b(42.42);
  EXPECT_TRUE(b.is<double>());
  EXPECT_FALSE(b.is<int>());
  EXPECT_FALSE(b.is<std::string>());
  EXPECT_EQ(42.42, b.as<double>());

  simppl::dbus::Any c(std::string("Hello"));
  EXPECT_FALSE(c.is<double>());
  EXPECT_FALSE(c.is<int>());
  EXPECT_TRUE(c.is<std::string>());
  EXPECT_STREQ("Hello", c.as<std::string>().c_str());

  simppl::dbus::Any d;
  EXPECT_FALSE(d.is<int>());
  EXPECT_FALSE(d.is<double>());
  EXPECT_FALSE(d.is<std::string>());
  EXPECT_FALSE(d.is<std::vector<int>>());

  simppl::dbus::Any e(std::vector<int>{1, 2, 3});
  EXPECT_FALSE(e.is<int>());
  EXPECT_FALSE(e.is<double>());
  EXPECT_FALSE(e.is<std::string>());
  EXPECT_TRUE(e.is<std::vector<int>>());

  auto v = e.as<std::vector<int>>();
  EXPECT_EQ(3, v.size());
  EXPECT_EQ(1, v[0]);
  EXPECT_EQ(2, v[1]);
  EXPECT_EQ(3, v[2]);

  simppl::dbus::Any f(
      std::vector<simppl::dbus::Any>{std::tuple<std::string, int>{"olaf", 1}});
  EXPECT_FALSE(f.is<std::vector<std::tuple<std::string>>>());
  bool fResult = f.is<std::vector<std::tuple<std::string, int>>>();
  EXPECT_TRUE(fResult);

  simppl::dbus::Any g(
      std::vector<simppl::dbus::Any>{std::map<std::string, int>{{"olaf", 1}}});
  bool gb = g.is<std::vector<std::map<std::string, int>>>();
  EXPECT_TRUE(gb);
}

void test_enc_dec_f(
    const std::function<void(DBusMessageIter &iter, DBusMessage *message)> &f) {
  // Create a dummy dbus message
  DBusMessage *message = dbus_message_new_method_call(
      "org.example.TargetService", "/org/example/TargetObject",
      "org.example.TargetInterface", "TargetMethod");
  EXPECT_NE(message, nullptr);
  DBusMessageIter iter;
  dbus_message_iter_init_append(message, &iter);

  // Call the actual function
  f(iter, message);

  // Cleanup
  dbus_message_unref(message);
}

template <typename T> void test_enc_dec(const T &t) {
  test_enc_dec_f([&t](DBusMessageIter &iter, DBusMessage *message) {
    // Encode
    simppl::dbus::Any any = t;
    EXPECT_TRUE(any.is<T>());
    EXPECT_EQ(any.as<T>(), t);
    simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any);

    dbus_message_iter_init(message, &iter);
    simppl::dbus::Any result;
    simppl::dbus::Codec<simppl::dbus::Any>::decode(iter, result);
    EXPECT_TRUE(result.is<T>());

    EXPECT_EQ(any.containedType, result.containedType);
    EXPECT_EQ(any.containedTypeSignature, result.containedTypeSignature);
    EXPECT_EQ(any.as<T>(), result.as<T>());
  });
}

template <typename T> void test_enc_dec_number() {
  test_enc_dec(static_cast<T>(std::numeric_limits<T>::min()));
  test_enc_dec(static_cast<T>(std::numeric_limits<T>::min() / 2));
  test_enc_dec(static_cast<T>(std::numeric_limits<T>::max()));
  test_enc_dec(static_cast<T>(std::numeric_limits<T>::max() / 2));
  test_enc_dec(static_cast<T>(0));
}

TEST(Any, empty) {
  test_enc_dec_f([](DBusMessageIter &iter, DBusMessage *message) {
    simppl::dbus::Any any;
    EXPECT_THROW(simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any),
                 std::invalid_argument);
  });
}

TEST(Any, encode_decode_uint8) { test_enc_dec_number<uint8_t>(); }
// TEST(Any, encode_decode_int8) { test_enc_dec_number<int8_t>(); } // There is
// no signed byte type in dbus :'(

TEST(Any, encode_decode_uint16) { test_enc_dec_number<uint16_t>(); }
TEST(Any, encode_decode_int16) { test_enc_dec_number<int16_t>(); }

TEST(Any, encode_decode_uint32) { test_enc_dec_number<uint32_t>(); }
TEST(Any, encode_decode_int32) { test_enc_dec_number<int32_t>(); }

TEST(Any, encode_decode_uint64) { test_enc_dec_number<uint64_t>(); }
TEST(Any, encode_decode_int64) { test_enc_dec_number<int64_t>(); }

TEST(Any, encode_decode_double) { test_enc_dec_number<double>(); }

TEST(Any, encode_decode_bool) {
  test_enc_dec(true);
  test_enc_dec(false);
}

TEST(Any, encode_decode_string) {
  test_enc_dec(std::string{"https://http.cat/status/510"});
}

TEST(Any, encode_decode_vector_simple) {
  std::vector<std::string> vec{"https://http.cat/status/200",
                               "https://http.cat/status/400",
                               "https://http.cat/status/510"};
  test_enc_dec(vec);
}

TEST(Any, encode_decode_vector_complex1) {
  std::vector<std::vector<std::string>> vec{
      {"https://http.cat/status/200"},
      {"https://http.cat/status/400", "https://http.cat/status/510"}};
  test_enc_dec(vec);
}

TEST(Any, encode_decode_vector_empty) {
  std::vector<std::string> vec{};
  test_enc_dec(vec);
}

TEST(Any, encode_decode_vector_empty_complex) {
  std::vector<std::vector<std::string>> vec{};
  test_enc_dec(vec);
}

TEST(Any, encode_decode_vector_any_empty) {
  test_enc_dec_f([](DBusMessageIter &iter, DBusMessage *message) {
    std::vector<simppl::dbus::Any> vec{};

    // Encode
    simppl::dbus::Any any = vec;
    EXPECT_TRUE(any.is<std::vector<simppl::dbus::Any>>());
    simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any);

    dbus_message_iter_init(message, &iter);
    simppl::dbus::Any result;
    simppl::dbus::Codec<simppl::dbus::Any>::decode(iter, result);

    EXPECT_EQ(any.containedType, result.containedType);
    EXPECT_EQ(any.containedTypeSignature, result.containedTypeSignature);

    EXPECT_TRUE(result.is<std::vector<simppl::dbus::Any>>());
    std::vector<simppl::dbus::Any> resultVec =
        result.as<std::vector<simppl::dbus::Any>>();

    EXPECT_EQ(vec.size(), resultVec.size());
  });
}

TEST(Any, encode_decode_vector_any) {
  test_enc_dec_f([](DBusMessageIter &iter, DBusMessage *message) {
    std::vector<simppl::dbus::Any> vec{
        std::string{"https://http.cat/status/200"}, static_cast<uint32_t>(25),
        static_cast<double>(42.42)};

    // Encode
    simppl::dbus::Any any = vec;
    EXPECT_TRUE(any.is<std::vector<simppl::dbus::Any>>());
    simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any);

    dbus_message_iter_init(message, &iter);
    simppl::dbus::Any result;
    simppl::dbus::Codec<simppl::dbus::Any>::decode(iter, result);

    EXPECT_EQ(any.containedType, result.containedType);
    EXPECT_EQ(any.containedTypeSignature, result.containedTypeSignature);

    EXPECT_TRUE(result.is<std::vector<simppl::dbus::Any>>());
    std::vector<simppl::dbus::Any> resultVec =
        result.as<std::vector<simppl::dbus::Any>>();

    EXPECT_EQ(vec.size(), resultVec.size());
    EXPECT_TRUE(vec[0].is<std::string>());
    EXPECT_EQ(vec[0].as<std::string>(),
              std::string{"https://http.cat/status/200"});
    EXPECT_TRUE(vec[1].is<uint32_t>());
    EXPECT_EQ(vec[1].as<uint32_t>(), static_cast<uint32_t>(25));
    EXPECT_TRUE(vec[2].is<double>());
    EXPECT_EQ(vec[2].as<double>(), static_cast<double>(42.42));
  });
}

TEST(Any, encode_decode_vector_full) {
  simppl::dbus::Dispatcher d("bus:session");

  std::thread t([]() {
    simppl::dbus::Dispatcher d("bus:session");
    Server s(d);
    d.run();
  });

  simppl::dbus::Stub<test::any::AServer> stub(d, "role");

  // wait for server to get ready
  std::this_thread::sleep_for(200ms);

  simppl::dbus::Any a = stub.getVecEmpty();
  EXPECT_TRUE(a.is<std::vector<std::string>>());
  for (size_t i = 0; i < 10; i++) {
    a = stub.setGet(a);
    EXPECT_TRUE(a.is<std::vector<std::string>>());
  }

  stub.stop(); // stop server
  t.join();
}

TEST(Any, get_vector) {
  std::vector<std::string> vec{"https://http.cat/status/200",
                               "https://http.cat/status/400",
                               "https://http.cat/status/510"};
  simppl::dbus::Any any = vec;
  EXPECT_TRUE(any.is<std::vector<std::string>>());
  std::vector<std::string> resultVec = any.as<std::vector<std::string>>();
  EXPECT_EQ(vec.size(), resultVec.size());
  EXPECT_EQ(vec, resultVec);
}

TEST(Any, get_vector_two_level) {
  std::vector<std::vector<std::string>> vec{
      {"https://http.cat/status/200"},
      {"https://http.cat/status/400", "https://http.cat/status/510"}};
  simppl::dbus::Any any = vec;
  EXPECT_TRUE(any.is<std::vector<std::vector<std::string>>>());
  std::vector<std::vector<std::string>> resultVec =
      any.as<std::vector<std::vector<std::string>>>();
  EXPECT_EQ(vec.size(), resultVec.size());
  EXPECT_EQ(vec, resultVec);
}

TEST(Any, encode_decode_tuple_empty) {
  std::tuple<std::string, std::string> tuple{};
  test_enc_dec(tuple);
}

TEST(Any, encode_decode_tuple) {
  std::tuple<std::string, std::string> tuple = std::make_tuple(
      "https://http.cat/status/200", "https://http.cat/status/400");
  test_enc_dec(tuple);
}

TEST(Any, get_tuple) {
  std::tuple<std::string, std::string, std::string> tuple = std::make_tuple(
      "https://http.cat/status/200", "https://http.cat/status/400",
      "https://http.cat/status/510");
  simppl::dbus::Any any = tuple;
  bool b = any.is<std::tuple<std::string, std::string, std::string>>();
  EXPECT_TRUE(b);
  std::tuple<std::string, std::string, std::string> resultTuple =
      any.as<std::tuple<std::string, std::string, std::string>>();
  EXPECT_EQ(tuple, resultTuple);
}

TEST(Any, get_tuple_two_level) {
  using tType_t = std::tuple<std::tuple<std::string, std::string>,
                             std::tuple<std::string, std::string>>;
  tType_t tuple = std::make_tuple<std::tuple<std::string, std::string>>(
      std::make_tuple<std::string, std::string>("https://http.cat/status/200",
                                                "https://http.cat/status/400"),
      std::make_tuple<std::string, std::string>("https://http.cat/status/404",
                                                "https://http.cat/status/500"));
  simppl::dbus::Any any = tuple;
  EXPECT_TRUE(any.is<tType_t>());
  tType_t resultTuple = any.as<tType_t>();
  EXPECT_EQ(tuple, resultTuple);
}

TEST(Any, encode_decode_tuple_vector) {
  std::tuple<std::vector<uint16_t>,
             std::vector<std::tuple<uint64_t, std::string>>>
      tuple = std::make_tuple<std::vector<uint16_t>,
                              std::vector<std::tuple<uint64_t, std::string>>>(
          {1, 2, 656}, {std::make_tuple<uint64_t, std::string>(4545, "Hallo"),
                        std::make_tuple<uint64_t, std::string>(7777, "Welt")});
  test_enc_dec(tuple);
}

TEST(Any, encode_decode_map_empty) {
  std::map<std::string, std::string> map{};
  test_enc_dec(map);
}

TEST(Any, encode_decode_map) {
  std::map<std::string, std::string> map{
      {"asdasd", "asdasdd"}, {"1111", "222"}, {"58", "555"}};
  test_enc_dec(map);
}

TEST(Any, encode_decode_map_tuple_vector) {
  std::map<std::string, std::tuple<std::vector<std::string>,
                                   std::map<std::string, std::string>>>
      map{{"asdasd", std::make_tuple<std::vector<std::string>,
                                     std::map<std::string, std::string>>(
                         {"a", "b", "c"}, {{"1", "2"}})},
          {"sadasdas", std::make_tuple<std::vector<std::string>,
                                       std::map<std::string, std::string>>(
                           {"g", "r", "t"}, {{"", "8"}})}};
  test_enc_dec(map);
}

TEST(Any, encode_decode_map_tuple) {
  std::map<std::string, std::tuple<uint32_t, std::string>> map{
      {"asdasd", std::make_tuple<uint32_t, std::string>(545, "asdasd")},
      {"tzghjghjgh", std::make_tuple<uint32_t, std::string>(8, "löiko")},
      {"ppp", std::make_tuple<uint32_t, std::string>(556, "asdasd")}};
  test_enc_dec(map);
}

TEST(Any, encode_decode_map_vector) {
  std::map<std::string, std::vector<uint32_t>> map{
      {"asdasd", std::vector<uint32_t>{5, 4, 8}}};
  test_enc_dec(map);
}

TEST(Any, encode_decode_map_tuple_vector_any) {
  test_enc_dec_f([](DBusMessageIter &iter, DBusMessage *message) {
    using full_map_t =
        std::map<std::string,
                 std::tuple<std::vector<std::string>,
                            std::map<std::string, simppl::dbus::Any>>>;
    full_map_t map{
        {"asdasd", std::make_tuple<std::vector<std::string>,
                                   std::map<std::string, simppl::dbus::Any>>(
                       {"a", "b", "c"}, {{"1", static_cast<uint32_t>(55115)}})},
        {"sadasdas",
         std::make_tuple<std::vector<std::string>,
                         std::map<std::string, simppl::dbus::Any>>(
             {"g", "r", "t"},
             std::map<std::string, simppl::dbus::Any>{
                 {"", std::string{"8"}}, {"ww", std::string{"8ssa"}}})}};

    // Encode
    simppl::dbus::Any any = map;
    bool b = any.is<full_map_t>();
    EXPECT_TRUE(b);
    EXPECT_FALSE(any.is<std::vector<simppl::dbus::Any>>());

    simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any);

    dbus_message_iter_init(message, &iter);
    simppl::dbus::Any result;
    simppl::dbus::Codec<simppl::dbus::Any>::decode(iter, result);

    EXPECT_EQ(any.containedType, result.containedType);
    EXPECT_EQ(any.containedTypeSignature, result.containedTypeSignature);

    b = result.is<full_map_t>();
    EXPECT_TRUE(b);

    full_map_t resultMap = result.as<full_map_t>();

    EXPECT_EQ(map.size(), resultMap.size());
  });
}

/// Verify the wire types and decode both forms of UNIX FD variant.
TEST(Any, encode_decode_unix_fd_variants) {
  using simppl::dbus::Any;
  using simppl::dbus::Codec;
  using simppl::dbus::FileDescriptor;
  using Options = std::map<std::string, Any>;

  const int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
  ASSERT_GE(fd, 0);
  const FileDescriptor descriptor(fd);
  const Options options{{"array", std::vector<FileDescriptor>{descriptor}},
                        {"single", descriptor}};

  test_enc_dec_f([&options](DBusMessageIter &iter, DBusMessage *message) {
    Codec<Options>::encode(iter, options);
    ASSERT_TRUE(dbus_message_has_signature(message, "a{sv}"));
    ASSERT_TRUE(dbus_message_iter_init(message, &iter));

    DBusMessageIter arrayIter{};
    DBusMessageIter entryIter{};
    DBusMessageIter variantIter{};
    dbus_message_iter_recurse(&iter, &arrayIter);
    dbus_message_iter_recurse(&arrayIter, &entryIter);
    ASSERT_TRUE(dbus_message_iter_next(&entryIter));
    ASSERT_EQ(DBUS_TYPE_VARIANT, dbus_message_iter_get_arg_type(&entryIter));
    dbus_message_iter_recurse(&entryIter, &variantIter);
    EXPECT_EQ(DBUS_TYPE_ARRAY, dbus_message_iter_get_arg_type(&variantIter));
    EXPECT_EQ(DBUS_TYPE_UNIX_FD,
              dbus_message_iter_get_element_type(&variantIter));

    Options decoded;
    Codec<Options>::decode(iter, decoded);
    ASSERT_EQ(2u, decoded.size());
    EXPECT_EQ("ah", decoded.at("array").containedTypeSignature);
    EXPECT_EQ("h", decoded.at("single").containedTypeSignature);
    const std::vector<FileDescriptor> array =
        decoded.at("array").as<std::vector<FileDescriptor>>();
    ASSERT_EQ(1u, array.size());
    EXPECT_NE(-1, fcntl(array.front().native_handle(), F_GETFD));
    const FileDescriptor single = decoded.at("single").as<FileDescriptor>();
    EXPECT_NE(-1, fcntl(single.native_handle(), F_GETFD));
  });
}

namespace {

/// Closes and releases one end of a private peer-to-peer connection.
struct ConnectionDeleter {
  void operator()(DBusConnection *connection) const {
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
  }
};
using ConnectionPtr = std::unique_ptr<DBusConnection, ConnectionDeleter>;

/// Releases a libdbus message and the descriptors it still owns.
struct MessageDeleter {
  void operator()(DBusMessage *message) const { dbus_message_unref(message); }
};
using MessagePtr = std::unique_ptr<DBusMessage, MessageDeleter>;

/// Owns two private libdbus peers and drives their I/O without a message bus.
class UnixFdPeers {
public:
  UnixFdPeers() = default;
  UnixFdPeers(const UnixFdPeers &) = delete;
  UnixFdPeers &operator=(const UnixFdPeers &) = delete;

  ~UnixFdPeers() {
    if (m_listener != nullptr) {
      dbus_server_disconnect(m_listener);
      dbus_server_unref(m_listener);
    }
  }

  /// Connect, authenticate, and negotiate UNIX FD passing on both ends.
  void connect() {
    m_listener = dbus_server_listen("unix:tmpdir=/tmp", nullptr);
    ASSERT_NE(nullptr, m_listener) << "Could not create the D-Bus listener";
    dbus_server_set_new_connection_function(m_listener, accept, this, nullptr);
    ASSERT_TRUE(dbus_server_set_watch_functions(
        m_listener, addWatch, removeWatch, nullptr, this, nullptr));

    char *address = dbus_server_get_address(m_listener);
    ASSERT_NE(nullptr, address);
    m_sender.reset(dbus_connection_open_private(address, nullptr));
    dbus_free(address);
    ASSERT_NE(nullptr, m_sender.get()) << "Could not connect to the listener";

    for (int attempt = 0; attempt < maxAttempts && !m_receiver; ++attempt) {
      pollListener();
    }
    ASSERT_NE(nullptr, m_receiver.get()) << "Listener did not accept the peer";

    for (int attempt = 0; attempt < maxAttempts && !canPassDescriptors();
         ++attempt) {
      dbus_connection_read_write(m_sender.get(), pollMilliseconds);
      dbus_connection_read_write(m_receiver.get(), pollMilliseconds);
    }
    ASSERT_TRUE(canPassDescriptors()) << "UNIX FD negotiation did not finish";
  }

  /// Queue a message, then drive both peers until the receiver gets it.
  MessagePtr transfer(DBusMessage *message) const {
    if (!dbus_connection_send(m_sender.get(), message, nullptr)) {
      return {};
    }
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
      dbus_connection_read_write(m_sender.get(), pollMilliseconds);
      dbus_connection_read_write(m_receiver.get(), pollMilliseconds);
      MessagePtr received(dbus_connection_pop_message(m_receiver.get()));
      if (received) {
        return received;
      }
    }
    return {};
  }

private:
  static constexpr int maxAttempts = 100;
  static constexpr int pollMilliseconds = 10;

  /// Drive the listener until it accepts the single test connection.
  void pollListener() {
    const std::vector<DBusWatch *> watches = m_watches;
    for (DBusWatch *watch : watches) {
      if (!dbus_watch_get_enabled(watch)) {
        continue;
      }
      pollfd descriptor{dbus_watch_get_unix_fd(watch), POLLIN, 0};
      if (poll(&descriptor, 1, pollMilliseconds) > 0 &&
          (descriptor.revents & POLLIN) != 0) {
        dbus_watch_handle(watch, DBUS_WATCH_READABLE);
        return;
      }
    }
  }

  bool canPassDescriptors() const {
    return dbus_connection_can_send_type(m_sender.get(), DBUS_TYPE_UNIX_FD) &&
           dbus_connection_can_send_type(m_receiver.get(), DBUS_TYPE_UNIX_FD);
  }

  static dbus_bool_t addWatch(DBusWatch *watch, void *data) {
    auto &peers = *static_cast<UnixFdPeers *>(data);
    try {
      peers.m_watches.push_back(watch);
    } catch (const std::bad_alloc &) {
      return FALSE;
    }
    return TRUE;
  }

  static void removeWatch(DBusWatch *watch, void *data) {
    auto &watches = static_cast<UnixFdPeers *>(data)->m_watches;
    watches.erase(std::remove(watches.begin(), watches.end(), watch),
                  watches.end());
  }

  static void accept(DBusServer *, DBusConnection *connection, void *data) {
    // libdbus closes the connection unless the callback keeps a reference.
    static_cast<UnixFdPeers *>(data)->m_receiver.reset(
        dbus_connection_ref(connection));
  }

  DBusServer *m_listener{nullptr};
  std::vector<DBusWatch *> m_watches;
  ConnectionPtr m_sender;
  ConnectionPtr m_receiver;
};

} // namespace

/// Send an FD nested in a{sv} through the kernel and check both owners.
TEST(Any, unix_fd_variant_round_trip_over_socket) {
  using simppl::dbus::Any;
  using simppl::dbus::Codec;
  using simppl::dbus::FileDescriptor;
  using Options = std::map<std::string, Any>;

  UnixFdPeers peers;
  ASSERT_NO_FATAL_FAILURE(peers.connect());

  const std::string payload{"simppl nested unix fd payload"};
  const ssize_t payloadSize = static_cast<ssize_t>(payload.size());

  const int senderFd = memfd_create("simppl-any-fd", MFD_CLOEXEC);
  ASSERT_GE(senderFd, 0);
  FileDescriptor senderDescriptor(senderFd);
  ASSERT_EQ(payloadSize, write(senderFd, payload.data(), payload.size()));
  ASSERT_EQ(0, lseek(senderFd, 0, SEEK_SET));

  MessagePtr request(dbus_message_new_method_call(
      nullptr, "/org/example/TargetObject", "org.example.TargetInterface",
      "TargetMethod"));
  ASSERT_NE(nullptr, request.get());
  dbus_message_set_no_reply(request.get(), TRUE);
  {
    const Options options{
        {"fds", std::vector<FileDescriptor>{senderDescriptor}}};
    DBusMessageIter iter;
    dbus_message_iter_init_append(request.get(), &iter);
    Codec<Options>::encode(iter, options);
  }
  MessagePtr received = peers.transfer(request.get());
  request.reset();
  ASSERT_NE(nullptr, received.get()) << "The receiver did not get the message";
  ASSERT_TRUE(dbus_message_has_signature(received.get(), "a{sv}"));

  {
    Options decoded;
    DBusMessageIter iter;
    ASSERT_TRUE(dbus_message_iter_init(received.get(), &iter));
    Codec<Options>::decode(iter, decoded);
    received.reset();

    const std::vector<FileDescriptor> receivedDescriptors =
        decoded.at("fds").as<std::vector<FileDescriptor>>();
    ASSERT_EQ(1u, receivedDescriptors.size());
    const int receivedFd = receivedDescriptors.front().native_handle();
    EXPECT_NE(senderFd, receivedFd);
    ASSERT_NE(-1, fcntl(receivedFd, F_GETFD));

    struct stat senderStat {};
    struct stat receivedStat {};
    ASSERT_EQ(0, fstat(senderFd, &senderStat));
    ASSERT_EQ(0, fstat(receivedFd, &receivedStat));
    EXPECT_EQ(senderStat.st_dev, receivedStat.st_dev);
    EXPECT_EQ(senderStat.st_ino, receivedStat.st_ino);

    // read() uses the shared file offset, which the sender rewound to zero.
    std::string receivedPayload(payload.size(), '\0');
    ASSERT_EQ(payloadSize, read(receivedFd, receivedPayload.data(),
                                receivedPayload.size()));
    EXPECT_EQ(payload, receivedPayload);
  }

  // Encoding and receiver cleanup must not transfer or close the sender FD.
  EXPECT_EQ(senderFd, senderDescriptor.native_handle());
  ASSERT_NE(-1, fcntl(senderFd, F_GETFD));
  std::string senderPayload(payload.size(), '\0');
  ASSERT_EQ(payloadSize,
            pread(senderFd, senderPayload.data(), senderPayload.size(), 0));
  EXPECT_EQ(payload, senderPayload);

  senderDescriptor = FileDescriptor();
  errno = 0;
  EXPECT_EQ(-1, fcntl(senderFd, F_GETFD));
  EXPECT_EQ(EBADF, errno);
}

TEST(Any, encode_decode_object_path) {
  simppl::dbus::ObjectPath objPath("/org/example/TargetObject");
  test_enc_dec(objPath);
}

TEST(Any, encode_decode_variant) {
  std::variant<std::string, uint32_t> var{"hallo"};
  test_enc_dec(var);
}

struct TestStruct1 {
  using serializer_type =
      typename simppl::dbus::make_serializer<uint8_t, std::string>::type;

  uint8_t u1;
  std::string s2;

  bool operator==(const TestStruct1 &other) const {
    return u1 == other.u1 && s2 == other.s2;
  }
};

TEST(Any, encode_decode_struct) {
  TestStruct1 s1{42, "Olla"};
  test_enc_dec(s1);
}
