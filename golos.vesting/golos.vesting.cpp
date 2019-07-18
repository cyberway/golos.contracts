#include "golos.vesting.hpp"
#include "config.hpp"
#include <eosio/transaction.hpp>
#include <eosio/event.hpp>
#include <golos.charge/golos.charge.hpp>
#include <common/dispatchers.hpp>

namespace golos {


using namespace eosio;

struct vesting_params_setter: set_params_visitor<vesting_state> {
    std::optional<bwprovider> new_bwprovider;
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const vesting_withdraw_param& p) {
        return set_param(p, &vesting_state::withdraw);
    }

    bool operator()(const vesting_min_amount_param& p) {
        return set_param(p, &vesting_state::min_amount);
    }

    bool operator()(const delegation_param& p) {
        return set_param(p, &vesting_state::delegation);
    }

    bool operator()(const bwprovider_param& p) {
        new_bwprovider = p;
        return set_param(p, &vesting_state::bwprovider);
    }
};

void vesting::validateprms(symbol symbol, std::vector<vesting_param> params) {
    vesting_params_singleton cfg(_self, symbol.code().raw());
    param_helper::check_params(params, cfg.exists());
}

void vesting::setparams(symbol symbol, std::vector<vesting_param> params) {
    eosio::check(symbol.is_valid(), "not valid symbol");

    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    vesting_params_singleton cfg(_self, symbol.code().raw());
    auto setter = param_helper::set_parameters<vesting_params_setter>(params, cfg, name(token::get_issuer(config::token_name, symbol.code())));
    if (setter.new_bwprovider) {
        auto provider = setter.new_bwprovider->provider;
        if (provider.actor != name()) {
            dispatch_inline("cyber"_n, "providebw"_n, {provider}, std::make_tuple(provider.actor, _self));
        }
    }
}

name get_recipient(const std::string& memo) {
    size_t memo_size = memo.size();
    const auto find_symbol = memo.find(';');
    if (find_symbol != std::string::npos && memo.size())
        memo_size = find_symbol;

    const size_t pref_size = config::send_prefix.size();
    if (memo_size < pref_size || memo.substr(0, pref_size) != config::send_prefix)
        return name();
    eosio::check(memo_size > pref_size, "must provide recipient's name");   // TODO: test #744
    return name(memo.substr(pref_size, memo_size - pref_size).c_str());
}

void vesting::on_transfer(name from, name to, asset quantity, std::string memo) {
    if (_self != to)
        return;

    auto recipient = get_recipient(memo);
    if (token::get_issuer(config::token_name, quantity.symbol.code()) == from && recipient == name()) {
        procwaiting(quantity.symbol, _self);
        return;
    }

    vesting_table table_vesting(_self, _self.value);    // TODO: use symbol as scope #550
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio::check(vesting != table_vesting.end(), "Token not found");    // TODO: test #744

    // balance already increased by quantity value at this point so use -quantity to correct it to get the right price
    asset converted = token_to_vesting(quantity, *vesting, -quantity.amount);
    table_vesting.modify(vesting, same_payer, [&](auto& item) {
        item.supply += converted;
        send_stat_event(item);
    });
    add_balance(recipient != name() ? recipient : from, converted, has_auth(to) ? to : from);
}

void vesting::do_transfer_vesting(name from, name to, asset quantity, std::string memo) {
    auto recipient = get_recipient(memo);
    if (token::get_issuer(config::token_name, quantity.symbol.code()) == from && recipient == name()) {
        procwaiting(quantity.symbol, _self);
        return;
    }

    add_balance(recipient != name() ? recipient : from, quantity, has_auth(to) ? to : from);
}

