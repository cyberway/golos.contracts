#include "geos.vesting.hpp"
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
    , _table_convert(_self, _self)
    , _table_delegate_vesting(_self, _self)
{}

void vesting::apply(uint64_t code, uint64_t action) {
    if (N(transfer) == action && N(golos.token) == code)
        buy_vesting(unpack_action_data<token::transfer_args>());

    else if (N(accruevg) == action)
        accrue_vesting(unpack_action_data<structures::accrue_vesting>());
    else if (N(convertvg) == action)
        convert_vesting(unpack_action_data<structures::transfer_vesting>());
    else if (N(delegatevg) == action)
        delegate_vesting(unpack_action_data<structures::delegate_vesting>());
    else if (N(undelegatevg) == action)
        undelegate_vesting(unpack_action_data<structures::transfer_vesting>());

    else if (N(issue) == action)
        issue(unpack_action_data<structures::issue_vesting>());
    else if (N(createpair) == action)
        create_pair(unpack_action_data<structures::pair_token_vesting>());
    else if (N(timeoutrdel) == action)
        calculate_delegate_vesting();
    else if (N(timeoutconv) == action)
        calculate_convert_vesting();
    else if (N(timeout) == action)
        timeout_delay_trx();
}

void vesting::buy_vesting(const token::transfer_args &m_transfer) {
    if(_self != m_transfer.from) // TODO account golos.vesting can not buy vesting
        require_auth(m_transfer.from);
    else return;

    auto sym_name = m_transfer.quantity.symbol.name();
    auto index = _table_pair.get_index<N(token)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    issue({_self, convert_token(m_transfer.quantity, pair->vesting.symbol)}, false, true);
    transfer(m_transfer.to, m_transfer.from, m_transfer.quantity, false);
}

void vesting::accrue_vesting(const structures::accrue_vesting &m_accrue_vesting) {
    require_auth(m_accrue_vesting.sender);

    require_recipient(m_accrue_vesting.sender);
    require_recipient(m_accrue_vesting.user);

    auto sym_name = m_accrue_vesting.quantity.symbol.name();
    auto index = _table_pair.get_index<N(vesting)>();
    auto pair = index.find(sym_name);
    eosio_assert(pair != index.end(), "Token not found");

    index.modify(pair, _self, [&](auto &item) {
        item.vesting += m_accrue_vesting.quantity;
    });

    add_balance(m_accrue_vesting.user, m_accrue_vesting.quantity, m_accrue_vesting.sender);
}

void vesting::convert_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    require_auth(m_transfer_vesting.sender);

    tables::account_table account(_self, m_transfer_vesting.sender);
    auto vest = account.find(m_transfer_vesting.quantity.symbol.name());
    eosio_assert(vest->vesting.amount >= m_transfer_vesting.quantity.amount, "Insufficient funds.");

    auto record = _table_convert.find(m_transfer_vesting.sender);
    if (record != _table_convert.end()) {
        _table_convert.modify(record, m_transfer_vesting.sender, [&]( auto &item ) {
            item.recipient = m_transfer_vesting.recipient;
            item.number_of_payments = OUTPUT_WEEK_COUNT;
            item.payout_period = OUTPUT_PAYOUT_PERIOD;
            item.payout_part = m_transfer_vesting.quantity/OUTPUT_WEEK_COUNT;
            item.balance_amount = m_transfer_vesting.quantity;
        });
    } else {
        _table_convert.emplace(m_transfer_vesting.sender, [&]( auto &item ) {
            item.sender = m_transfer_vesting.sender;
            item.recipient = m_transfer_vesting.recipient;
            item.number_of_payments = OUTPUT_WEEK_COUNT;
            item.payout_period = OUTPUT_PAYOUT_PERIOD;
            item.payout_part = m_transfer_vesting.quantity/OUTPUT_WEEK_COUNT;
            item.balance_amount = m_transfer_vesting.quantity;
        });
    }
}

