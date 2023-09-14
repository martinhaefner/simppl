#include <any>
#include <cstdint>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "simppl/interface.h"
#include "simppl/serialization.h"
#include "simppl/skeleton.h"
#include "simppl/stub.h"

#include "simppl/any.h"
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
                    EXPECT_EQ(3, result.size());
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

  EXPECT_EQ(3, result.size());
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
    simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any);

    dbus_message_iter_init(message, &iter);
    simppl::dbus::Any result;
    simppl::dbus::Codec<simppl::dbus::Any>::decode(iter, result);
    EXPECT_TRUE(result.is<T>());

    EXPECT_EQ(any.containedType, result.containedType);
    EXPECT_EQ(any.containedTypeSignature, result.containedTypeSignature);
    EXPECT_TRUE(result.is<T>());
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
    EXPECT_THROW(simppl::dbus::Codec<simppl::dbus::Any>::encode(iter, any), std::invalid_argument);
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

TEST(Any, get_vec) {
  std::vector<std::string> vec{"https://http.cat/status/200",
                               "https://http.cat/status/400",
                               "https://http.cat/status/510"};
  simppl::dbus::Any any = vec;
  EXPECT_TRUE(any.is<std::vector<std::string>>());
  std::vector<std::string> resultVec = any.as<std::vector<std::string>>();
  EXPECT_EQ(vec.size(), resultVec.size());
  EXPECT_EQ(vec, resultVec);
}

TEST(Any, get_vec_two_level) {
  std::vector<std::vector<std::string>> vec{
      {"https://http.cat/status/200"},
      {"https://http.cat/status/400", "https://http.cat/status/510"}};
  simppl::dbus::Any any = vec;
  EXPECT_TRUE(any.is<std::vector<std::vector<std::string>>>());
  std::vector<std::vector<std::string>> resultVec = any.as<std::vector<std::vector<std::string>>>();
  EXPECT_EQ(vec.size(), resultVec.size());
  EXPECT_EQ(vec, resultVec);
}
