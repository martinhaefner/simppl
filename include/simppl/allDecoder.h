#pragma once

#include <cassert>
#include <dbus/dbus-protocol.h>
#include <dbus/dbus.h>
#include <any>
#include <type_traits>
#include <vector>
#include <string>

namespace simppl
{
namespace dbus
{
    struct Any2;
    struct AnyVec;
    
    template<typename T> T decode2(DBusMessageIter& iter);
    template<> AnyVec decode2(DBusMessageIter& iter);
    template<> Any2 decode2(DBusMessageIter& iter);
    template<> std::string decode2(DBusMessageIter& iter);

    struct Any2 {
        // DBUS_TYPE_...
        int containedType{DBUS_TYPE_INVALID};
        std::any value_;

        template<typename T> T as() const;
        template<typename T> bool is() const;
    };

    struct AnyVec {
        struct Element {
            // DBUS_TYPE_...
            int containedType{DBUS_TYPE_INVALID};
            std::any value_;
        };
        std::vector<Element> vec;
    };

    template<typename T> struct is_type {
        bool check(const std::any& any) {
            if(any.type() == typeid(Any2)) {
                return is_type<T>().check(std::any_cast<Any2>(any).value_);
            }
            if(any.type() == typeid(AnyVec)) {
                return false;
            }
            return any.type() == typeid(T);
        }
    };

    template<typename T, typename Alloc> struct is_type<std::vector<T, Alloc>> {
        bool check(const std::any& any) {
            if(any.type() != typeid(AnyVec)) {
                return false;
            }

            for (const AnyVec::Element& elem : std::any_cast<AnyVec>(any).vec) {
                if(!is_type<T>().check(elem.value_)) {
                    return false;
                }
            }
            return true;
        }
    };

    template<typename T> struct as_type {
        T convert(const std::any& any) {
            if(any.type() == typeid(Any2)) {
                return as_type<T>().convert(std::any_cast<Any2>(any).value_);
            }
            if(any.type() == typeid(AnyVec)) {
                assert(false);
            }
            return std::any_cast<T>(any);
        }
    };

    template<typename T, typename Alloc> struct as_type<std::vector<T, Alloc>> {
        std::vector<T, Alloc> convert(const std::any& any) {
            if(any.type() != typeid(AnyVec)) {
                assert(false);
            }

            std::vector<T, Alloc> result;
            for (const AnyVec::Element& elem : std::any_cast<AnyVec>(any).vec) {
                assert(elem.value_.type() == typeid(T));
                result.emplace_back(std::any_cast<T>(elem.value_));
            }
            return result;
        }
    };

    // Needs to be implemented inside the header file.
    template<typename T> bool Any2::is() const {
            return is_type<T>().check(value_);
    }

    template<typename T> T Any2::as() const {
        return as_type<T>().convert(value_);
    }
}
}