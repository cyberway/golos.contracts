#pragma once

#include <tuple>
#include <utility>

namespace golos {


template<typename T, typename... Ts>
auto tuple_head(std::tuple<T,Ts...> t) {
    return std::get<0>(t);
}

template<std::size_t... Ns, typename... Ts>
auto tuple_tail_impl(std::index_sequence<Ns...>, std::tuple<Ts...> t) {
    return std::make_tuple(std::get<Ns + 1u>(t)...);
}

template<typename... Ts>
auto tuple_tail(std::tuple<Ts...> t) {
    return tuple_tail_impl(std::make_index_sequence<sizeof...(Ts) - 1u>(), t);
}


} // namespace golos
