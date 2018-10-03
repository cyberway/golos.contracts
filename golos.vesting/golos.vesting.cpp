#include "golos.vesting.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/fixedpoint.hpp>

#include "eosio.token/eosio.token.hpp"

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        vesting(receiver).apply(code, action);
    }
}

void vesting::apply(uint64_t code, uint64_t action) {
    if (N(transfer) == action && N(eosio.token) == code)
        execute_action(this, &vesting::buy_vesting);

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

    else if (N(open) == action)
        execute_action(this, &vesting::open);
    else if (N(close) == action)
        execute_action(this, &vesting::close);

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

    eosio_assert(from != to, "cannot transfer to self");
    eosio_assert(is_account(to), "to account does not exist");
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");

    auto sym_name = quantity.symbol.name();
    tables::vesting_table table_pair(_self, _self);
    auto index = table_pair.get_index<N(token)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    const auto &tokens = convert_to_vesting(quantity, *pair);
    index.modify(pair, 0, [&](auto &item) {
        item.vesting += tokens;
        item.token += quantity;
    });

    auto payer = has_auth( to ) ? to : from;

    require_recipient(from);
    add_balance(from, tokens, payer);
}

void vesting::accrue_vesting(account_name sender, account_name user, asset quantity) {
    require_auth(sender);

    require_recipient(user);

    auto sym_name = quantity.symbol.name();
    tables::vesting_table table_pair(_self, _self);
    auto index = table_pair.get_index<N(vesting)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");
    index.modify(pair, _self, [&](auto &item) {
        item.vesting += quantity;
    });

    // TODO deductions
    tables::account_table account(_self, user);
    auto balance = account.find(sym_name);
    if (balance == account.end()) {
        add_balance(user, quantity, sender);
        return;
    }

    asset summary_delegate;
    summary_delegate.symbol = quantity.symbol;

    tables::delegate_table table_delegate(_self, sym_name);
    auto index_delegate = table_delegate.get_index<N(recipient)>();
    auto it_index_user = index_delegate.find(user);

    asset efective_vesting = balance->vesting + balance->received_vesting;
    while (it_index_user != index_delegate.end() && it_index_user->recipient == user && bool_asset(efective_vesting)) {
        auto proportion = it_index_user->quantity * FRACTION / efective_vesting;
        const auto &delegates_award = (((quantity * proportion) / FRACTION) * it_index_user->percentage_deductions) / FRACTION;

        index_delegate.modify(it_index_user, 0, [&](auto &item) {
            item.deductions += delegates_award;
        });

        summary_delegate += delegates_award;
        ++it_index_user;
    }

    add_balance(user, quantity - summary_delegate, sender);
}

void vesting::convert_vesting(account_name sender, account_name recipient, asset quantity) {
    require_auth(sender);

    tables::account_table account(_self, sender);
    auto vest = account.find(quantity.symbol.name());
    eosio_assert(vest->vesting.amount >= quantity.amount, "Insufficient funds.");
    eosio_assert(MIN_AMOUNT_VESTING_CONCLUSION <= vest->vesting.amount, "Insufficient funds for converting");

    tables::convert_table table(_self, quantity.symbol.name());
    auto record = table.find(sender);
    if (record != table.end()) {
        table.modify(record, sender, [&]( auto &item ) {
            item.recipient = recipient;
            item.number_of_payments = NUMBER_OF_CONVERSIONS;
            item.payout_time = time_point_sec(now() + TOTAL_TERM_OF_CONCLUSION);
            item.payout_part = quantity/NUMBER_OF_CONVERSIONS;
            item.balance_amount = quantity;
        });
    } else {
        table.emplace(sender, [&]( auto &item ) {
            item.sender = sender;
            item.recipient = recipient;
            item.number_of_payments = NUMBER_OF_CONVERSIONS;
            item.payout_time = time_point_sec(now() + TOTAL_TERM_OF_CONCLUSION);
            item.payout_part = quantity/NUMBER_OF_CONVERSIONS;
            item.balance_amount = quantity;
        });
    }
}

void vesting::cancel_convert_vesting(account_name sender, asset type) {
    require_auth(sender);

    tables::convert_table table(_self, type.symbol.name());
    auto record = table.find(sender);
    eosio_assert(record != table.end(), "Not found convert record sender");

    table.erase(record);
}

