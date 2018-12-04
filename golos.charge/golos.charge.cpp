#include "golos.charge.hpp"
#include <eosio.token/eosio.token.hpp>
#include <golos.vesting/golos.vesting.hpp>

namespace golos {
using namespace eosio;
using namespace atmsp::storable;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        auto execute_action = [&](const auto fn) {
            return eosio::execute_action(eosio::name(receiver), eosio::name(code), fn);
        };
#define NN(x) N(x).value

    if (NN(use) == action)
        execute_action(&charge::use);
    else if (NN(setrestorer) == action)
        execute_action(&charge::set_restorer);
    }
}

static constexpr auto max_arg = static_cast<uint64_t>(std::numeric_limits<fixp_t>::max());

void charge::use(name user, symbol_code token_code, unsigned char suffix, uint64_t price_arg, uint64_t cutoff_arg) {
    eosio_assert(price_arg <= cutoff_arg, "price > cutoff");
    eosio_assert(price_arg <= max_arg, "price > max_input");
    eosio_assert(cutoff_arg <= max_arg, "cutoff > max_input");
        
    fixp_t price = price_arg;
    fixp_t cutoff = cutoff_arg;
    
    auto issuer = name(token::get_issuer(config::token_name, token_code));
    require_auth(issuer);
    auto charge_symbol = symbol(token_code, suffix);
    balances balances_table(_self, user.value);
    balances::const_iterator itr = balances_table.find(charge_symbol.raw());
    auto new_val = calc_value(user, token_code, balances_table, itr, price);
    eosio_assert(new_val <= cutoff, "new_val > cutoff");
    if (itr == balances_table.end()) {
        if(new_val > 0)
            balances_table.emplace(issuer, [&]( auto &item ) {
                item.charge_symbol = charge_symbol.raw();
                item.last_update = current_time();
                item.value = new_val.data();
            });
        return;
    }
    if (new_val > 0)
        balances_table.modify(*itr, issuer, [&]( auto &item ) {
            item.last_update = current_time();
            item.value = new_val.data();
        });
    else
        balances_table.erase(itr);
}

void charge::set_restorer(symbol_code token_code, unsigned char suffix, std::string func_str,
        uint64_t max_prev, uint64_t max_vesting, uint64_t max_elapsed) {
    
    eosio_assert(max_prev <= max_arg, "max_prev > max_input");
    eosio_assert(max_vesting <= max_arg, "max_vesting > max_input");
    eosio_assert(max_elapsed <= max_arg, "max_elapsed > max_input");
    auto issuer = name(token::get_issuer(config::token_name, token_code));
    require_auth(issuer);
    auto charge_symbol = symbol(token_code, suffix);
    restorers restorers_table(_self, _self.value);
    auto itr = restorers_table.find(charge_symbol.raw());
    
    atmsp::parser<fixp_t> pa;
    atmsp::machine<fixp_t> machine;
    
    bytecode func;
    pa(machine, func_str, "p,v,t");//prev value, vesting, time(elapsed seconds)
    func.from_machine(machine);
    
    if (itr != restorers_table.end())
        restorers_table.modify(*itr, issuer, [&]( auto &item ) {
             item.func = func;
             item.max_prev = fixp_t(max_prev).data();
             item.max_vesting = fixp_t(max_vesting).data();
             item.max_elapsed = fixp_t(max_elapsed).data();
        });
    else
        restorers_table.emplace(issuer, [&]( auto &item ) {
             item.charge_symbol = charge_symbol.raw(); 
             item.func = func;
             item.max_prev = fixp_t(max_prev).data();
             item.max_vesting = fixp_t(max_vesting).data();
             item.max_elapsed = fixp_t(max_elapsed).data();
        });
}

fixp_t charge::calc_value(name user, symbol_code token_code, balances& balances_table, balances::const_iterator& itr, fixp_t price) const {
    if (itr == balances_table.end())
        return price;
    auto cur_time = current_time();
    eosio_assert(cur_time >= itr->last_update, "LOGIC ERROR! charge::calc_value: cur_time < itr->last_update");
    fixp_t restored = fixp_t(0);
    if (cur_time > itr->last_update) {
        
        restorers restorers_table(_self, _self.value);
        auto restorer_itr = restorers_table.find(itr->charge_symbol);
        eosio_assert(restorer_itr != restorers_table.end(), "charge::calc_value restorer_itr == restorers_table.end()");
        atmsp::machine<fixp_t> machine;
        
        int64_t elapsed_seconds = static_cast<int64_t>((cur_time - itr->last_update) / eosio::seconds(1).count());        
        auto prev = FP(itr->value);
        int64_t vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, user, token_code).amount;
        restorer_itr->func.to_machine(machine);
        restored = machine.run(
            {prev, fp_cast<fixp_t>(vesting, false), fp_cast<fixp_t>(elapsed_seconds, false)}, {
                {fixp_t(0), FP(restorer_itr->max_prev)}, 
                {fixp_t(0), FP(restorer_itr->max_vesting)}, 
                {fixp_t(0), FP(restorer_itr->max_elapsed)}
            });
    }
    
    return fp_cast<fixp_t>((elap_t(FP(itr->value)) - elap_t(restored)) + elap_t(price));
}
} /// namespace golos

