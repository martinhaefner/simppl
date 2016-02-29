#include <gtest/gtest.h>

#include <map>

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


TEST(Variant, map)
{
   simppl::Variant<std::map<int, std::string> > v;
   
   std::map<int, std::string> m {
      { 1, "Hallo" }, 
      { 2, "Welt" }
   };
   
   v = m;
   EXPECT_EQ(2, (v.get<std::map<int, std::string>>()->size()));
   
   int i=0;
   for(auto& e : *v.get<std::map<int, std::string>>())
   {
      if (i == 0)
      {
         EXPECT_EQ(1, e.first);
         EXPECT_EQ(std::string("Hallo"), e.second);
      }
      else if (i == 1)
      {
         EXPECT_EQ(2, e.first);
         EXPECT_EQ(std::string("Welt"), e.second);
      }
      
      ++i;
   }
   
   EXPECT_EQ(2, i);
}
