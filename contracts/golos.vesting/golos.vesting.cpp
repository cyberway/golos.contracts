#include "golos.vesting.hpp"
#include <eosiolib/transaction.hpp>


using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        vesting(receiver).apply(code, action);
    }
}

vesting::vesting(account_name self)
    : contract(self)
    , _table_pair(_self, _self)
    , _table_delegate_vesting(_self, _self)
{}

void vesting::apply(uint64_t code, uint64_t action) {
    if (N(transfer) == action && N(eosio.token) == code)
        execute_action( this, &vesting::buy_vesting );

    else if (N(accruevg) == action)
        execute_action(this, &vesting::accrue_vesting);
    else if (N(convertvg) == action)
        execute_action(this, &vesting::convert_vesting);
    else if (N(delegatevg) == action)
        execute_action(this, &vesting::delegate_vesting);
    else if (N(undelegatevg) == action)
        execute_action(this, &vesting::undelegate_vesting);
    else if (N(cancelvg) == action)
        execute_action(this, &vesting::cancel_convert_vesting);

    else if (N(issue) == action)
//        issue(unpack_action_data<structures::issue_vesting>());
        execute_action(this, &vesting::issue);
    else if (N(createpair) == action)
        execute_action(this, &vesting::create_pair);
    else if (N(timeoutrdel) == action)
        calculate_delegate_vesting();
    else if (N(timeoutconv) == action)
        calculate_convert_vesting();
    else if (N(timeout) == action)
        timeout_delay_trx();
}

void vesting::buy_vesting(account_name  from,
                          account_name  to,
                          asset         quantity,
                          string        /*memo*/) {
    if(_self != from) // TODO account golos.vesting can not buy vesting
        require_auth(from);
    else return;

    auto sym_name = quantity.symbol.name();
    auto index = _table_pair.get_index<N(token)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    issue({_self, convert_token(quantity, pair->vesting.symbol)}, false, true);
    transfer(to, from, quantity, false);
}

void vesting::accrue_vesting(account_name sender, account_name user, asset quantity) {
    require_auth(sender);

    require_recipient(sender);
    require_recipient(user);

    auto sym_name = quantity.symbol.name();
    auto index = _table_pair.get_index<N(vesting)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    index.modify(pair, _self, [&](auto &item) {
        item.vesting += quantity;
    });

    add_balance(user, quantity, sender);
}

void vesting::convert_vesting(account_name sender, account_name recipient, asset quantity) {
    require_auth(sender);

    tables::account_table account(_self, sender);
    auto vest = account.find(quantity.symbol.name());
    eosio_assert(vest->vesting.amount >= quantity.amount, "Insufficient funds.");

    tables::convert_table table(_self, quantity.symbol.name());
    auto record = table.find(sender);
    if (record != table.end()) {
        table.modify(record, sender, [&]( auto &item ) {
            item.recipient = recipient;
            item.number_of_payments = OUTPUT_WEEK_COUNT;
            item.payout_time = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
            item.payout_part = quantity/OUTPUT_WEEK_COUNT;
            item.balance_amount = quantity;
        });
    } else {
        table.emplace(sender, [&]( auto &item ) {
            item.sender = sender;
            item.recipient = recipient;
            item.number_of_payments = OUTPUT_WEEK_COUNT;
            item.payout_time = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
            item.payout_part = quantity/OUTPUT_WEEK_COUNT;
            item.balance_amount = quantity;
        });
    }
}

void vesting::cancel_convert_vesting(account_name sender, asset type) {
    require_auth(sender);

    tables::convert_table table(_self, type.symbol.name());
    auto record = table.find(sender);
    eosio_assert(record != table.end(), "not record");

    table.erase(record);
}

