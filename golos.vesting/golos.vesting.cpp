#include "golos.vesting.hpp"
#include "config.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/event.hpp>
#include <cyber.token/cyber.token.hpp>
#include <golos.charge/golos.charge.hpp>

namespace golos {


using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        // vesting(receiver).apply(code, action);
    auto execute_action = [&](const auto fn) {
        return eosio::execute_action(eosio::name(receiver), eosio::name(code), fn);
    };
#define NN(x) N(x).value

    if (NN(transfer) == action && config::token_name.value == code)
        execute_action(&vesting::on_transfer);

    else if (NN(retire) == action)
        execute_action(&vesting::retire);
    else if (NN(unlocklimit) == action)
        execute_action(&vesting::unlock_limit);
    else if (NN(convertvg) == action)
        execute_action(&vesting::convert_vesting);
    else if (NN(cancelvg) == action)
        execute_action(&vesting::cancel_convert_vesting);
    else if (NN(delegatevg) == action)
        execute_action(&vesting::delegate_vesting);
    else if (NN(undelegatevg) == action)
        execute_action(&vesting::undelegate_vesting);

    else if (NN(open) == action)
        execute_action(&vesting::open);
    else if (NN(close) == action)
        execute_action(&vesting::close);

    else if (NN(createvest) == action)
        execute_action(&vesting::create);

    else if (NN(timeoutrdel) == action)
        execute_action(&vesting::calculate_delegate_vesting);
    else if (NN(timeoutconv) == action)
        execute_action(&vesting::calculate_convert_vesting);
    else if (NN(timeout) == action)
        execute_action(&vesting::timeout_delay_trx);

    else if (NN(validateprms) == action)
        execute_action(&vesting::validateprms);
    else if (NN(setparams) == action)
        execute_action(&vesting::setparams);
    else if (NN(paydelegator) == action)
        execute_action(&vesting::paydelegator);
    }
#undef NN
}

struct vesting_params_setter: set_params_visitor<vesting_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const vesting_withdraw_param& p) {
        return set_param(p, &vesting_state::vesting_withdraw_params);
    }

    bool operator()(const vesting_min_amount_param& p) {
        return set_param(p, &vesting_state::amount_params);
    }

    bool operator()(const delegation_param& p) {
        return set_param(p, &vesting_state::delegation_params);
    }
};

void vesting::validateprms(symbol symbol, std::vector<vesting_params> params) {
    vesting_params_singleton cfg(_self, symbol.code().raw());
    param_helper::check_params(params, cfg.exists());
}

void vesting::setparams(symbol symbol, std::vector<vesting_params> params) {
    eosio_assert(symbol.is_valid(), "not valid symbol");

    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    vesting_params_singleton cfg(_self, symbol.code().raw());
    param_helper::set_parameters<vesting_params_setter>(params, cfg, name(token::get_issuer(config::token_name, symbol.code())));
}

void vesting::on_transfer(name from, name to, asset quantity, std::string memo) {
    if (_self != to)
        return;
        
    auto recipient = get_recipient(memo);
    if(token::get_issuer(config::token_name, quantity.symbol.code()) == from && recipient == name())
        return;     // just increase token supply

    tables::vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio_assert(vesting != table_vesting.end(), "Token not found");

    asset converted = convert_to_vesting(quantity, *vesting);
    table_vesting.modify(vesting, name(), [&](auto& item) {
        item.supply += converted;
        // TODO Add notify about supply changes
    });
    
    add_balance(recipient != name() ? recipient : from, converted, has_auth(to) ? to : from);
}

name vesting::get_recipient(const std::string& memo) {
    const size_t pref_size = config::send_prefix.size();
    const size_t memo_size = memo.size();
    if (memo_size < pref_size || memo.substr(0, pref_size) != config::send_prefix)
        return name();
    eosio_assert(memo_size > pref_size, "must provide recipient's name");
    return name(memo.substr(pref_size).c_str());
}

void vesting::retire(asset quantity, name user) {
    require_auth(name(token::get_issuer(config::token_name, quantity.symbol.code())));
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must retire positive quantity");

    tables::vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(quantity.symbol.code().raw());
    eosio_assert(vesting != table_vesting.end(), "Vesting not found");
    eosio_assert(quantity.symbol == vesting->supply.symbol, "symbol precision mismatch");
    eosio_assert(quantity.amount <= vesting->supply.amount, "invalid amount");

    sub_balance(user, quantity, true);
    table_vesting.modify(vesting, name(), [&](auto& item) {
        item.supply -= quantity;
        // TODO Add notify about supply changes
    });
}

