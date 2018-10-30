#pragma once
#include "types.h"
#include "config.hpp"
#include "objects.hpp"
#include <common/calclib/atmsp_storable.h>

using namespace golos::config;

template<typename T, typename F>
auto get_itr(T& tbl, uint64_t key, account_name payer, F&& insert_fn) {
    auto itr = tbl.find(key);
    if(itr == tbl.end())
        itr = tbl.emplace(payer, insert_fn);
    return itr;
}

fixp_t set_and_run(atmsp::machine<fixp_t>& machine, const atmsp::storable::bytecode& bc, const std::vector<fixp_t>& args, const std::vector<std::pair<fixp_t, fixp_t> >& domain = {}) {
    bc.to_machine(machine);
    return machine.run(args, domain);
}

fixp_t add_cut(fixp_t lhs, fixp_t rhs) {
    return fp_cast<fixp_t>(elap_t(lhs) + elap_t(rhs), false);
}

elaf_t get_limit_prop(int64_t arg) {
    eosio_assert(arg >= 0, "get_limit_prop: arg < 0");
    return elaf_t(elai_t(std::min(arg, ONE_HUNDRED_PERCENT)) / elai_t(ONE_HUNDRED_PERCENT));
}

fixp_t get_prop(int64_t arg) {
    return fp_cast<fixp_t>(elai_t(arg) / elai_t(ONE_HUNDRED_PERCENT));
}

void check_positive_monotonic(atmsp::machine<fixp_t>& machine, fixp_t max_arg, const std::string& name, bool inc) {

    fixp_t prev_res = machine.run({max_arg});
    if(!inc)
        eosio_assert(prev_res >= fixp_t(0), ("check positive failed for " + name).c_str());
    fixp_t cur_arg = max_arg;
    for(size_t i = 0; i < CHECK_MONOTONIC_STEPS; i++) {
        cur_arg /= fixp_t(2);
        fixp_t cur_res = machine.run({cur_arg});
        eosio_assert(inc ? (cur_res <= prev_res) : (cur_res >= prev_res), ("check monotonic failed for " + name).c_str());
        prev_res = cur_res;
    }
    fixp_t res_zero = machine.run({fixp_t(0)});
    if(inc)
        eosio_assert(res_zero >= fixp_t(0), ("check positive failed for " + name).c_str());
    eosio_assert(inc ? (res_zero <= prev_res) : (res_zero >= prev_res), ("check monotonic [0] failed for " + name).c_str());
}
