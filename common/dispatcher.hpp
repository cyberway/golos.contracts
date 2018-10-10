#pragma once

#include "tuple_helper.hpp"

namespace golos {

using namespace eosio;

#ifndef DOMAIN_TYPE
#define DOMAIN_TYPE symbol_type
#pragma message ("No DOMAIN_TYPE defined for `execute_action`, using `symbol_type`")
#endif

// todo: make configurable app_domain type
template<typename T, typename Q, typename... Args>
bool execute_action(uint64_t action, void (Q::*func)(Args...)) {
// bool execute_action(T* obj, void (Q::*func)(Args...)) {
    size_t size = action_data_size();
    //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
    constexpr size_t max_stack_buffer_size = 512;
    void* buffer = nullptr;
    if (size > 0) {
        buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
        read_action_data(buffer, size);
    }

    // auto args = unpack<std::tuple<std::decay_t<Args>...>>((char*)buffer, size);
    auto args = unpack<std::tuple<DOMAIN_TYPE/* app domain */, std::decay_t<Args>.../* function args */>>((char*)buffer, size);

    if (max_stack_buffer_size < size) {
        free(buffer);
    }

    T obj(current_receiver(), tuple_head(args), action);    // pass action to constructor

    auto f2 = [&](auto... a) {
        (obj.*func)(a...);
    };

    // boost::mp11::tuple_apply(f2, args);
    boost::mp11::tuple_apply(f2, tuple_tail(args));
    return true;
}

// `action` var is visible from here. TODO: find better way
#define APP_DOMAIN_API_CALL(r, TYPENAME, elem)              \
    case ::eosio::string_to_name(BOOST_PP_STRINGIZE(elem)): \
        ::golos::execute_action<TYPENAME>(action, &TYPENAME::elem); \
        break;

#define APP_DOMAIN_API(TYPENAME, MEMBERS) \
    BOOST_PP_SEQ_FOR_EACH(APP_DOMAIN_API_CALL, TYPENAME, MEMBERS)

#define APP_DOMAIN_ABI(TYPENAME, MEMBERS)                                                                                        \
    extern "C" {                                                                                                                 \
        void apply(uint64_t receiver, uint64_t code, uint64_t action) {                                                          \
            auto self = receiver;                                                                                                \
            if (action == N(onerror)) {                                                                                          \
                /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
                eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");             \
            }                                                                                                                    \
            if (code == self || action == N(onerror)) {                                                                          \
                switch (action) {                                                                                                \
                    APP_DOMAIN_API(TYPENAME, MEMBERS)                                                                            \
                    /*default:                                                                                                     \
                        eosio_assert(false, "invalid action");*/                                                                   \
                    break;                                                                                                       \
                }                                                                                                                \
                /* does not allow destructor of thiscontract to run: eosio_exit(0); */                                           \
            }                                                                                                                    \
        }                                                                                                                        \
    }

} // namespace golos