void vesting::convert_vesting(name from, name to, asset quantity) {
    require_auth(from);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto &withdraw_params = cfg.get().vesting_withdraw_params;
    const auto &amount_params = cfg.get().amount_params;

    tables::account_table account(_self, from.value);
    auto vest = account.find(quantity.symbol.code().raw());
    eosio_assert(vest != account.end(), "unknown asset");
    eosio_assert(vest->vesting.symbol == quantity.symbol, "wrong asset precision");
    eosio_assert(vest->available_vesting().amount >= quantity.amount, "Insufficient funds");
    eosio_assert(amount_params.min_amount <= vest->vesting.amount, "Insufficient funds for converting");
    eosio_assert(quantity.amount > 0, "quantity must be positive");

    tables::convert_table table(_self, quantity.symbol.code().raw());
    auto record = table.find(from.value);

    const auto intervals = withdraw_params.intervals;
    auto fill_record = [&](auto& item) {
        item.recipient = to;
        item.number_of_payments = intervals;
        item.payout_time = time_point_sec(now() + withdraw_params.interval_seconds);
        item.payout_part = quantity / intervals;
        item.balance_amount = quantity;
        // print("payout_time: ", now() + withdraw_params.interval_seconds, " now:", now(), "\n");
    };

    if (record != table.end()) {
        table.modify(record, from, fill_record);
    } else {
        table.emplace(from, [&](auto& item) {
            item.sender = from;
            fill_record(item);
        });
    }
}

// TODO: pass symbol name instead of asset
void vesting::cancel_convert_vesting(name sender, asset type) {
    require_auth(sender);

    tables::convert_table table(_self, type.symbol.code().raw());
    auto record = table.find(sender.value);
    eosio_assert(record != table.end(), "Not found convert record sender");

    table.erase(record);
}

void vesting::unlock_limit(name owner, asset quantity) {
    require_auth(owner);
    auto sym = quantity.symbol;
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount >= 0, "the number of tokens should not be less than 0");
    tables::account_table accounts(_self, owner.value);
    const auto& b = accounts.get(sym.code().raw(), "no balance object found");
    eosio_assert(b.unlocked_limit.symbol == sym, "symbol precision mismatch");

    accounts.modify(b, name(), [&](auto& item) {
        item.unlocked_limit = quantity;
    });
}