void vesting::on_bulk_transfer(name from, std::vector<token::recipient> recipients) {
    auto token_symbol = recipients.at(0).quantity.symbol;

    asset sum_quantity(0, token_symbol);
    for (auto recipient_obj : recipients) {
        if (_self != recipient_obj.to)
            continue;

        sum_quantity += recipient_obj.quantity;
    }

    if (!sum_quantity.amount)
        return;

    vesting_table table_vesting(_self, _self.value);    // TODO: use symbol as scope #550
    auto vesting = table_vesting.find(token_symbol.code().raw());
    eosio::check(vesting != table_vesting.end(), "Token not found");    // TODO: test #744

    asset sum_quantity_recipient(0, vesting->supply.symbol);
    for (auto recipient_obj : recipients) {
        if (_self != recipient_obj.to)
            continue;

        asset quantity_recipient = token_to_vesting(recipient_obj.quantity, *vesting, -sum_quantity.amount);
        sum_quantity_recipient += quantity_recipient;
        do_transfer_vesting(from, recipient_obj.to, quantity_recipient, recipient_obj.memo);
    }

    table_vesting.modify(vesting, same_payer, [&](auto& item) {
        item.supply += sum_quantity_recipient;
        send_stat_event(item);
    });

}

void vesting::retire(asset quantity, name user) {
    require_auth(name(token::get_issuer(config::token_name, quantity.symbol.code())));
    eosio::check(quantity.is_valid(), "invalid quantity");  // TODO: test #744
    eosio::check(quantity.amount > 0, "must retire positive quantity");

    vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio::check(vesting != table_vesting.end(), "Vesting not found");
    eosio::check(quantity.symbol == vesting->supply.symbol, "symbol precision mismatch");
    eosio::check(quantity.amount <= vesting->supply.amount, "invalid amount");

    sub_balance(user, quantity, true);
    table_vesting.modify(vesting, same_payer, [&](auto& item) {
        item.supply -= quantity;
        send_stat_event(item);
    });
}

void vesting::withdraw(name from, name to, asset quantity) {
    eosio::check(quantity.amount > 0, "quantity must be positive");
    require_auth(from);
    eosio::check(is_account(to), "to account does not exist");  // maybe it's enough to check balance existance?

    const auto sym = quantity.symbol.code().raw();
    vesting_params_singleton cfg(_self, sym);
    eosio::check(cfg.exists(), "not found vesting params");
    const auto& params = cfg.get();
    const auto intervals = params.withdraw.intervals;
    const auto interval_sec = params.withdraw.interval_seconds;
    const auto rate = quantity / intervals;
    eosio::check(rate.amount > 0, "withdraw rate is too low");

    account_table account(_self, from.value);
    auto vest = account.find(sym);
    eosio::check(vest != account.end(), "unknown asset");
    eosio::check(vest->vesting.symbol == quantity.symbol, "wrong asset precision");
    eosio::check(vest->available_vesting().amount >= quantity.amount, "Insufficient funds");    // TODO: test #744
    eosio::check(vest->vesting.amount >= params.min_amount.min_amount, "Insufficient funds for converting"); // TODO: test #744

    eosio::check(token::balance_exist(config::token_name, to, quantity.symbol.code()),
        "to account have not opened balance");

    withdraw_table table(_self, sym);
    auto record = table.find(from.value);

    auto fill_record = [&](auto& item) {
        item.to = to;
        item.remaining_payments = intervals;
        item.interval_seconds = interval_sec;
        item.next_payout = eosio::current_time_point() + eosio::seconds(interval_sec);
        item.withdraw_rate = rate;
        item.to_withdraw = quantity;
    };

    if (record != table.end()) {
        table.modify(record, from, fill_record);
    } else {
        table.emplace(from, [&](auto& item) {
            item.from = from;
            fill_record(item);
        });
    }
}

void vesting::stopwithdraw(name owner, symbol sym) {
    require_auth(owner);

    withdraw_table table(_self, sym.code().raw());
    auto record = table.find(owner.value);
    eosio::check(record != table.end(), "Not found convert record sender"); // TODO: test #744
    table.erase(record);
}