void vesting::delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t percentage_deductions) {
    eosio_assert(sender != recipient, "You can not delegate to yourself");

    tables::account_table account_sender(_self, sender);
    auto balance_sender = account_sender.find(quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "Not found token");
    eosio_assert(balance_sender->vesting >= quantity, "Not enough vesting");

    account_sender.modify(balance_sender, sender, [&](auto &item){
        item.vesting -= quantity;
    });

    tables::delegate_table table(_self, sender);
    auto it_recipient = table.find(quantity.symbol.name());
    if (it_recipient != table.end()) {
        table.modify(it_recipient, sender, [&](structures::delegate_record &item){
            item.quantity += quantity;
            item.percentage_deductions = percentage_deductions;
        });
    } else {
        table.emplace(sender, [&](structures::delegate_record &item){
            item.recipient = recipient;
            item.quantity = quantity;
            item.percentage_deductions = percentage_deductions;
//            item.return_date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
            item.return_date = time_point_sec(now()); // TODO test
        });
    }

    tables::account_table account_recipient(_self, recipient);
    auto balance_recipient = account_recipient.find(quantity.symbol.name());
    if (balance_recipient != account_recipient.end()) {
        account_recipient.modify(balance_recipient, sender, [&](auto &item){
            item.delegate_vesting += quantity;
        });
    } else {
        account_recipient.emplace(sender, [&](auto &item){
            item.delegate_vesting = quantity;
            item.vesting.symbol = quantity.symbol;
            item.received_vesting.symbol = quantity.symbol;
        });
    }
}

void vesting::undelegate_vesting(account_name sender, account_name recipient, asset quantity) {
    tables::delegate_table table(_self, sender);
    auto it_delegate = table.find(quantity.symbol.name());
    eosio_assert(it_delegate != table.end(), "Not enough delegated vesting");
    eosio_assert(it_delegate->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(it_delegate->quantity >= quantity, "There are not enough delegated tools for output");

    if (it_delegate->quantity == quantity) {
        table.erase(it_delegate);
    } else {
        table.modify(it_delegate, sender, [&](auto &item){
            item.quantity -= quantity;
        });
    }

    _table_delegate_vesting.emplace(sender, [&](structures::return_delegate &item){
        item.id = _table_delegate_vesting.available_primary_key();
        item.recipient = sender;
        item.amount = quantity;
        item.date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
    });

    tables::account_table account_recipient(_self, recipient);
    auto balance = account_recipient.find(quantity.symbol.name());
    eosio_assert(balance != account_recipient.end(), "This token is not on the balance sheet");
    account_recipient.modify(balance, sender, [&](auto &item){
        item.delegate_vesting -= quantity;
    });
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);

    for(auto obj_pair : _table_pair) {
        tables::convert_table table(_self, obj_pair.vesting.symbol.name());
        auto index = table.get_index<N(payout_time)>();
        for (auto obj = index.cbegin(); obj != index.cend(); ) {
            if (obj->payout_time <= time_point_sec(now())) {
                if(obj->number_of_payments > 0) {
                    index.modify(obj, 0, [&](auto &item) {
                        item.payout_time = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
                        item.number_of_payments -= WEEK_PERIOD;

                        tables::account_table account(_self, obj->sender);
                        auto balance = account.find(obj->payout_part.symbol.name());

                        auto lambda_action_sender = [&](asset quantity){
                            account.modify(balance, 0, [&](auto &item){
                                item.vesting -= quantity;
                            });

                            auto sym_name = quantity.symbol.name();
                            auto index = _table_pair.get_index<N(vesting)>();
                            auto pair = index.find(sym_name);
                            eosio_assert(pair != index.end(), "Vesting not found");

                            index.modify(pair, 0, [&](auto &item) {
                                item.vesting -= quantity;
                                item.token -= convert_token(quantity, pair->token.symbol);
                            });

                            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {_self, N(active)},
                            { _self, obj->recipient, convert_token(quantity, pair->token.symbol), "Translation into tokens" } );
                        };

                        if (balance->vesting < obj->payout_part) {
                            item.number_of_payments = NOT_TOKENS_WEEK_PERIOD;
                        } else if (obj->number_of_payments == WEEK_PERIOD) {
                            lambda_action_sender(obj->balance_amount - (obj->payout_part * (OUTPUT_WEEK_COUNT - WEEK_PERIOD)));
                        } else {
                            lambda_action_sender(obj->payout_part);
                        }
                    });
                }

                if (obj->number_of_payments) {
                    ++obj;
                } else {
                    obj = index.erase(obj);
                }
            } else {
                break;
            }
        }
    }
}

