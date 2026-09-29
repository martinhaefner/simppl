#include "simppl/stubbase.h"


simppl::dbus::detail::GetAllProperties::GetAllProperties(simppl::dbus::StubBase& stub, const RequestOptions& opts)
 : stub_(stub)
 , opts_(opts)
{
    // NOOP
}


simppl::dbus::detail::GetAllProperties simppl::dbus::detail::GetAllProperties::operator[](const RequestOptions& opts)
{
    return GetAllProperties(stub_, opts);
}


void simppl::dbus::detail::GetAllProperties::operator()()
{
    stub_.get_all_properties_request(opts_.timeout_);
}


simppl::dbus::detail::GetAllProperties::getall_properties_holder_type
simppl::dbus::detail::GetAllProperties::async()
{
    return stub_.get_all_properties_request_async(opts_.timeout_);
}