void vesting::unlocklimit(name owner, asset quantity) {
    require_auth(owner);
    auto sym = quantity.symbol;
    eosio::check(quantity.is_valid(), "invalid quantity");  // TODO: test #744
    eosio::check(quantity.amount >= 0, "the number of tokens should not be less than 0");
    account_table accounts(_self, owner.value);
    const auto& b = accounts.get(sym.code().raw(), "no balance object found");
    eosio::check(b.unlocked_limit.symbol == sym, "symbol precision mismatch");  // TODO: test #744

    accounts.modify(b, same_payer, [&](auto& item) {
        item.unlocked_limit = quantity;
    });
}

void vesting::delegate(name from, name to, asset quantity, uint16_t interest_rate) {
    require_auth(from);
    require_auth(to);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio::check(cfg.exists(), "not found vesting params");
    const auto& params = cfg.get();
    const auto& min_amount = params.min_amount.min_amount;
    const auto& delegation_params = params.delegation;
    const auto& withdraw_params = params.withdraw;

    eosio::check(from != to, "You can not delegate to yourself");
    eosio::check(quantity.amount > 0, "the number of tokens should not be less than 0");
    eosio::check(quantity.amount >= min_amount, "Insufficient funds for delegation");
    eosio::check(interest_rate <= config::_100percent, "interest_rate cannot be greater than 100% (10000)");
    auto token_code = quantity.symbol.code();
    auto sname = token_code.raw();
    account_table account_sender(_self, from.value);
    auto balance_sender = account_sender.find(sname);
    eosio::check(balance_sender != account_sender.end(), "Not found token");    // TODO: test #744
    auto avail_balance = balance_sender->vesting - get_withdrawing_vesting(_self, from, quantity.symbol);

    auto deleg_after = quantity + balance_sender->delegated;
    eosio::check(avail_balance >= deleg_after, "Insufficient funds for delegation");
    // avail_balance is guaranteed to be > 0 at this point
    int64_t deleg_prop = static_cast<int128_t>(deleg_after.amount) * config::_100percent / avail_balance.amount;
    eosio::check(charge::get_current_value(config::charge_name, from, token_code) <= config::_100percent - deleg_prop,
        "can't delegate, not enough power");

    account_sender.modify(balance_sender, from, [&](auto& item){
        item.delegated += quantity;
        send_account_event(from, item);
    });

    delegation_table table(_self, sname);
    auto index_table = table.get_index<"delegator"_n>();
    auto delegate_record = index_table.find({from, to});
    auto exists = delegate_record != index_table.end();
    if (exists) {
        index_table.modify(delegate_record, same_payer, [&](auto& item) {
            item.quantity += quantity;
        });
    } else {
        table.emplace(from, [&](auto& item){
            item.id = table.available_primary_key();
            item.delegator = from;
            item.delegatee = to;
            item.quantity = quantity;
            item.interest_rate = interest_rate;
            item.min_delegation_time = eosio::current_time_point() + eosio::seconds(delegation_params.min_time);
        });
    }

    account_table account_recipient(_self, to.value);
    auto balance_recipient = account_recipient.find(sname);
    eosio::check(balance_recipient != account_recipient.end(), "Not found balance token vesting");  // TODO: test #744
    eosio::check(balance_recipient->delegators < delegation_params.max_delegators, "Recipient reached maximum of delegators");
    account_recipient.modify(balance_recipient, from, [&](auto& item) {
        item.received += quantity;
        if (!exists) {
            item.delegators++;
        }
        send_account_event(to, item);
    });

    eosio::check(balance_recipient->received.amount >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::undelegate(name from, name to, asset quantity) {
    bool delegator_signed = has_auth(from);
    eosio::check(delegator_signed || has_auth(to), "missing required authority");

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio::check(cfg.exists(), "not found vesting params");
    const auto& delegation_params = cfg.get().delegation;

    delegation_table table(_self, quantity.symbol.code().raw());
    auto index_table = table.get_index<"delegator"_n>();
    auto delegate_record = index_table.find({from, to});
    eosio::check(delegate_record != index_table.end(), "Not enough delegated vesting"); // TODO: #551, #744

    eosio::check(quantity.amount >= delegation_params.min_amount, "Insufficient funds for undelegation");
    eosio::check(delegate_record->min_delegation_time <= eosio::current_time_point(),
        "Tokens are frozen until the end of the period");
    eosio::check(delegate_record->quantity >= quantity, "There are not enough delegated tools for output");

    auto erasing = delegate_record->quantity == quantity;
    if (erasing) {
        index_table.erase(delegate_record);
    } else {
        index_table.modify(delegate_record, same_payer, [&](auto& item) {
            item.quantity -= quantity;
        });
    }

    return_delegation_table table_delegate_vesting(_self, _self.value);
    table_delegate_vesting.emplace(delegator_signed ? from : to, [&](auto& item) {
        item.id = table_delegate_vesting.available_primary_key();
        item.delegator = from;
        item.quantity = quantity;
        item.date = eosio::current_time_point() + eosio::seconds(delegation_params.return_time);
    });

    account_table account_recipient(_self, to.value);
    auto balance = account_recipient.find(quantity.symbol.code().raw());
    eosio::check(balance != account_recipient.end(), "This token is not on the recipient balance sheet");   // TODO: test #744
    account_recipient.modify(balance, same_payer, [&](auto& item) {
        item.received -= quantity;
        if (erasing) {
            item.delegators--;
        }
        send_account_event(to, item);
    });

    auto remaining = balance->received.amount;
    eosio::check(!remaining || remaining >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::create(symbol symbol, name notify_acc) {
    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(symbol.code().raw());
    eosio::check(vesting == table_vesting.end(), "Vesting already exists"); // TODO: test #744

    table_vesting.emplace(_self, [&](auto& item){
        item.supply = asset(0, symbol);
        item.notify_acc = notify_acc;
        send_stat_event(item);
    });
}

bool vesting::process_withdraws(eosio::time_point now, symbol symbol, name payer) {
    int max_steps = 16;           // TODO: configurable #707
    const auto memo = "withdraw";
    vesting_table vestings(_self, _self.value);
    for (const auto& vesting : vestings) {
        int64_t correction = 0; // due inline actions delay, balance used in price calculations needs to be corrected #578
        withdraw_table table(_self, vesting.supply.symbol.code().raw());
        auto idx = table.get_index<"nextpayout"_n>();
        bool fail = false, last_payment = false;
        for (
            auto obj = idx.cbegin();
            obj != idx.cend() && obj->next_payout <= now;
            fail || last_payment ? obj = idx.erase(obj) : ++obj
        ) {
            if (max_steps-- <= 0) {
            	return true;
            }

            fail = obj->remaining_payments == 0 || obj->to_withdraw < obj->withdraw_rate;  // must not happen
            if (fail) continue;
            last_payment = obj->remaining_payments == 1;
            auto to_send = last_payment ? obj->to_withdraw : obj->withdraw_rate;
            const name to = obj->to;
            const name from = obj->from;
            account_table accounts(_self, from.value);
            auto balance = accounts.find(to_send.symbol.code().raw());
            fail =
                balance == accounts.end() ||    // must not happen here, checked earlier (?archived)
                balance->vesting < to_send ||   // must not happen
                to_send.amount <= 0;            // must not happen
            if (fail) continue;

            // First convert and only then reduce supply (conversion rate changes after subtract)
            auto converted = vesting_to_token(to_send, vesting, -correction);   // get_balance can throw (only on broken system, preserve)
            fail = converted.amount == 0;       // amount is too low, it's impossible to withdraw
            if (fail) continue;

            fail = !token::balance_exist(config::token_name, to, converted.symbol.code());
            if (fail) continue;

            if (!last_payment) {
                idx.modify(obj, same_payer, [&](auto& item) {
                    item.next_payout += item.interval_seconds;  // alternative: now+interval_seconds
                    item.to_withdraw -= item.withdraw_rate;
                    item.remaining_payments--;
                });
            }
            sub_balance(from, to_send);
            vestings.modify(vesting, same_payer, [&](auto& v) {
                v.supply -= to_send;
                send_stat_event(v);
            });
            INLINE_ACTION_SENDER(token, payment)(config::token_name, {_self, config::code_name},
                {_self, to, converted, memo});
            correction += converted.amount;    // accumulate
        }
    }
    return false;
}

bool vesting::return_delegations(eosio::time_point till, symbol symbol, name payer) {
    int max_steps = 16;
    return_delegation_table tbl(_self, _self.value);
    auto idx = tbl.get_index<"date"_n>();
    for (auto obj = idx.cbegin(); obj != idx.cend() && obj->date <= till;) {
        if (max_steps-- <= 0) {
            return true;
        }

        account_table delegator_balances(_self, obj->delegator.value);
        auto balance = delegator_balances.find(obj->quantity.symbol.code().raw());
        // The following checks can only fail on broken system, preserve them to prevent break more (can't resolve automatically)
        eosio::check(balance != delegator_balances.end(), "returndeleg: Vesting balance not found"); // impossible
        eosio::check(balance->delegated >= obj->quantity, "returndeleg: returning > delegated");     // impossible
        delegator_balances.modify(balance, same_payer, [&](auto& item){
            item.delegated -= obj->quantity;
            send_account_event(obj->delegator, item);
        });
        obj = idx.erase(obj);
    }
    return false;
}

void vesting::procwaiting(symbol symbol, name payer) {
    auto now = eosio::current_time_point();
    bool remain_withdraws = process_withdraws(now, symbol, payer);
    bool remain_returns = return_delegations(now, symbol, payer);
    if (remain_withdraws || remain_returns) {
        vesting_params_singleton cfg(_self, symbol.code().raw());
        transaction trx(now + eosio::seconds(3*60*60));
        trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "procwaiting"_n, std::make_tuple(symbol, _self)});
        providebw_for_trx(trx, cfg.get().bwprovider.provider);
        trx.delay_sec = 120;
        trx.send(static_cast<uint128_t>(now.time_since_epoch().count()) << 64, payer, true);
    }
}

void vesting::open(name owner, symbol symbol, name ram_payer) {
    require_auth(ram_payer);
    eosio::check(is_account(owner), "owner account does not exist");
    vesting_table stat(_self, _self.value);
    auto token_stat = stat.require_find(symbol.code().raw(), "not found token vesting");
    eosio::check(token_stat->supply.symbol.precision() == symbol.precision(), "mismatch of accuracy of vesting");

    account_table accounts(_self, owner.value);
    auto it = accounts.find(symbol.code().raw());
    eosio::check(it == accounts.end(), "already exists");   // TODO: test #744
    accounts.emplace(ram_payer, [&](auto& a) {
        a.vesting.symbol = symbol;
        a.delegated.symbol = symbol;
        a.received.symbol = symbol;
        a.unlocked_limit.symbol = symbol;
    });
}

void vesting::close(name owner, symbol symbol) {
    require_auth(owner);
    account_table account(_self, owner.value);
    auto it = account.find(symbol.code().raw());
    eosio::check(it != account.end(), "Balance row already deleted or never existed. Action won't have any effect");    // TODO: test #744
    eosio::check(it->vesting.amount == 0, "Cannot close because the balance vesting is not zero");  // TODO: test #744
    eosio::check(it->delegated.amount == 0, "Cannot close because the balance delegate vesting is not zero");   // TODO: test #744
    eosio::check(it->received.amount == 0, "Cannot close because the balance received vesting not zero");   // TODO: test #744
    account.erase(it);
}

void vesting::notify_balance_change(name owner, asset diff) {
    vesting_table table_vesting(_self, _self.value);
    auto notify = table_vesting.find(diff.symbol.code().raw());
    action(
        permission_level{token::get_issuer(config::token_name, diff.symbol.code()), config::changevest_name},
        notify->notify_acc,
        "changevest"_n,
        std::make_tuple(owner, diff)
    ).send();
}

void vesting::sub_balance(name owner, asset value, bool retire_mode) {
    eosio::check(value.amount >= 0, "sub_balance: value.amount < 0");
    if (value.amount == 0)
        return;
    account_table account(_self, owner.value);
    const auto& from = account.get(value.symbol.code().raw(), "no balance object found");
    if (retire_mode)
        eosio::check(can_retire_vesting(_self, owner, value), "overdrawn unlocked balance"); // TODO: remove additional db.get #554
    else
        eosio::check(from.available_vesting() >= value, "overdrawn balance");   // TODO: test #744

    account.modify(from, same_payer, [&](auto& a) {
        a.vesting -= value;
        if (retire_mode)
            a.unlocked_limit -= value;
        send_account_event(owner, a);
    });
    notify_balance_change(owner, -value);
}

void vesting::add_balance(name owner, asset value, name ram_payer) {
    eosio::check(value.amount >= 0, "add_balance: value.amount < 0");
    if (value.amount == 0)
        return;
    account_table account(_self, owner.value);
    auto to = account.find(value.symbol.code().raw());
    if (to == account.end()) {
        account.emplace(ram_payer, [&](auto& a) {
            a.vesting = value;
            a.delegated.symbol = value.symbol;
            a.received.symbol = value.symbol;
            send_account_event(owner, a);
        });
    } else {
        account.modify(to, same_payer, [&](auto& a) {
            a.vesting += value;
            send_account_event(owner, a);
        });
    }
    notify_balance_change(owner, value);
}

void vesting::send_account_event(name account, const struct account& balance) {
    eosio::event(_self, "balance"_n, std::make_tuple(account, balance)).send();
}

void vesting::send_stat_event(const vesting_stats& info) {
    eosio::event(_self, "stat"_n, info.supply).send();
}

int64_t fix_precision(const asset from, const symbol to) {
    int a = from.symbol.precision();
    int b = to.precision();
    if (a == b) {
        return from.amount;
    }
    auto e10 = [](int x) {
        int r = 1;
        while (x-- > 0)
            r *= 10;
        return r;
    };
    int min = std::min(a, b);
    auto div = e10(a - min);
    auto mult = e10(b - min);
    return static_cast<int128_t>(from.amount) * mult / div;
}

const asset vesting::vesting_to_token(const asset& src, const vesting_stats& vinfo, int64_t correction) const {
    auto token_supply = token::get_balance(config::token_name, _self, src.symbol.code());
    if (correction) {   // unlikely
        token_supply.set_amount(token_supply.amount + correction);
    }
    int64_t amount = vinfo.supply.amount && token_supply.amount
        ? static_cast<int64_t>((static_cast<int128_t>(src.amount) * token_supply.amount) / vinfo.supply.amount)
        : fix_precision(src, token_supply.symbol);
    return asset(amount, token_supply.symbol);
}

const asset vesting::token_to_vesting(const asset& src, const vesting_stats& vinfo, int64_t correction) const {
    auto balance = token::get_balance(config::token_name, _self, src.symbol.code());
    balance.set_amount(balance.amount + correction);
    int64_t amount = vinfo.supply.amount && balance.amount
        ? static_cast<int64_t>((static_cast<int128_t>(src.amount) * vinfo.supply.amount) / balance.amount)
        : fix_precision(src, vinfo.supply.symbol);
    return asset(amount, vinfo.supply.symbol);
}

void vesting::providebw_for_trx(transaction& trx, const permission_level& provider) {
    if (provider.actor != name()) {
        trx.actions.emplace_back(action{provider, "cyber"_n, "providebw"_n, std::make_tuple(provider.actor, _self)});
    }
}

} // golos

DISPATCH_WITH_BULK_TRANSFER(golos::vesting, on_transfer, on_bulk_transfer, (validateprms)(setparams)
        (retire)(unlocklimit)(withdraw)(stopwithdraw)(delegate)(undelegate)(create)
        (open)(close)(procwaiting))