void vesting::delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t percentage_deductions) {
    require_auth(sender);
    eosio_assert(sender != recipient, "You can not delegate to yourself");

    eosio_assert(MIN_AMOUNT_DELEGATION_VESTING <= quantity.amount, "Insufficient funds for delegation");

    tables::account_table account_sender(_self, sender);
    auto balance_sender = account_sender.find(quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "Not found token");
    eosio_assert(balance_sender->vesting >= quantity, "Not enough vesting");

    account_sender.modify(balance_sender, sender, [&](auto &item){
        item.vesting -= quantity;
        item.delegate_vesting += quantity;
    });

    const auto unique_key = (uint128_t)sender + (((uint128_t)recipient)<<64);
    tables::delegate_table table(_self, quantity.symbol.name());
    auto index_table = table.get_index<N(unique_key)>();
    auto delegate_record = index_table.find(unique_key);
    if (delegate_record != index_table.end()) {
        index_table.modify(delegate_record, 0, [&](auto &item){
            item.quantity += quantity;
            item.percentage_deductions = percentage_deductions;
        });
    } else {
        table.emplace(sender, [&](structures::delegate_record &item){
            item.id = table.available_primary_key();
            item.unique_key = unique_key;
            item.sender = sender;
            item.recipient = recipient;
            item.quantity = quantity;
            item.deductions.symbol = quantity.symbol;
            item.percentage_deductions = percentage_deductions;
            item.return_date = time_point_sec(now() + TIME_LIMIT_FOR_RETURN_DELEGATE_VESTING);
        });
    }

    tables::account_table account_recipient(_self, recipient);
    auto balance_recipient = account_recipient.find(quantity.symbol.name());
    eosio_assert(balance_recipient != account_recipient.end(), "Not found balance token vesting");
    account_recipient.modify(balance_recipient, sender, [&](auto &item){
        item.received_vesting += quantity;
    });
}

