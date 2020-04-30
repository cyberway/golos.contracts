#include <golos.referral/golos.referral.hpp>
#include <golos.referral/config.hpp>
#include <common/dispatchers.hpp>
#include <eosio/transaction.hpp>

namespace golos {

using namespace eosio;

struct referral_params_setter: set_params_visitor<referral_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const breakout_parametrs& p) {
        return set_param(p, &referral_state::breakout_params, &breakout_parametrs::not_equal);
    }

    bool operator()(const expire_parametrs& p) {
        return set_param(p, &referral_state::expire_params);
    }

    bool operator()(const percent_parametrs& p) {
        return set_param(p, &referral_state::percent_params);
    }
};

void referral::validateprms(std::vector<referral_params> params) {
}

void referral::setparams(std::vector<referral_params> params) {
    require_auth(_self);

    referral_params_singleton cfg(_self, _self.value);
    param_helper::set_parameters<referral_params_setter>(params, cfg, _self);
}

void referral::add_referral(name referrer, name referral, uint16_t percent, uint64_t expire, asset breakout) {
    require_auth(_self);

    closeoldref();

    eosio::check(referrer != referral, "referral can not be referrer");
    referrals_table referrals(_self, _self.value);
    auto it_referral = referrals.find(referral.value);
    eosio::check(it_referral == referrals.end(), "A referral with the same name already exists");

    referral_params_singleton cfg(_self, _self.value);
    eosio::check(cfg.exists(), "not found parametrs");

    const auto now = eosio::current_time_point();
    const auto min_expire = now.sec_since_epoch();
    const auto max_expire = (now + eosio::seconds(cfg.get().expire_params.max_expire)).sec_since_epoch();
    eosio::check(expire   >  min_expire, "expire <= current block time");
    eosio::check(expire   <= max_expire, "expire > current block time + max_expire");
    eosio::check(breakout >= cfg.get().breakout_params.min_breakout, "breakout < min_breakout");
    eosio::check(breakout <= cfg.get().breakout_params.max_breakout, "breakout > max_breakout");
    eosio::check(percent  <= cfg.get().percent_params.max_percent, "specified parameter is greater than limit");

    referrals.emplace(_self, [&](auto &item) {
        item.referral = referral;
        item.referrer = referrer;
        item.percent  = percent;
        item.expire   = expire;
        item.breakout = breakout;
    });
}

void referral::on_transfer(name from, name to, asset quantity, std::string memo) {
    if (_self != to)
        return;

    closeoldref();

    referrals_table referrals(_self, _self.value);
    auto it_referral = referrals.find(from.value);
    eosio::check(it_referral != referrals.end(), "A referral with this name doesn't exist.");
    eosio::check(it_referral->breakout.amount == quantity.amount, "Amount of funds doesn't equal.");

    INLINE_ACTION_SENDER(token, transfer)
        (config::token_name, {_self, config::code_name},
        {_self, it_referral->referrer, quantity, ""});

    referrals.erase(it_referral);
}

void referral::closeoldref() {
    referral_params_singleton cfg(_self, _self.value);
    eosio::check(cfg.exists(), "referral: parametrs not found");

    referrals_table referrals(_self, _self.value);  // NOTE: can be cached to not recreate in calling actions
    const auto now = static_cast<uint64_t>(eosio::current_time_point().sec_since_epoch());
    auto idx = referrals.get_index<"expirekey"_n>();
    auto itr = idx.begin();
    int  i = config::max_deletions_per_trx;
    while (itr != idx.end() && i > 0 && itr->expire < now) {
        itr = idx.erase(itr);
        i--;
    }
}

}
