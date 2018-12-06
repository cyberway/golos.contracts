#include "golos.charge.hpp"
#include <eosio.token/eosio.token.hpp>
#include <golos.vesting/golos.vesting.hpp>

namespace golos {
using namespace eosio;
using namespace atmsp::storable;

static constexpr auto max_arg = static_cast<uint64_t>(std::numeric_limits<fixp_t>::max());

static fixp_t to_fixp(int64_t arg) {
    return fp_cast<fixp_t>(elai_t(arg) / elai_t(config::_100percent));
}

static int64_t from_fixp(fixp_t arg) {
    return fp_cast<int64_t>(elap_t(arg) * elai_t(config::_100percent), false);
}

fixp_t charge::use_helper(name issuer, name user, symbol_code token_code, uint8_t charge_id, int64_t price_arg, int64_t cutoff_arg) {
    eosio_assert(cutoff_arg < 0 || price_arg <= cutoff_arg, "price > cutoff");
    eosio_assert(price_arg <= max_arg, "price > max_input");
    eosio_assert(cutoff_arg < 0 || cutoff_arg <= max_arg, "cutoff > max_input");
    require_auth(issuer);
    fixp_t price = to_fixp(price_arg);
    
    auto charge_symbol = symbol(token_code, charge_id);
    balances balances_table(_self, user.value);
    balances::const_iterator itr = balances_table.find(charge_symbol.raw());
    auto new_val = calc_value(user, token_code, balances_table, itr, price);
    eosio_assert(cutoff_arg < 0 || new_val <= to_fixp(cutoff_arg), "new_val > cutoff");
    if (itr == balances_table.end()) {
        if (new_val > 0)
            balances_table.emplace(issuer, [&]( auto &item ) {
                item.charge_symbol = charge_symbol.raw();
                item.token_code = token_code;
                item.charge_id = charge_id;
                item.last_update = current_time();
                item.value = new_val.data();
            });
        return new_val;
    }
    if (new_val > 0)
        balances_table.modify(*itr, issuer, [&]( auto &item ) {
            item.last_update = current_time();
            item.value = new_val.data();
        });
    else
        balances_table.erase(itr);
    return new_val;
}

void charge::use(name user, symbol_code token_code, uint8_t charge_id, int64_t price_arg, int64_t cutoff_arg) {
    use_helper(name(token::get_issuer(config::token_name, token_code)), user, token_code, charge_id, price_arg, cutoff_arg);
}

void charge::useandstore(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id, int64_t price_arg) {
    auto issuer = name(token::get_issuer(config::token_name, token_code));
    auto new_val = use_helper(issuer, user, token_code, charge_id, price_arg);
    
    storedvals storedvals_table(_self, user.value);
    auto storedvals_index = storedvals_table.get_index<N(symbolstamp)>();
    auto k = stored::get_key(token_code, charge_id, stamp_id);
    auto itr = storedvals_index.find(k);
    if (itr != storedvals_index.end())
        storedvals_index.modify(itr, issuer, [&]( auto &item ) { item.value = new_val.data(); });
    else
        storedvals_table.emplace(issuer, [&]( auto &item ) {
            item.id = storedvals_table.available_primary_key();
            item.symbol_stamp = k;
            item.value = new_val.data();
        });
}

void charge::removestored(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id) {
    require_auth(name(token::get_issuer(config::token_name, token_code)));
    storedvals storedvals_table(_self, user.value);
    auto storedvals_index = storedvals_table.get_index<N(symbolstamp)>();
    auto itr = storedvals_index.find(stored::get_key(token_code, charge_id, stamp_id));
    eosio_assert(itr != storedvals_index.end(), "itr == storedvals_index.end()");
    storedvals_index.erase(itr);
}

int64_t charge::get(name code, name user, symbol_code token_code, uint8_t charge_id, uint64_t stamp_id) {
    storedvals storedvals_table(code, user.value);
    auto storedvals_index = storedvals_table.get_index<N(symbolstamp)>();
    auto itr = storedvals_index.find(stored::get_key(token_code, charge_id, stamp_id));
    return itr != storedvals_index.end() ? from_fixp(FP(itr->value)) : -1; //charge can't be negative
}

void charge::setrestorer(symbol_code token_code, uint8_t charge_id, std::string func_str, 
        int64_t max_prev, int64_t max_vesting, int64_t max_elapsed) {
    
    eosio_assert(max_prev <= max_arg, "max_prev > max_input");
    eosio_assert(max_vesting <= max_arg, "max_vesting > max_input");
    eosio_assert(max_elapsed <= max_arg, "max_elapsed > max_input");
    auto issuer = name(token::get_issuer(config::token_name, token_code));
    require_auth(issuer);
    auto charge_symbol = symbol(token_code, charge_id);
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
             item.max_prev = to_fixp(max_prev).data();
             item.max_vesting = to_fixp(max_vesting).data();
             item.max_elapsed = to_fixp(max_elapsed).data();
        });
    else
        restorers_table.emplace(issuer, [&]( auto &item ) {
             item.charge_symbol = charge_symbol.raw();
             item.token_code = token_code;
             item.charge_id = charge_id;
             item.func = func;
             item.max_prev = to_fixp(max_prev).data();
             item.max_vesting = to_fixp(max_vesting).data();
             item.max_elapsed = to_fixp(max_elapsed).data();
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
    
    return std::max(fp_cast<fixp_t>((elap_t(FP(itr->value)) - elap_t(restored)) + elap_t(price)), fixp_t(0));
}
EOSIO_DISPATCH(charge, (use)(useandstore)(removestored)(setrestorer) )

} /// namespace golos