void vesting::calculate_delegate_vesting() {
    require_auth(_self);
    auto index = _table_delegate_vesting.get_index<N(date)>();
    for (auto obj = index.cbegin(); obj != index.cend(); ) {
        if (obj->date <= time_point_sec(now())) {
            tables::account_table account(_self, obj->recipient);
            auto balance = account.find(obj->amount.symbol.name());
            if (balance != account.end()) {
                account.modify(balance, 0, [&](auto &item){
                    item.vesting += obj->amount;
                });
            } else {
                account.emplace(_self, [&](auto &item){
                    item.vesting = obj->amount;
                });
            }

            obj = index.erase(obj);
        } else {
            break;
        }
    }
}

void vesting::create_pair(asset token, asset vesting) {
    require_auth(_self);

    auto index_token = _table_pair.get_index<N(token)>();
    auto it_token = index_token.find(token.symbol.name());
    eosio_assert(it_token == index_token.end(), "Pair with such a token already exists");

    auto index = _table_pair.get_index<N(vesting)>();
    auto it_vesting = index.find(vesting.symbol.name());
    eosio_assert(it_vesting == index.end(), "Pair with such a vesting already exists");

    _table_pair.emplace(_self, [&](auto &item){
        item.id = _table_pair.available_primary_key();
        item.vesting.symbol = vesting.symbol;
        item.token.symbol   = token.symbol;
    });
}

void vesting::issue(const structures::issue_vesting &m_issue, bool is_autorization, bool is_buy) {
    if (is_autorization)
        require_auth(_self);

    auto sym = m_issue.quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );

    auto sym_name = sym.name();
    auto index = _table_pair.get_index<N(vesting)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Vesting not found");

    index.modify(pair, _self, [&](auto &item) {
        item.vesting += m_issue.quantity;
        if (is_buy)
            item.token = convert_token(m_issue.quantity, pair->token.symbol);
    });

    add_balance(m_issue.to, m_issue.quantity, _self);
}

void vesting::transfer(account_name from, account_name to, asset quantity, bool is_autorization) {
    eosio_assert(from != to, "cannot transfer to self");
    if (is_autorization)
        require_auth(from);
    eosio_assert(is_account(to), "to account does not exist");

    require_recipient(from);
    require_recipient(to);

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");

    auto sym_name = quantity.symbol.name();
    auto index = _table_pair.get_index<N(token)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    sub_balance(from, convert_token(quantity, pair->vesting.symbol));
    add_balance(to, convert_token(quantity, pair->vesting.symbol), from);
}

void vesting::sub_balance(account_name owner, asset value) {
    tables::account_table account(_self, owner);

    const auto& from = account.get(value.symbol.name(), "no balance object found");
    eosio_assert(from.vesting >= value, "overdrawn balance");

    if(from.vesting == value) {
       account.erase(from);
    } else {
       account.modify(from, 0, [&]( auto& a) {
           a.vesting -= value;
       });
    }
}

void vesting::add_balance(account_name owner, asset value, account_name ram_payer) {
    tables::account_table account(_self, owner);
    auto to = account.find(value.symbol.name());
    if( to == account.end() ) {
        account.emplace( ram_payer, [&]( auto& a ){
            a.vesting = value;
            a.delegate_vesting.symbol = value.symbol;
            a.received_vesting.symbol = value.symbol;
        });
    } else {
        account.modify( to, 0, [&]( auto& a ) {
            a.vesting += value;
        });
    }
}

asset vesting::convert_token(const asset &m_token, symbol_type type) {
    asset type_token = m_token;
    type_token.symbol = type;
    return type_token;
}

void vesting::timeout_delay_trx() {
    require_auth(_self);

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutrdel), structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutconv), structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeout),     structures::shash{now()}});
    trx.delay_sec = TIMEOUT;
    trx.send(_self, _self);
}
