#include "vesting-contract.hpp"

#include <eosiolib/transaction.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        vesting(receiver).apply(code, action);
    }
}

vesting::vesting(account_name self)
    : contract(self)
    , _table_convert(_self, _self)
    , _table_delegate_vesting(_self, _self)
{}

void vesting::apply(uint64_t /*code*/, uint64_t action) {
    if (N(buyvg) == action)
        buy_vesting(unpack_action_data<structures::transfer_vesting>());
    else if (N(accruevg) == action)
        accrue_vesting(unpack_action_data<structures::accrue_vesting>());
    else if (N(convertvg) == action)
        convert_vesting(unpack_action_data<structures::transfer_vesting>());
    else if (N(delegatevg) == action)
        delegate_vesting(unpack_action_data<structures::delegate_vesting>());
    else if (N(undelegatevg) == action)
        undelegate_vesting(unpack_action_data<structures::transfer_vesting>());

    if (N(init) == action)
        init_contract();
    else if (N(timeoutrdel) == action)
        calculate_delegate_vesting();
    else if (N(timeoutconv) == action)
        calculate_convert_vesting();
    else if (N(timeout) == action)
        timeout_delay_trx();
}

// need active permission users / Exemple: cleos set account permission golos active '{"threshold":1, "accounts":[{"permission":{"actor":"golos","permission":"eosio.code"}, "weight":1}]}' owner -p golos
void vesting::buy_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    require_auth(m_transfer_vesting.sender);

    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {m_transfer_vesting.sender, N(active)},
       { m_transfer_vesting.sender, N(pool), m_transfer_vesting.quantity, std::string("buy vesting") } );

    // TODO расчёт вестинга по коэффициенту
    add_balance(m_transfer_vesting.recipient, convert_token_to_vesting(m_transfer_vesting.quantity), m_transfer_vesting.sender);

    return;
    tables::vesting_table vest(_self, TOKEN_VESTING);
    auto existing = vest.find(TOKEN_VESTING);
    if (existing != vest.end()) {
        vest.modify(existing, m_transfer_vesting.sender, [&](auto &item){
            item.vesting_in_system += convert_token_to_vesting(m_transfer_vesting.quantity);
            item.tokens_in_vesting += m_transfer_vesting.quantity;
        });
    } else {
        vest.emplace(m_transfer_vesting.sender, [&](auto &item){
            item.vesting_in_system = convert_token_to_vesting(m_transfer_vesting.quantity);
            item.tokens_in_vesting = m_transfer_vesting.quantity;
        });
    }
}

void vesting::accrue_vesting(const structures::accrue_vesting &m_accrue_vesting) {
    require_auth(m_accrue_vesting.sender);

    add_balance(m_accrue_vesting.user, convert_token_to_vesting(m_accrue_vesting.quantity), m_accrue_vesting.sender);

    tables::vesting_table vest(_self, m_accrue_vesting.quantity.symbol.name());
    auto existing = vest.find(m_accrue_vesting.quantity.symbol.name());
    if (existing != vest.end()) {
        vest.modify(existing, m_accrue_vesting.sender, [&](auto &item){
            item.vesting_in_system += convert_token_to_vesting(m_accrue_vesting.quantity);
        });
    } else {
        vest.emplace(m_accrue_vesting.sender, [&](auto &item){
            item.vesting_in_system = convert_token_to_vesting(m_accrue_vesting.quantity);
        });
    }
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
    tables::account_table account_sender(_self, m_delegate_vesting.sender);
    eosio_assert(m_delegate_vesting.sender != m_delegate_vesting.recipient, "You can not delegate to yourself");

    auto balance_sender = account_sender.find(m_delegate_vesting.quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "Not a token");
    eosio_assert(balance_sender->vesting >= m_delegate_vesting.quantity, "Not enough vesting");
    account_sender.modify(balance_sender, m_delegate_vesting.sender, [&](auto &item){
        item.vesting -= m_delegate_vesting.quantity;
    });

    tables::delegate_table table(_self, m_delegate_vesting.sender);
    auto it_recipient = table.find(m_delegate_vesting.recipient);
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
//            item.return_date = time_point_sec(now() + OUTPUT_PAYOUT_PERIOD);
            item.return_date = time_point_sec(now());
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
            item.vesting = NULL_TOKEN_VESTING;
            item.received_vesting = NULL_TOKEN_VESTING;
        });
    }
}

void vesting::undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    tables::delegate_table table(_self, m_transfer_vesting.sender);
    auto it_recipient = table.find(m_transfer_vesting.recipient);
    eosio_assert(it_recipient != table.end(), "Not enough delegated vesting");

    eosio_assert(it_recipient->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");

    if (it_recipient->quantity == m_transfer_vesting.quantity) {
        table.erase(it_recipient);
    } else {
        table.modify(it_recipient, m_transfer_vesting.sender, [&](auto &item){
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

void vesting::init_contract() {
    require_auth(_self);
    timeout_delay_trx();
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
                if (balance->vesting < obj->payout_part) {
                    item.number_of_payments = NOT_TOKENS_WEEK_PERIOD;
                } else if (obj->number_of_payments == WEEK_PERIOD) {
                    account.modify(balance, _self, [&](auto &item){
                        item.vesting -= obj->balance_amount - (obj->payout_part * (OUTPUT_WEEK_COUNT - WEEK_PERIOD));
                    });

                    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(pool), N(active)},
                       { N(pool), obj->recipient, convert_vesting_to_token(obj->balance_amount - (obj->payout_part * (OUTPUT_WEEK_COUNT - WEEK_PERIOD))), std::string("payment vesting") } );
                } else {
                    account.modify(balance, _self, [&](auto &item){
                        item.vesting -= obj->payout_part;
                    });

                    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(pool), N(active)},
                       { N(pool), obj->recipient, convert_vesting_to_token(obj->payout_part), std::string("payment vesting") } );
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
                account.modify(balance, _self, [&](auto &item){
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

void vesting::add_balance(account_name owner, asset value, account_name ram_payer) {
    tables::account_table account(_self, owner);
    auto to = account.find( value.symbol.name() );
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

asset vesting::convert_token_to_vesting(const asset &m_token) {
    asset vest = NULL_TOKEN_VESTING;
    vest.amount = m_token.amount;
    return vest;
}

asset vesting::convert_vesting_to_token(const asset &m_token) {
    asset golos = NULL_TOKEN_GOLOS;
    golos.amount = m_token.amount;
    return golos;
}

void vesting::timeout_delay_trx() {
    require_auth(_self);

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutrdel), structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutconv), structures::shash{now()}});
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeout),     structures::shash{now()}});
    trx.delay_sec = TIMEOUT;
    trx.send(_self, _self, true);
}
