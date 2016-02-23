#include <gtest/gtest.h>

#include "simppl/variant.h"

namespace {
   
   int constructs = 0;
   int destructs = 0;
   
   struct TestHelper
   {
      TestHelper()
      {
         ++constructs;
      }
      
      TestHelper(const TestHelper&)
      {
         ++constructs;
      }
      
      ~TestHelper()
      {
         ++destructs;
      }
   };
   
}


TEST(Variant, basic)
{
   simppl::Variant<int, double, std::string, TestHelper> v;
   
   v = 42;
   EXPECT_EQ(42, *v.get<int>());
   
   v = std::string("Hallo Welt");
   EXPECT_EQ(std::string("Hallo Welt"), *v.get<std::string>());
   
   v = TestHelper();
   
   v = 43;
   EXPECT_EQ(43, *v.get<int>());
   
   EXPECT_EQ(2, constructs);
   EXPECT_EQ(2, destructs);
}