void vesting::delegate_vesting(name sender, name recipient, asset quantity, uint16_t interest_rate, uint8_t payout_strategy) {
    require_auth(sender);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto &amount_params = cfg.get().amount_params;
    const auto &delegation_params = cfg.get().delegation_params;
    const auto &withdraw_params = cfg.get().vesting_withdraw_params;

    eosio_assert(sender != recipient, "You can not delegate to yourself");
    eosio_assert(payout_strategy >= 0 && payout_strategy < 2, "not valid value payout_strategy");
    eosio_assert(quantity.amount > 0, "the number of tokens should not be less than 0");
    eosio_assert(quantity.amount >= amount_params.min_amount, "Insufficient funds for delegation");
    eosio_assert(interest_rate <= delegation_params.max_interest, "Exceeded the percentage of delegated vesting");
    
    auto token_code = quantity.symbol.code();
    auto sname = token_code.raw();
    tables::account_table account_sender(_self, sender.value);
    auto balance_sender = account_sender.find(sname);
    eosio_assert(balance_sender != account_sender.end(), "Not found token");
    auto user_balance = balance_sender->vesting;
    
    tables::convert_table convert_tbl(_self, sname);
    auto convert_obj = convert_tbl.find(sender.value);
    if (convert_obj != convert_tbl.end()) {
        auto remains_int = convert_obj->payout_part * convert_obj->number_of_payments;
        auto remains_fract = convert_obj->balance_amount - convert_obj->payout_part * withdraw_params.intervals;
        user_balance -= (remains_int + remains_fract);
    }
    auto deleg_after = quantity + balance_sender->delegate_vesting;
    eosio_assert(user_balance >= deleg_after, "insufficient funds for delegation");
    int64_t deleg_prop = user_balance.amount ? 
        (static_cast<int128_t>(deleg_after.amount) * config::_100percent) / user_balance.amount : 0;
    eosio_assert(charge::get_current_value(config::charge_name, sender, token_code) <= config::_100percent - deleg_prop,
        "can't delegate, not enough power");

    account_sender.modify(balance_sender, sender, [&](auto& item){
        item.delegate_vesting += quantity;
        // TODO Add notify about vesting changed
    });

    tables::delegate_table table(_self, sname);
    auto index_table = table.get_index<"unique"_n>();
    auto delegate_record = index_table.find(structures::delegate_record::unique_key(sender, recipient));
    if (delegate_record != index_table.end()) {

        eosio_assert(delegate_record->interest_rate == interest_rate, "interest_rate does not match");
        eosio_assert(delegate_record->payout_strategy == payout_strategy, "payout_strategy does not match");

        index_table.modify(delegate_record, name(), [&](auto& item){
            item.quantity += quantity;
        });
    } else {
        table.emplace(sender, [&](auto& item){
            item.id = table.available_primary_key();
            item.sender = sender;
            item.recipient = recipient;
            item.quantity = quantity;
            item.interest_rate = interest_rate;
            item.payout_strategy = payout_strategy; // TODO 0 - to_delegator, 1 - to_delegated_vestings
            item.return_date = time_point_sec(now() + delegation_params.min_time);
        });
    }

    tables::account_table account_recipient(_self, recipient.value);
    auto balance_recipient = account_recipient.find(sname);
    eosio_assert(balance_recipient != account_recipient.end(), "Not found balance token vesting");
    account_recipient.modify(balance_recipient, sender, [&](auto& item){
        item.received_vesting += quantity;
        // TODO Add notify about vesting changed
    });

    eosio_assert(balance_recipient->received_vesting.amount >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::undelegate_vesting(name sender, name recipient, asset quantity) {
    require_auth(sender);

    vesting_params_singleton cfg(_self, quantity.symbol.code().raw());
    eosio_assert(cfg.exists(), "not found vesting params");
    const auto &delegation_params = cfg.get().delegation_params;

    tables::delegate_table table(_self, quantity.symbol.code().raw());
    auto index_table = table.get_index<"unique"_n>();
    auto delegate_record = index_table.find(structures::delegate_record::unique_key(sender, recipient));
    eosio_assert(delegate_record != index_table.end(), "Not enough delegated vesting");

    eosio_assert(quantity.amount >= delegation_params.min_amount, "Insufficient funds for undelegation");
    eosio_assert(delegate_record->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(delegate_record->quantity >= quantity, "There are not enough delegated tools for output");

    if (delegate_record->quantity == quantity) {
        index_table.erase(delegate_record);
    } else {
        index_table.modify(delegate_record, sender, [&](auto& item){
            item.quantity -= quantity;
        });
    }

    tables::return_delegate_table table_delegate_vesting(_self, _self.value);
    table_delegate_vesting.emplace(sender, [&](auto& item){
        item.id = table_delegate_vesting.available_primary_key();
        item.recipient = sender;
        item.amount = quantity;
        item.date = time_point_sec(now() + delegation_params.return_time);
    });

    tables::account_table account_recipient(_self, recipient.value);
    auto balance = account_recipient.find(quantity.symbol.code().raw());
    eosio_assert(balance != account_recipient.end(), "This token is not on the recipient balance sheet");
    account_recipient.modify(balance, sender, [&](auto& item){
        item.received_vesting -= quantity;
        // TODO Add notify about vesting changed
    });

    eosio_assert(balance->received_vesting.amount >= delegation_params.min_remainder, "delegated vesting withdrawn");
}

void vesting::create(symbol symbol, name notify_acc) {
    require_auth(name(token::get_issuer(config::token_name, symbol.code())));

    tables::vesting_table table_vesting(_self, _self.value);
    auto vesting = table_vesting.find(symbol.code().raw());
    eosio_assert(vesting == table_vesting.end(), "Vesting already exists");

    table_vesting.emplace(_self, [&](auto& item){
        item.supply = asset(0, symbol);
        item.notify_acc = notify_acc;
        // TODO Add notify about supply changes
    });
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);
    tables::vesting_table table_vesting(_self, _self.value);

    for (auto vesting : table_vesting) {
        vesting_params_singleton cfg(_self, vesting.supply.symbol.code().raw());
        eosio_assert(cfg.exists(), "not found vesting params");
        const auto &withdraw_params = cfg.get().vesting_withdraw_params;

        tables::convert_table table(_self, vesting.supply.symbol.code().raw());
        auto index = table.get_index<"payouttime"_n>();
        for (auto obj = index.cbegin(); obj != index.cend(); ) {
            if (obj->payout_time > time_point_sec(now()))
                break;

            if (obj->number_of_payments > 0) {
                index.modify(obj, name(), [&](auto& item) {
                    item.payout_time = time_point_sec(now() + withdraw_params.interval_seconds);
                    --item.number_of_payments;

                    tables::account_table account(_self, obj->sender.value);
                    auto balance = account.find(obj->payout_part.symbol.code().raw());
                    eosio_assert(balance != account.end(), "Vesting balance not found");    // must not happen here, checked earlier

                    auto quantity = balance->vesting;
                    if (balance->vesting < obj->payout_part) {
                        item.number_of_payments = 0;
                    } else if (!obj->number_of_payments) { // TODO obj->number_of_payments == 0
                        quantity = obj->balance_amount - (obj->payout_part * (withdraw_params.intervals - 1));
                    } else {
                        quantity = obj->payout_part;
                    }

                    sub_balance(obj->sender, quantity);
                    auto vest = table_vesting.find(quantity.symbol.code().raw());
                    eosio_assert(vest != table_vesting.end(), "Vesting not found"); // must not happen at this point
                    table_vesting.modify(vest, name(), [&](auto& item) {
                        item.supply -= quantity;
                        // TODO Add notify about supply change
                    });
                    INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {_self, config::active_name},
                        {_self, obj->recipient, convert_to_token(quantity, *vest), "Convert vesting"});
                });

                ++obj;
            } else {
                obj = index.erase(obj);
            }

        }
    }
}

void vesting::calculate_delegate_vesting() {
    require_auth(_self);
    tables::return_delegate_table table_delegate_vesting(_self, _self.value);
    auto index = table_delegate_vesting.get_index<"date"_n>();
    for (auto obj = index.cbegin(); obj != index.cend();) {
        if (obj->date > time_point_sec(now()))
            break;

        tables::account_table account_recipient(_self, obj->recipient.value);
        auto balance_recipient = account_recipient.find(obj->amount.symbol.code().raw());
        eosio_assert(balance_recipient != account_recipient.end(), "This token is not on the sender balance sheet");
        account_recipient.modify(balance_recipient, name(), [&](auto &item){
            item.delegate_vesting -= obj->amount;
            // TODO Add event about vesting changed
        });

        obj = index.erase(obj);
    }
}

void vesting::open(name owner, symbol symbol, name ram_payer) {
    require_auth(ram_payer);
    tables::account_table accounts(_self, owner.value);
    auto it = accounts.find(symbol.code().raw());
    eosio_assert(it == accounts.end(), "already exists");
    accounts.emplace(ram_payer, [&](auto& a) {
        a.vesting.symbol = symbol;
        a.delegate_vesting.symbol = symbol;
        a.received_vesting.symbol = symbol;
        a.unlocked_limit.symbol = symbol;
    });
}

void vesting::close(name owner, symbol symbol) {
    require_auth(owner);
    tables::account_table account(_self, owner.value);
    auto it = account.find(symbol.code().raw());
    eosio_assert(it != account.end(), "Balance row already deleted or never existed. Action won't have any effect");
    eosio_assert(it->vesting.amount == 0, "Cannot close because the balance vesting is not zero");
    eosio_assert(it->delegate_vesting.amount == 0, "Cannot close because the balance delegate vesting is not zero");
    eosio_assert(it->received_vesting.amount == 0, "Cannot close because the balance received vesting not zero");
    account.erase(it);
}

void vesting::notify_balance_change(name owner, asset diff) {
    tables::vesting_table table_vesting(_self, _self.value);
    auto notify = table_vesting.find(diff.symbol.code().raw());
    action(
        permission_level{_self, config::active_name},
        notify->notify_acc,
        "changevest"_n,
        std::make_tuple(owner, diff)
    ).send();
}

void vesting::sub_balance(name owner, asset value, bool retire_mode) {
    eosio_assert(value.amount >= 0, "sub_balance: value.amount < 0");
    if (value.amount == 0)
        return;
    tables::account_table account(_self, owner.value);
    const auto& from = account.get(value.symbol.code().raw(), "no balance object found");
    if (retire_mode)
        eosio_assert(from.unlocked_vesting() >= value, "overdrawn unlocked balance");
    else
        eosio_assert(from.available_vesting() >= value, "overdrawn balance");

    account.modify(from, name(), [&](auto& a) {
        a.vesting -= value;
        if (retire_mode)
            a.unlocked_limit -= value;
        // Add notify about vesting changed
    });
    notify_balance_change(owner, -value);
}

void vesting::add_balance(name owner, asset value, name ram_payer) {
    eosio_assert(value.amount >= 0, "add_balance: value.amount < 0");
    if (value.amount == 0)
        return;
    tables::account_table account(_self, owner.value);
    auto to = account.find(value.symbol.code().raw());
    if (to == account.end()) {
        account.emplace(ram_payer, [&](auto& a) {
            a.vesting = value;
            a.delegate_vesting.symbol = value.symbol;
            a.received_vesting.symbol = value.symbol;
            // TODO Add notify about vesting changed
        });
    } else {
        account.modify(to, name(), [&](auto& a) {
            a.vesting += value;
            // TODO Add notify about vesting changed
        });
    }
    notify_balance_change(owner, value);
}

void vesting::send_account_event(name account, const structures::user_balance& balance) {
    eosio::event(_self, "balance"_n, std::make_tuple(account, balance)).send();
}

void vesting::send_vesting_event(const structures::vesting_info& info) {
    eosio::event(_self, "vesting"_n, info.supply).send();
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

const asset vesting::convert_to_token(const asset& src, const structures::vesting_info& vinfo) const {
    auto sym = src.symbol;
    auto token_supply = token::get_balance(config::token_name, _self, sym.code());
    // eosio_assert(sym.name() == token_supply.symbol.name() && sym == vinfo.supply.symbol, "The token type does not match");   // guaranteed to be valid here

    int64_t amount = vinfo.supply.amount && token_supply.amount
        ? static_cast<int64_t>((static_cast<int128_t>(src.amount) * token_supply.amount)
            / (vinfo.supply.amount + src.amount))
        : fix_precision(src, token_supply.symbol);
    return asset(amount, token_supply.symbol);
}

const asset vesting::convert_to_vesting(const asset& src, const structures::vesting_info& vinfo) const {
    auto sym = src.symbol;
    auto balance = token::get_balance(config::token_name, _self, sym.code()) - src;
    // eosio_assert(sym == balance.symbol && sym.name() == vinfo.supply.symbol.name(), "The token type does not match"); //

    int64_t amount = vinfo.supply.amount && balance.amount
        ? static_cast<int64_t>((static_cast<int128_t>(src.amount) * vinfo.supply.amount) / balance.amount)
        : fix_precision(src, vinfo.supply.symbol);
    return asset(amount, vinfo.supply.symbol);
}

void vesting::timeout_delay_trx() {
    require_auth(_self);
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, config::active_name), _self, "timeoutrdel"_n, structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, config::active_name), _self, "timeoutconv"_n, structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, config::active_name), _self, "timeout"_n,     structures::shash{now()}});
    trx.delay_sec = config::vesting_delay_tx_timeout;
    trx.send(_self.value, _self);
}

void vesting::paydelegator(name account, asset reward, name delegator, 
        uint16_t interest_rate, uint8_t payout_strategy) {
    require_auth(_self);
    if (payout_strategy == config::payout_strategy::to_delegated_vesting) {
        tables::delegate_table table(_self, reward.symbol.code().raw());
        auto index_table = table.get_index<"unique"_n>();
        auto delegate_record = index_table.find(structures::delegate_record::unique_key(delegator, account));
        if (delegate_record != index_table.end()) {
            tables::account_table acc_table_dlg(_self, delegator.value);
            auto balance_dlg = acc_table_dlg.find(reward.symbol.code().raw());
            acc_table_dlg.modify(balance_dlg, name(), [&](auto& item) {
                item.delegate_vesting += reward;
            });
            tables::account_table acc_table_rcv(_self, account.value);
            auto balance_rcv = acc_table_rcv.find(reward.symbol.code().raw());
            acc_table_rcv.modify(balance_rcv, name(), [&](auto& item) {
                item.received_vesting += reward;
            });
            index_table.modify(delegate_record, name(), [&](auto& item) {
                item.quantity += reward;
            });
        }
    }
    add_balance(delegator, reward, same_payer); 
}

} // golos
