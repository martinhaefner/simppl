#include "simppl/stubbase.h"
#include "simppl/timeout.h"


simppl::dbus::detail::GetAllProperties::GetAllProperties(simppl::dbus::StubBase& stub)
 : stub_(stub)
{
    // NOOP
}


simppl::dbus::detail::GetAllProperties& simppl::dbus::detail::GetAllProperties::operator[](int flags)
{
    if (flags & (1<<0))
        detail::request_specific_timeout = timeout.timeout_;

    return *this;
}


void simppl::dbus::detail::GetAllProperties::operator()()
{
    stub_.get_all_properties_request();
}


simppl::dbus::detail::GetAllProperties::getall_properties_holder_type
simppl::dbus::detail::GetAllProperties::async()
{
    return stub_.get_all_properties_request_async();
}
