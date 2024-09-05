#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>
#include "simppl/type_mapper.h"
#include "dbus/dbus.h"

TEST(TypeMapper, int32_t)
{
    ASSERT_EQ(simppl::dbus::get_debus_type<int32_t>(), DBUS_TYPE_INT32);
}

TEST(TypeMapper, string)
{
    ASSERT_EQ(simppl::dbus::get_debus_type<std::string>(), DBUS_TYPE_STRING);
}

TEST(TypeMapper, vector)
{
    ASSERT_EQ(simppl::dbus::get_debus_type<std::vector<std::string>>(), DBUS_TYPE_ARRAY);
}

TEST(TypeMapper, vectorRef)
{
    ASSERT_EQ(simppl::dbus::get_debus_type<std::vector<std::string>&>(), DBUS_TYPE_ARRAY);
}
