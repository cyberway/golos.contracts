#include "golos.vesting.hpp"
#include "config.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/event.hpp>
#include <golos.charge/golos.charge.hpp>
#include <common/dispatchers.hpp>

namespace golos {


using namespace eosio;

struct vesting_params_setter: set_params_visitor<vesting_state> {
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
};

void vesting::validateprms(symbol symbol, std::vector<vesting_param> params) {
    vesting_params_singleton cfg(_self, symbol.code().raw());
    param_helper::check_params(params, cfg.exists());
}

void vesting::setparams(symbol symbol, std::vector<vesting_param> params) {
    eosio_assert(symbol.is_valid(), "not valid symbol");

    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    vesting_params_singleton cfg(_self, symbol.code().raw());
    param_helper::set_parameters<vesting_params_setter>(params, cfg, name(token::get_issuer(config::token_name, symbol.code())));
}

name get_recipient(const std::string& memo) {
    size_t memo_size = memo.size();
    const auto find_symbol = memo.find(';');
    if (find_symbol != std::string::npos && memo.size())
        memo_size = find_symbol;

    const size_t pref_size = config::send_prefix.size();
    if (memo_size < pref_size || memo.substr(0, pref_size) != config::send_prefix)
        return name();
    eosio_assert(memo_size > pref_size, "must provide recipient's name");
    return name(memo.substr(pref_size, memo_size - pref_size).c_str());
}

void vesting::on_transfer(name from, name to, asset quantity, std::string memo) {
    if (_self != to)
        return;

    auto recipient = get_recipient(memo);
    if (token::get_issuer(config::token_name, quantity.symbol.code()) == from && recipient == name())
        return;     // just increase token supply

    vesting_table table_vesting(_self, _self.value);    // TODO: use symbol as scope #550
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio_assert(vesting != table_vesting.end(), "Token not found");

    // balance already increased by quantity value at this point so use -quantity to correct it to get the right price
    asset converted = token_to_vesting(quantity, *vesting, -quantity.amount);
    table_vesting.modify(vesting, name(), [&](auto& item) {
        item.supply += converted;
        send_stat_event(item);
    });
    add_balance(recipient != name() ? recipient : from, converted, has_auth(to) ? to : from);
}

void vesting::do_transfer_vesting(name from, name to, asset quantity, std::string memo) {
    auto recipient = get_recipient(memo);
    if (token::get_issuer(config::token_name, quantity.symbol.code()) == from && recipient == name())
        return;     // just increase token supply

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
    eosio_assert(vesting != table_vesting.end(), "Token not found");

    asset sum_quantity_recipient(0, vesting->supply.symbol);
    for (auto recipient_obj : recipients) {
        if (_self != recipient_obj.to)
            continue;

        asset quantity_recipient = token_to_vesting(recipient_obj.quantity, *vesting, -sum_quantity.amount);
        sum_quantity_recipient += quantity_recipient;
        do_transfer_vesting(from, recipient_obj.to, quantity_recipient, recipient_obj.memo);
    }

    table_vesting.modify(vesting, name(), [&](auto& item) {
        item.supply += sum_quantity_recipient;
        send_stat_event(item);
    });

}

void vesting::retire(asset quantity, name user) {
    require_auth(name(token::get_issuer(config::token_name, quantity.symbol.code())));
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must retire positive quantity");

    vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio_assert(vesting != table_vesting.end(), "Vesting not found");
    eosio_assert(quantity.symbol == vesting->supply.symbol, "symbol precision mismatch");
    eosio_assert(quantity.amount <= vesting->supply.amount, "invalid amount");

    sub_balance(user, quantity, true);
    table_vesting.modify(vesting, name(), [&](auto& item) {
        item.supply -= quantity;
        send_stat_event(item);
    });
}

void vesting::withdraw(name from, name to, asset quantity) {
    eosio_assert(quantity.amount > 0, "quantity must be positive");
    require_auth(from);
    eosio_assert(is_account(to), "to account does not exist");

    const auto sym = quantity.symbol.code().raw();
    vesting_params_singleton cfg(_self, sym);
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto& withdraw_params = cfg.get().withdraw;
    const auto& min_amount = cfg.get().min_amount.min_amount;

    account_table account(_self, from.value);
    auto vest = account.find(sym);
    eosio_assert(vest != account.end(), "unknown asset");
    eosio_assert(vest->vesting.symbol == quantity.symbol, "wrong asset precision");
    eosio_assert(vest->available_vesting().amount >= quantity.amount, "Insufficient funds");
    eosio_assert(vest->vesting.amount >= min_amount, "Insufficient funds for converting");

    withdraw_table table(_self, sym);
    auto record = table.find(from.value);

    const auto intervals = withdraw_params.intervals;
    auto fill_record = [&](auto& item) {
        item.to = to;
        item.remaining_payments = intervals;
        item.interval_seconds = withdraw_params.interval_seconds;
        item.next_payout = time_point_sec(now() + withdraw_params.interval_seconds);
        item.withdraw_rate = quantity / intervals;
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
    eosio_assert(record != table.end(), "Not found convert record sender");
    table.erase(record);
}

void vesting::unlocklimit(name owner, asset quantity) {
    require_auth(owner);
    auto sym = quantity.symbol;
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount >= 0, "the number of tokens should not be less than 0");
    account_table accounts(_self, owner.value);
    const auto& b = accounts.get(sym.code().raw(), "no balance object found");
    eosio_assert(b.unlocked_limit.symbol == sym, "symbol precision mismatch");

    accounts.modify(b, name(), [&](auto& item) {
        item.unlocked_limit = quantity;
    });
}

void vesting::delegate(name from, name to, asset quantity, uint16_t interest_rate) {
    require_auth(from);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto& params = cfg.get();
    const auto& min_amount = params.min_amount.min_amount;
    const auto& delegation_params = params.delegation;
    const auto& withdraw_params = params.withdraw;

    eosio_assert(from != to, "You can not delegate to yourself");
    eosio_assert(quantity.amount > 0, "the number of tokens should not be less than 0");
    eosio_assert(quantity.amount >= min_amount, "Insufficient funds for delegation");
    eosio_assert(interest_rate <= delegation_params.max_interest, "Exceeded the percentage of delegated vesting");

    auto token_code = quantity.symbol.code();
    auto sname = token_code.raw();
    account_table account_sender(_self, from.value);
    auto balance_sender = account_sender.find(sname);
    eosio_assert(balance_sender != account_sender.end(), "Not found token");
    auto user_balance = balance_sender->vesting;

    withdraw_table convert_tbl(_self, sname);
    auto convert_obj = convert_tbl.find(from.value);
    if (convert_obj != convert_tbl.end()) {
        user_balance -= convert_obj->to_withdraw;
    }
    auto deleg_after = quantity + balance_sender->delegated;
    eosio_assert(user_balance >= deleg_after, "insufficient funds for delegation");
    int64_t deleg_prop = user_balance.amount ?
        (static_cast<int128_t>(deleg_after.amount) * config::_100percent) / user_balance.amount : 0;
    eosio_assert(charge::get_current_value(config::charge_name, from, token_code) <= config::_100percent - deleg_prop,
        "can't delegate, not enough power");

    account_sender.modify(balance_sender, from, [&](auto& item){
        item.delegated += quantity;
        send_account_event(from, item);
    });

    delegation_table table(_self, sname);
    auto index_table = table.get_index<"delegator"_n>();
    auto delegate_record = index_table.find({from, to});
    if (delegate_record != index_table.end()) {
        eosio_assert(delegate_record->interest_rate == interest_rate, "interest_rate does not match");
        index_table.modify(delegate_record, name(), [&](auto& item) {
            item.quantity += quantity;
        });
    } else {
        table.emplace(from, [&](auto& item){
            item.id = table.available_primary_key();
            item.delegator = from;
            item.delegatee = to;
            item.quantity = quantity;
            item.interest_rate = interest_rate;
            item.min_delegation_time = time_point_sec(now() + delegation_params.min_time);
        });
    }

    account_table account_recipient(_self, to.value);
    auto balance_recipient = account_recipient.find(sname);
    eosio_assert(balance_recipient != account_recipient.end(), "Not found balance token vesting");
    account_recipient.modify(balance_recipient, from, [&](auto& item) {
        item.received += quantity;
        send_account_event(to, item);
    });

    eosio_assert(balance_recipient->received.amount >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::undelegate(name from, name to, asset quantity) {
    require_auth(from);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto& delegation_params = cfg.get().delegation;

    delegation_table table(_self, quantity.symbol.code().raw());
    auto index_table = table.get_index<"delegator"_n>();
    auto delegate_record = index_table.find({from, to});
    eosio_assert(delegate_record != index_table.end(), "Not enough delegated vesting"); // wrong

    eosio_assert(quantity.amount >= delegation_params.min_amount, "Insufficient funds for undelegation");
    eosio_assert(delegate_record->min_delegation_time <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(delegate_record->quantity >= quantity, "There are not enough delegated tools for output");

    if (delegate_record->quantity == quantity) {
        index_table.erase(delegate_record);
    } else {
        index_table.modify(delegate_record, from, [&](auto& item) {
            item.quantity -= quantity;
        });
    }

    return_delegation_table table_delegate_vesting(_self, _self.value);
    table_delegate_vesting.emplace(from, [&](auto& item) {
        item.id = table_delegate_vesting.available_primary_key();
        item.delegator = from;
        item.quantity = quantity;
        item.date = time_point_sec(now() + delegation_params.return_time);
    });

    account_table account_recipient(_self, to.value);
    auto balance = account_recipient.find(quantity.symbol.code().raw());
    eosio_assert(balance != account_recipient.end(), "This token is not on the recipient balance sheet");
    account_recipient.modify(balance, from, [&](auto& item) {
        item.received -= quantity;
        send_account_event(to, item);
    });

    eosio_assert(balance->received.amount >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::create(symbol symbol, name notify_acc) {
    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(symbol.code().raw());
    eosio_assert(vesting == table_vesting.end(), "Vesting already exists");

    table_vesting.emplace(_self, [&](auto& item){
        item.supply = asset(0, symbol);
        item.notify_acc = notify_acc;
        send_stat_event(item);
    });
}

void vesting::timeoutconv() {
    require_auth(_self);
    const int max_steps = 16;           // TODO: configurable #707
    const auto memo = "withdraw";
    const auto now = time_point_sec(::now());
    vesting_table vestings(_self, _self.value);
    for (const auto& vesting : vestings) {
        int64_t correction = 0; // due inline actions delay, balance used in price calculations needs to be corrected #578
        withdraw_table table(_self, vesting.supply.symbol.code().raw());
        auto idx = table.get_index<"nextpayout"_n>();
        for (auto obj = idx.cbegin(); obj != idx.cend() && obj->next_payout <= now; ) {
            bool fail = obj->remaining_payments == 0 || obj->to_withdraw < obj->withdraw_rate;  // must not happen
            if (fail) {
                obj = idx.erase(obj);
                continue;
            }

            bool last_payment = obj->remaining_payments == 1;
            auto to_send = last_payment ? obj->to_withdraw : obj->withdraw_rate;
            if (!last_payment) {
                idx.modify(obj, same_payer, [&](auto& item) {
                    item.next_payout += item.interval_seconds;  // alternative: now+interval_seconds
                    item.to_withdraw -= item.withdraw_rate;
                    item.remaining_payments--;
                });
            }

            const name to = obj->to;
            const name from = obj->from;
            account_table account(_self, from.value);
            auto balance = account.find(to_send.symbol.code().raw());
            fail =
                balance == account.end() ||     // must not happen here, checked earlier (?archived)
                balance->vesting < to_send;     // must not happen
            if (last_payment || fail) {
                obj = idx.erase(obj);
                if (fail)
                    continue;
            } else {
                ++obj;
            }

            if (to_send.amount > 0) {
                // First convert and only then reduce supply (conversion rate changes after subtract)
                auto converted = vesting_to_token(to_send, vesting, -correction);   // TODO: get_balance can throw #549
                if (converted.amount == 0) {
                    obj = idx.erase(obj);   // amount is too low, it's impossible to withdraw
                    continue;
                }
                sub_balance(from, to_send);
                vestings.modify(vesting, name(), [&](auto& v) {
                    v.supply -= to_send;
                    send_stat_event(v);
                });
                // TODO: payment action #549
                INLINE_ACTION_SENDER(token, transfer)(config::token_name, {_self, config::code_name},
                    {_self, to, converted, memo});
                correction += converted.amount;    // accumulate
            } else {
                // possible only if minimal allowed withdraw is less than number of payments, skip
            }
            max_steps--;
            if (max_steps <= 0)
                return;
        }
    }
}

void vesting::timeoutrdel() {
    // TODO: this action must never throw because it will break returning of delegations on all accounts #549
    require_auth(_self);
    return_delegation_table table_delegate_vesting(_self, _self.value);
    auto index = table_delegate_vesting.get_index<"date"_n>();
    auto till = time_point_sec(now());
    for (auto obj = index.cbegin(); obj != index.cend() && obj->date <= till;) {
        account_table account_recipient(_self, obj->delegator.value);
        auto balance_recipient = account_recipient.find(obj->quantity.symbol.code().raw());
        eosio_assert(balance_recipient != account_recipient.end(), "This token is not on the sender balance sheet");
        account_recipient.modify(balance_recipient, name(), [&](auto &item){
            item.delegated -= obj->quantity;
            send_account_event(obj->delegator, item);
        });
        obj = index.erase(obj);
    }
}

void vesting::open(name owner, symbol symbol, name ram_payer) {
    require_auth(ram_payer);
    eosio_assert(is_account(owner), "owner account does not exist");
    vesting_table stat(_self, _self.value);
    auto token_stat = stat.require_find(symbol.code().raw(), "not found token vesting");
    eosio_assert(token_stat->supply.symbol.precision() == symbol.precision(), "mismatch of accuracy of vesting");

    account_table accounts(_self, owner.value);
    auto it = accounts.find(symbol.code().raw());
    eosio_assert(it == accounts.end(), "already exists");
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
    eosio_assert(it != account.end(), "Balance row already deleted or never existed. Action won't have any effect");
    eosio_assert(it->vesting.amount == 0, "Cannot close because the balance vesting is not zero");
    eosio_assert(it->delegated.amount == 0, "Cannot close because the balance delegate vesting is not zero");
    eosio_assert(it->received.amount == 0, "Cannot close because the balance received vesting not zero");
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
    eosio_assert(value.amount >= 0, "sub_balance: value.amount < 0");
    if (value.amount == 0)
        return;
    account_table account(_self, owner.value);
    const auto& from = account.get(value.symbol.code().raw(), "no balance object found");
    if (retire_mode)
        eosio_assert(from.unlocked_vesting() >= value, "overdrawn unlocked balance");
    else
        eosio_assert(from.available_vesting() >= value, "overdrawn balance");

    account.modify(from, name(), [&](auto& a) {
        a.vesting -= value;
        if (retire_mode)
            a.unlocked_limit -= value;
        send_account_event(owner, a);
    });
    notify_balance_change(owner, -value);
}

void vesting::add_balance(name owner, asset value, name ram_payer) {
    eosio_assert(value.amount >= 0, "add_balance: value.amount < 0");
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
        account.modify(to, name(), [&](auto& a) {
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

void vesting::timeout() {
    require_auth(_self);
    uint128_t sender_id = _self.value;
    transaction trx;
    trx.delay_sec = config::vesting_delay_tx_timeout;
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "timeout"_n, ""});
    trx.send(sender_id, _self);

    transaction trx2;
    trx2.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "timeoutrdel"_n, ""});
    trx2.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "timeoutconv"_n, ""});
    trx2.send(sender_id + 1, _self, true);
}

} // golos

DISPATCH_WITH_BULK_TRANSFER(golos::vesting, on_transfer, on_bulk_transfer, (validateprms)(setparams)
        (retire)(unlocklimit)(withdraw)(stopwithdraw)(delegate)(undelegate)(create)
        (open)(close)(timeout)(timeoutconv)(timeoutrdel))