void vesting::undelegate_vesting(account_name sender, account_name recipient, asset quantity) {
    require_auth(sender);

    const auto unique_key = (uint128_t)sender + (((uint128_t)recipient)<<64);
    tables::delegate_table table(_self, quantity.symbol.name());
    auto index_table = table.get_index<N(unique_key)>();
    auto delegate_record = index_table.find(unique_key);
    eosio_assert(delegate_record != index_table.end(), "Not enough delegated vesting");

    eosio_assert(delegate_record->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(delegate_record->quantity >= quantity, "There are not enough delegated tools for output");

    asset part_deductions;
    if (delegate_record->quantity == quantity) {
        part_deductions = delegate_record->deductions;
        index_table.erase(delegate_record);
    } else {
        auto persent = (quantity * FULL_PERSENT) / delegate_record->quantity;
        part_deductions = delegate_record->deductions * persent / FULL_PERSENT;

        index_table.modify(delegate_record, sender, [&](auto &item){
            item.quantity -= quantity;
            item.deductions -= part_deductions;
        });
    }

    tables::return_delegate_table table_delegate_vesting(_self, _self);
    table_delegate_vesting.emplace(sender, [&](structures::return_delegate &item){
        item.id = table_delegate_vesting.available_primary_key();
        item.recipient = sender;
        item.amount = quantity + part_deductions;
        item.date = time_point_sec(now() + TIME_LIMIT_FOR_RETURN_DELEGATE_VESTING);
    });

    tables::account_table account_recipient(_self, recipient);
    auto balance = account_recipient.find(quantity.symbol.name());
    eosio_assert(balance != account_recipient.end(), "This token is not on the recipient balance sheet");
    account_recipient.modify(balance, sender, [&](auto &item){
        item.received_vesting -= quantity;
    });

    tables::account_table account_sender(_self, sender);
    auto balance_sender = account_sender.find(quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "This token is not on the sender balance sheet");
    account_sender.modify(balance_sender, sender, [&](auto &item){
        item.delegate_vesting -= quantity;
    });
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);

    tables::vesting_table table_pair(_self, _self);
    for(auto obj_pair : table_pair) {
        tables::convert_table table(_self, obj_pair.vesting.symbol.name());
        auto index = table.get_index<N(payout_time)>();
        for (auto obj = index.cbegin(); obj != index.cend(); ) {

            if (obj->payout_time > time_point_sec(now()))
                break;

            if(obj->number_of_payments > 0) {
                index.modify(obj, 0, [&](auto &item) {
                    item.payout_time = time_point_sec(now() + TOTAL_TERM_OF_CONCLUSION);
                    --item.number_of_payments;

                    tables::account_table account(_self, obj->sender);
                    auto balance = account.find(obj->payout_part.symbol.name());
                    eosio_assert(balance != account.end(), "Not found vesting balance");

                    auto lambda_action_sender = [&](asset quantity){
                        account.modify(balance, 0, [&](auto &item){
                            item.vesting -= quantity;
                        });

                        auto sym_name = quantity.symbol.name();
                        auto index = table_pair.get_index<N(vesting)>();
                        auto pair = index.find(sym_name);
                        eosio_assert(pair != index.end(), "Vesting not found");

                        const auto &tokens = convert_to_token(quantity, *pair);
                        index.modify(pair, 0, [&](auto &item) {
                            item.vesting -= quantity;
                            item.token -= tokens;
                        });

                        INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {_self, N(active)},
                        { _self, obj->recipient, tokens, "Translation into tokens" } );
                    };

                    if (balance->vesting < obj->payout_part) {
                        item.number_of_payments = 0;
                        lambda_action_sender(balance->vesting);
                    } else if (!obj->number_of_payments) { // TODO obj->number_of_payments == 0
                        lambda_action_sender(obj->balance_amount - (obj->payout_part * (NUMBER_OF_CONVERSIONS - 1)));
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
        }
    }
}

void vesting::calculate_delegate_vesting() {
    require_auth(_self);
    tables::return_delegate_table table_delegate_vesting(_self, _self);
    auto index = table_delegate_vesting.get_index<N(date)>();
    for (auto obj = index.cbegin(); obj != index.cend(); ) {
        if (obj->date > time_point_sec(now()))
            break;

        tables::account_table account(_self, obj->recipient);
        auto balance = account.find(obj->amount.symbol.name());
        eosio_assert(balance != account.end(), "Not found balance token vesting");
        account.modify(balance, 0, [&](auto &item){
            item.vesting += obj->amount;
        });

        obj = index.erase(obj);
    }
}

void vesting::create_pair(asset token, asset vesting) {
    require_auth(_self);

    tables::vesting_table table_pair(_self, _self);
    auto index_token = table_pair.get_index<N(token)>();
    auto it_token = index_token.find(token.symbol.name());
    eosio_assert(it_token == index_token.end(), "Pair with such a token already exists");

    auto index = table_pair.get_index<N(vesting)>();
    auto it_vesting = index.find(vesting.symbol.name());
    eosio_assert(it_vesting == index.end(), "Pair with such a vesting already exists");

    table_pair.emplace(_self, [&](auto &item){
        item.id = table_pair.available_primary_key();
        item.vesting.symbol = vesting.symbol;
        item.token.symbol   = token.symbol;
    });
}

void vesting::open(account_name owner, symbol_type symbol, account_name ram_payer) {
    require_auth( ram_payer );
    tables::account_table accounts( _self, owner );
    auto it = accounts.find( symbol.name() );
    eosio_assert( it == accounts.end(), "Токен существует" );
       accounts.emplace( ram_payer, [&]( auto& a ){
           a.vesting.symbol = symbol;
           a.delegate_vesting.symbol = symbol;
           a.received_vesting.symbol = symbol;
       });
}

void vesting::close(account_name owner, symbol_type symbol) {
    require_auth( owner );
    tables::account_table account( _self, owner );
    auto it = account.find( symbol.name() );
    eosio_assert( it != account.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    eosio_assert( it->vesting.amount == 0, "Cannot close because the balance vesting is not zero." );
    eosio_assert( it->delegate_vesting.amount == 0, "Cannot close because the balance delegate vesting is not zero." );
    eosio_assert( it->received_vesting.amount == 0, "Cannot close because the balance received vesting not zero." );
    account.erase( it );
}

void vesting::sub_balance(account_name owner, asset value) {
    tables::account_table account(_self, owner);

    const auto& from = account.get(value.symbol.name(), "no balance object found");
    eosio_assert(from.vesting >= value, "overdrawn balance");

    account.modify(from, 0, [&]( auto& a) {
        a.vesting -= value;
    });
}

void vesting::add_balance(account_name owner, asset value, account_name ram_payer) {
    tables::account_table account(_self, owner);
    auto to = account.find(value.symbol.name());
    eosio_assert(to != account.end(), "Not found balance token vesting");

//    if( to == account.end() ) {
//        open(owner, value.symbol, ram_payer);
//    } else {
        account.modify( to, 0, [&]( auto& a ) {
            a.vesting += value;
        });
//    }
}

const bool vesting::bool_asset(const asset &obj) const {
    return obj.amount;
}

const asset vesting::convert_to_token(const asset &m_token, const structures::token_vesting &pair) const {
    uint64_t amount;
    symbol_type symbol = pair.token.symbol;

    eosio_assert(m_token.symbol == pair.vesting.symbol, "The token type does not match.");

    if (!pair.vesting.amount || pair.token.amount)
        amount = m_token.amount;
    else {
        eosio_assert(pair.token.amount, "No tokens converted to vesting");
        amount = (m_token.amount * pair.token.amount) / pair.vesting.amount; // TODO m_token type vesting
    }

    symbol = pair.token.symbol;
    return asset(amount, symbol);
}

const asset vesting::convert_to_vesting(const asset &m_token, const structures::token_vesting &pair) const {
    uint64_t amount;
    symbol_type symbol = pair.vesting.symbol;

    eosio_assert(m_token.symbol == pair.token.symbol, "The token type does not match.");

    if (!pair.vesting.amount || pair.token.amount)
        amount = m_token.amount;
    else {
        eosio_assert(pair.vesting.amount, "No vesting converted to tokens");
        amount = (m_token.amount * pair.vesting.amount) / pair.token.amount; // TODO m_token type token
    }

    return asset(amount, symbol);
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
