#include <gtest/gtest.h>

#include "simppl/detail/util.h"

namespace test
{

TEST(Utils, extract_interface)
{
   auto interface = simppl::dbus::detail::extract_interfaces(1,
	"simppl::make_typelist<simppl::example::EchoService<0, simppl::dbus::SkeletonBase, simppl::dbus::ServerMethod, simppl::dbus::ServerSignal, simppl::dbus::ServerProperty, simppl::dbus::SkeletonBase> >");
   ASSERT_EQ(1, interface.size());
   EXPECT_EQ(interface[0], "simppl.example.EchoService");
}

}
