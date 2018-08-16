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
        calculate_convert_vesting();
    else if (N(timeoutconv) == action)
        calculate_delegate_vesting();
}

void vesting::buy_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    require_auth(m_transfer_vesting.sender);

    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {m_transfer_vesting.sender, N(active)},
       { m_transfer_vesting.sender, N(eosio.null), m_transfer_vesting.quantity, std::string("buy vesting") } );

    // TODO расчёт вестинга по коэффициенту
    add_balance(m_transfer_vesting.recipient, convert_token_to_vesting(m_transfer_vesting.quantity), m_transfer_vesting.sender);
    // TODO изменить таблицу статистики

    tables::vesting_table vest(_self, TOKEN_VESTING.name());
    auto existing = vest.find(TOKEN_VESTING.name());
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
    tables::account_table account(_self, m_delegate_vesting.sender);
    auto vest = account.find(m_delegate_vesting.quantity.symbol.name());
    eosio_assert(vest != account.end(), "Not a token");
    eosio_assert(vest->vesting >= m_delegate_vesting.quantity, "Not enough vesting");

    tables::delegate_table table(_self, m_delegate_vesting.sender);
    auto it_recipient = table.find(m_delegate_vesting.recipient);
    if (it_recipient != table.end()) {
        table.modify(it_recipient, m_delegate_vesting.sender, [&](structures::delegate_record &item){
            item.quantity += m_delegate_vesting.quantity;
            item.percentage_deductions = m_delegate_vesting.percentage_deductions;
            item.return_date = time_point_sec(OUTPUT_PAYOUT_PERIOD);
        });
    } else {
        table.emplace(m_delegate_vesting.sender, [&](structures::delegate_record &item){
            item.recipient = m_delegate_vesting.recipient;
            item.quantity = m_delegate_vesting.quantity;
            item.percentage_deductions = m_delegate_vesting.percentage_deductions;
            item.return_date = time_point_sec(OUTPUT_PAYOUT_PERIOD);
        });
    }
}

void vesting::undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting) {
    bool flag_valid_vesting_token = false;
    tables::delegate_table table(_self, m_transfer_vesting.sender);
    auto index_recipient = table.template get_index<N(recipient)>();
    auto it_recipient = index_recipient.find(m_transfer_vesting.recipient);
    while (it_recipient != index_recipient.end() && it_recipient->recipient == m_transfer_vesting.recipient) {
        if (it_recipient->quantity.symbol.name() != m_transfer_vesting.quantity.symbol.name()) {
            ++it_recipient;
            continue;
        }

        eosio_assert(it_recipient->quantity >= m_transfer_vesting.quantity, "Not enough delegated vesting");
        flag_valid_vesting_token = true;

        if (it_recipient->quantity == m_transfer_vesting.quantity) {
            index_recipient.erase(it_recipient);
        } else {
            index_recipient.modify(it_recipient, m_transfer_vesting.sender, [&](auto &item){
                item.quantity -= m_transfer_vesting.quantity;
            });
        }

        break;
    }

    eosio_assert(!flag_valid_vesting_token, "Not valid vesting token");

    _table_delegate_vesting.emplace(m_transfer_vesting.sender, [&](structures::return_delegate &item){
        item.id = _table_delegate_vesting.available_primary_key();
        item.recipient = m_transfer_vesting.recipient;
        item.amount = m_transfer_vesting.quantity;
        item.date = time_point_sec(OUTPUT_PAYOUT_PERIOD);
    });
}

void vesting::init_contract() {
    require_auth(_self);
    timeout_delay_trx();
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);
}

void vesting::calculate_delegate_vesting() {
    require_auth(_self);
}

void vesting::add_balance(account_name owner, asset value, account_name ram_payer) {
    tables::account_table account(_self, owner);
    auto to = account.find( value.symbol.name() );
    if( to == account.end() ) {
        account.emplace( ram_payer, [&]( auto& a ){
            a.vesting = value;
        });
    } else {
        account.modify( to, 0, [&]( auto& a ) {
            a.vesting += value;
        });
    }
}

void vesting::sub_balance(account_name owner, asset value) {
    tables::account_table account(_self, owner);

    const auto& from = account.get( value.symbol.name(), "no balance object found" );
    eosio_assert( from.vesting.amount >= value.amount, "overdrawn balance" );

    if( from.vesting.amount == value.amount ) {
       account.erase( from );
    } else {
       account.modify( from, owner, [&]( auto& a ) {
           a.vesting -= value;
       });
    }
}

asset vesting::convert_token_to_vesting(const asset &m_token) {
    asset vest;
    vest.symbol = TOKEN_VESTING;
    vest.amount = m_token.amount;
    return vest;
}

void vesting::timeout_delay_trx() {
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutrdel), {now()} });
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(timeoutconv), {now()} });
    trx.delay_sec = TIMEOUT;
    trx.send(now(), _self, true);
}
