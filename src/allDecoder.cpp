#include "simppl/allDecoder.h"
#include <any>
#include <cstdint>
#include <dbus/dbus-protocol.h>
#include <dbus/dbus.h>
#include <string>
#include <cassert>
#include <type_traits>
#include <vector>

namespace simppl
{
namespace dbus
{
    std::any decodeAsAny(DBusMessageIter& varIter, int type) {
        switch (type) {
            case DBUS_TYPE_INT32:
                return decode2<int32_t>(varIter);
            case DBUS_TYPE_STRING:
                return decode2<std::string>(varIter);
            case DBUS_TYPE_VARIANT:
                return decode2<Any2>(varIter);
            case DBUS_TYPE_ARRAY:
                return decode2<AnyVec>(varIter);
            default:
                assert(false);
        }
    }

    template<> AnyVec decode2(DBusMessageIter& iter) {
        DBusMessageIter arrayIter;
        dbus_message_iter_recurse(&iter, &arrayIter);

        std::vector<AnyVec::Element> array;
        int elementType = dbus_message_iter_get_arg_type(&arrayIter);
        if(elementType != DBUS_TYPE_INVALID) {
            do {
                array.emplace_back(AnyVec::Element{elementType, decodeAsAny(arrayIter, elementType)});
            } while (dbus_message_iter_next(&arrayIter));
        }
        return AnyVec{array};
    }

    template<> Any2 decode2(DBusMessageIter& iter) {
        DBusMessageIter varIter;
        dbus_message_iter_recurse(&iter, &varIter);

        int type = dbus_message_iter_get_arg_type(&varIter);
        return {type, decodeAsAny(varIter, type)};
    }

    template<> std::string decode2(DBusMessageIter& iter) {
        char* value;
        dbus_message_iter_get_basic(&iter, &value);
        return std::string(value);
    }

    template<typename T> T decode2(DBusMessageIter& iter) {
        T value;
        dbus_message_iter_get_basic(&iter, &value);
        return value;
    }
}
}