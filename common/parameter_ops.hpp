#pragma once
#include "parameter.hpp"
#include "parameter_ops_spec.hpp"
#include <eosiolib/datastream.hpp>
#include <variant>

namespace golos {


// variant deserializer don't implemented in CDT 1.2.x
template<int I, typename Stream, typename... Ts>
void deserialize(datastream<Stream>& ds, std::variant<Ts...>& var, int i) {
    if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
        if (i == I) {
            std::variant_alternative_t<I, std::variant<Ts...>> tmp;
            ds >> tmp;
            var = std::move(tmp);
        } else {
            deserialize<I+1>(ds,var,i);
        }
    } else {
        eosio_assert(false, "invalid variant index");
    }
}

template<typename Stream, typename... Ts>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, std::variant<Ts...>& var) {
    unsigned_int index;
    ds >> index;
    deserialize<0>(ds,var,index);
    return ds;
}


////////////////////////////////////////////////////////////////////////////////
// param wrapper ops

// compare
template<typename T, int I>
bool operator<(const param_wrapper<T,I>& a, const param_wrapper<T,I>& b) {
    auto l = to_tuple(a, nop_t<I>{});
    auto r = to_tuple(b, nop_t<I>{});
    return l < r;
}

template<typename T, int I>
bool operator==(const param_wrapper<T,I>& a, const param_wrapper<T,I>& b) {
    auto l = to_tuple(a, nop_t<I>{});
    auto r = to_tuple(b, nop_t<I>{});
    return l == r;
}
template<typename T, int I>
bool operator==(const T& a, const param_wrapper<T,I>& b) {
    auto l = to_tuple(a, nop_t<I>{});
    auto r = to_tuple(b, nop_t<I>{});
    return l == r;
}
template<typename T, int I>
bool operator!=(const param_wrapper<T,I>& a, const param_wrapper<T,I>& b) {
    return !(a == b);
}
template<typename T, int I>
bool operator!=(const T& a, const param_wrapper<T,I>& b) {
    return !(a == b);
}

// deserialize
template<typename Stream, typename T, int I>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, param_wrapper<T,I>& var) {
    T obj;
    // auto fields = to_tuple(obj, nop_t<I>{});
    // ds >> fields;    // need tuple of references for that
    deserialize(ds, obj, nop_t<I>{});
    var = std::move(obj);
    return ds;
}

template<typename Stream, typename T, int I>
inline datastream<Stream>& operator<<(datastream<Stream>& ds, const param_wrapper<T,I>& var) {
    auto fields = to_tuple(var, nop_t<I>{});
    ds << fields;
    return ds;
}


} // golos