void vesting::delegate_vesting(const structures::delegate_vesting &m_delegate_vesting) {
    eosio_assert(m_delegate_vesting.sender != m_delegate_vesting.recipient, "You can not delegate to yourself");

    tables::account_table account_sender(_self, m_delegate_vesting.sender);
    auto balance_sender = account_sender.find(m_delegate_vesting.quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "Not found token");
    eosio_assert(balance_sender->vesting >= m_delegate_vesting.quantity, "Not enough vesting");

    account_sender.modify(balance_sender, m_delegate_vesting.sender, [&](auto &item){
        item.vesting -= m_delegate_vesting.quantity;
    });

    tables::delegate_table table(_self, m_delegate_vesting.sender);
    auto it_recipient = table.find(m_delegate_vesting.quantity.symbol.name());
    if (it_recipient != table.end()) {
        table.modify(it_recipient, m_delegate_vesting.sender, [&](structures::delegate_record &item){
            item.quantity += m_delegate_vesting.quantity;
            item.percentage_deductions = m_delegate_vesting.percentage_deductions;
            item.return_date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
        });
    } else {
        table.emplace(m_delegate_vesting.sender, [&](structures::delegate_record &item){
            item.recipient = m_delegate_vesting.recipient;
            item.quantity = m_delegate_vesting.quantity;
            item.percentage_deductions = m_delegate_vesting.percentage_deductions;
            item.return_date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
//            item.return_date = time_point_sec(now()); // TODO test
        });
    }

    tables::account_table account_recipient(_self, m_delegate_vesting.recipient);
    auto balance_recipient = account_recipient.find(m_delegate_vesting.quantity.symbol.name());
    if (balance_recipient != account_recipient.end()) {
        account_recipient.modify(balance_recipient, m_delegate_vesting.sender, [&](auto &item){
            item.delegate_vesting += m_delegate_vesting.quantity;
        });
    } else {
        account_recipient.emplace(m_delegate_vesting.sender, [&](auto &item){
            item.delegate_vesting = m_delegate_vesting.quantity;
            item.vesting.symbol = m_delegate_vesting.quantity.symbol;
            item.received_vesting.symbol = m_delegate_vesting.quantity.symbol;
        });
    }
}

void vesting::undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    tables::delegate_table table(_self, m_transfer_vesting.sender);
    auto it_delegate = table.find(m_transfer_vesting.quantity.symbol.name());
    eosio_assert(it_delegate != table.end(), "Not enough delegated vesting");
    eosio_assert(it_delegate->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(it_delegate->quantity >= m_transfer_vesting.quantity, "There are not enough delegated tools for output");

    if (it_delegate->quantity == m_transfer_vesting.quantity) {
        table.erase(it_delegate);
    } else {
        table.modify(it_delegate, m_transfer_vesting.sender, [&](auto &item){
            item.quantity -= m_transfer_vesting.quantity;
        });
    }

    _table_delegate_vesting.emplace(m_transfer_vesting.sender, [&](structures::return_delegate &item){
        item.id = _table_delegate_vesting.available_primary_key();
        item.recipient = m_transfer_vesting.sender;
        item.amount = m_transfer_vesting.quantity;
        item.date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
    });

    tables::account_table account_recipient(_self, m_transfer_vesting.recipient);
    auto balance = account_recipient.find(m_transfer_vesting.quantity.symbol.name());
    eosio_assert(balance != account_recipient.end(), "This token is not on the balance sheet");
    account_recipient.modify(balance, m_transfer_vesting.sender, [&](auto &item){
        item.delegate_vesting -= m_transfer_vesting.quantity;
    });
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);

    for (auto obj = _table_convert.begin(); obj != _table_convert.end(); ) {
        if (obj->number_of_payments > 0 && (obj->payout_period > 0 && obj->payout_period <= OUTPUT_PAYOUT_PERIOD)) {
            _table_convert.modify(obj, _self, [&](auto &item){
                item.payout_period -= TIMEOUT;
                print(item.payout_period, "\n");
            });
        }

        if (obj->number_of_payments > 0 && (obj->payout_period <= 0 || obj->payout_period > OUTPUT_PAYOUT_PERIOD)) {
            _table_convert.modify(obj, _self, [&](auto &item) {
                item.payout_period = OUTPUT_PAYOUT_PERIOD;
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

                    INLINE_ACTION_SENDER(eosio::token, transfer)( N(golos.token), {_self, N(active)},
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
            obj = _table_convert.erase(obj);
        }
    }
}

void vesting::calculate_delegate_vesting() {
    require_auth(_self);

    for (auto obj = _table_delegate_vesting.begin(); obj != _table_delegate_vesting.end(); ) {
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

            obj = _table_delegate_vesting.erase(obj);
        } else {
            ++obj;
        }
    }
}

void vesting::create_pair(const structures::pair_token_vesting &token_vesting) {
    require_auth(_self);

    auto index_token = _table_pair.get_index<N(token)>();
    auto it_token = index_token.find(token_vesting.token.symbol.name());
    eosio_assert(it_token == index_token.end(), "Pair with such a token already exists");

    auto index = _table_pair.get_index<N(vesting)>();
    auto it_vesting = index.find(token_vesting.vesting.symbol.name());
    eosio_assert(it_vesting == index.end(), "Pair with such a vesting already exists");

    _table_pair.emplace(_self, [&](auto &item){
        item.id = _table_pair.available_primary_key();
        item.vesting.symbol = token_vesting.vesting.symbol;
        item.token.symbol   = token_vesting.token.symbol;
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
