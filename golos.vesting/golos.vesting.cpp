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
        execute_action(this, &vesting::on_transfer);

    else if (N(retire) == action)
        execute_action(this, &vesting::retire);
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

    else if (N(createvest) == action)
        execute_action(this, &vesting::create);
    else if (N(timeoutrdel) == action)
        calculate_delegate_vesting();
    else if (N(timeoutconv) == action)
        calculate_convert_vesting();
    else if (N(timeout) == action)
        timeout_delay_trx();
}

void vesting::on_transfer(account_name from, account_name  to, asset quantity, std::string memo) {
    if(_self != from) // TODO account golos.vesting can not buy vesting
        require_auth(from);
    else return;

    eosio_assert(from != to, "cannot transfer to self");
    eosio_assert(is_account(to), "to account does not exist");
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");

    tables::vesting_table table_vesting(_self, _self);
    auto vesting = table_vesting.find(quantity.symbol.name());
    eosio_assert(vesting != table_vesting.end(), "Token not found");

    bool from_issuer = std::find(vesting->issuers.begin(), vesting->issuers.end(), from) != vesting->issuers.end();

    asset converted(0, quantity.symbol);

    if(!from_issuer || !memo.empty())
    {
        converted = convert_to_vesting(quantity, *vesting);
        table_vesting.modify(vesting, 0, [&](auto& item) { item.supply += converted; });
        require_recipient(from);
        auto payer = has_auth(to) ? to : from;
        add_balance(from, converted, payer);
    }

    if(from_issuer && !memo.empty())
        accrue_vesting(from, string_to_name(memo.c_str()), converted);
}

void vesting::retire(account_name issuer, asset quantity, account_name user) {
    require_auth(issuer);
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must retire positive quantity");
    
    tables::vesting_table table_vesting(_self, _self);
    auto vesting = table_vesting.find(quantity.symbol.name());
    eosio_assert(vesting != table_vesting.end(), "Vesting not found");
    eosio_assert(quantity.symbol == vesting->supply.symbol, "symbol precision mismatch" );
    bool from_issuer = std::find(vesting->issuers.begin(), vesting->issuers.end(), issuer) != vesting->issuers.end();
    eosio_assert(from_issuer, "issuer mismatch");
    eosio_assert(quantity.amount <= vesting->supply.amount, "invalid amount");
    
    sub_balance(user, quantity);
    table_vesting.modify(vesting, 0, [&](auto &item) {
        item.supply -= quantity;
    });
}

void vesting::accrue_vesting(account_name sender, account_name user, asset quantity) {
    require_auth(sender);
    require_recipient(user);

    auto sym_name = quantity.symbol.name();

    tables::account_table account(_self, user);
    auto balance = account.find(sym_name);
    eosio_assert(balance != account.end(), "Not found balance account");

    asset summary_delegate;
    summary_delegate.symbol = quantity.symbol;

    tables::delegate_table table_delegate(_self, sym_name);
    auto index_delegate = table_delegate.get_index<N(recipient)>();
    auto it_index_user = index_delegate.find(user);

    asset efective_vesting = balance->vesting + balance->received_vesting;
    while (it_index_user != index_delegate.end() && it_index_user->recipient == user && bool_asset(efective_vesting)) {
        auto proportion = it_index_user->quantity * FRACTION / efective_vesting;
        const auto &delegates_award = (((quantity * proportion) / FRACTION) * it_index_user->interest_rate) / FRACTION;

        index_delegate.modify(it_index_user, 0, [&](auto &item) {
            item.deductions += delegates_award;
        });

        summary_delegate += delegates_award;
        ++it_index_user;
    }

    sub_balance(sender, quantity);
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

void vesting::delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t interest_rate, uint8_t payout_strategy) {
    require_auth(sender);
    eosio_assert(sender != recipient, "You can not delegate to yourself");

    eosio_assert(quantity.amount >= 0, "the number of tokens should not be less than 0");
    eosio_assert(quantity.amount >= MIN_AMOUNT_DELEGATION_VESTING, "Insufficient funds for delegation");
    eosio_assert(interest_rate <= MAX_PERSENT_DELEGATION, "Exceeded the percentage of delegated vesting");

    tables::account_table account_sender(_self, sender);
    auto balance_sender = account_sender.find(quantity.symbol.name());
    eosio_assert(balance_sender != account_sender.end(), "Not found token");

    account_sender.modify(balance_sender, sender, [&](auto &item){
        item.delegate_vesting += quantity;
    });

    tables::delegate_table table(_self, quantity.symbol.name());
    auto index_table = table.get_index<N(unique)>();
    auto delegate_record = index_table.find(structures::delegate_record::unique_key(sender, recipient));
    if (delegate_record != index_table.end()) {
        index_table.modify(delegate_record, 0, [&](auto &item){
            item.quantity += quantity;
        });
    } else {
        table.emplace(sender, [&](structures::delegate_record &item){
            item.id = table.available_primary_key();
            item.sender = sender;
            item.recipient = recipient;
            item.quantity = quantity;
            item.deductions.symbol = quantity.symbol;
            item.interest_rate = interest_rate;
            item.payout_strategy = payout_strategy; // TODO 0 - to_delegator, 1 - to_delegated_vestings
            item.return_date = time_point_sec(now() + TIME_LIMIT_FOR_RETURN_DELEGATE_VESTING);
        });
    }

    tables::account_table account_recipient(_self, recipient);
    auto balance_recipient = account_recipient.find(quantity.symbol.name());
    eosio_assert(balance_recipient != account_recipient.end(), "Not found balance token vesting");
    account_recipient.modify(balance_recipient, sender, [&](auto &item){
        item.received_vesting += quantity;
    });

    eosio_assert(balance_recipient->received_vesting.amount >= MIN_DELEGATION, "delegated vesting withdrawn");
}

void vesting::undelegate_vesting(account_name sender, account_name recipient, asset quantity) {
    require_auth(sender);

    tables::delegate_table table(_self, quantity.symbol.name());
    auto index_table = table.get_index<N(unique)>();
    auto delegate_record = index_table.find(structures::delegate_record::unique_key(sender, recipient));
    eosio_assert(delegate_record != index_table.end(), "Not enough delegated vesting");

    eosio_assert(quantity.amount >= MIN_AMOUNT_DELEGATION_VESTING, "Insufficient funds for undelegation");
    eosio_assert(delegate_record->return_date <= time_point_sec(now()), "Tokens are frozen until the end of the period");
    eosio_assert(delegate_record->quantity >= quantity, "There are not enough delegated tools for output");

    if (delegate_record->quantity == quantity) {
        index_table.erase(delegate_record);
    } else {
        index_table.modify(delegate_record, sender, [&](auto &item){
            item.quantity -= quantity;
        });
    }

    tables::return_delegate_table table_delegate_vesting(_self, _self);
    table_delegate_vesting.emplace(sender, [&](structures::return_delegate &item){
        item.id = table_delegate_vesting.available_primary_key();
        item.recipient = sender;
        item.amount = quantity;
        item.date = time_point_sec(now() + TIME_LIMIT_FOR_RETURN_DELEGATE_VESTING);
    });

    tables::account_table account_recipient(_self, recipient);
    auto balance = account_recipient.find(quantity.symbol.name());
    eosio_assert(balance != account_recipient.end(), "This token is not on the recipient balance sheet");
    account_recipient.modify(balance, sender, [&](auto &item){
        item.received_vesting -= quantity;
    });

    eosio_assert(balance->received_vesting.amount >= MIN_DELEGATION, "delegated vesting withdrawn");
}

void vesting::create(account_name creator, symbol_type symbol, std::vector<account_name> issuers) {
    require_auth(creator);
    eosio_assert(creator == token(N(eosio.token)).get_issuer(symbol.name()), "Only token issuer can create it");

    tables::vesting_table table_vesting(_self, _self);
    auto vesting = table_vesting.find(symbol.name());
    eosio_assert(vesting == table_vesting.end(), "Vesting already exists");

    table_vesting.emplace(_self, [&](auto &item){
        item.supply = asset(0, symbol);
        item.issuers = issuers;
    });
}

void vesting::calculate_convert_vesting() {
    require_auth(_self);
    tables::vesting_table table_vesting(_self, _self);
    for(auto vesting : table_vesting) {
        tables::convert_table table(_self, vesting.supply.symbol.name());
        auto index = table.get_index<N(payouttime)>();
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

                    auto lambda_action_sender = [&](asset quantity) {
                        sub_balance(obj->sender, quantity);

                        auto vest = table_vesting.find(quantity.symbol.name());
                        eosio_assert(vest != table_vesting.end(), "Vesting not found");

                        table_vesting.modify(vest, 0, [&](auto &item) {
                            item.supply -= quantity;
                        });

                        INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {_self, N(active)},
                        { _self, obj->recipient, convert_to_token(quantity, *vest), "Translation into tokens" } );
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
    for (auto obj = index.cbegin(); obj != index.cend();) {
        if (obj->date > time_point_sec(now()))
            break;

        tables::account_table account_recipient(_self, obj->recipient);
        auto balance_recipient = account_recipient.find(obj->amount.symbol.name());
        eosio_assert(balance_recipient != account_recipient.end(), "This token is not on the sender balance sheet");
        account_recipient.modify(balance_recipient, 0, [&](auto &item){
            item.delegate_vesting -= obj->amount;
        });

        obj = index.erase(obj);
    }
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

void vesting::notify_balance_change(account_name owner, asset diff) {
    action(
        permission_level{_self, N(active)},
        CTRL_CONTRACT,
        N(changevest),
        std::make_tuple(diff.symbol, owner, diff)   // TODO: asset is enough, but ctrl uses symbol (1st arg) to detect app
    ).send();
}

void vesting::sub_balance(account_name owner, asset value) {
    eosio_assert(value.amount >= 0, "sub_balance: value.amount < 0");
    tables::account_table account(_self, owner);
    const auto& from = account.get(value.symbol.name(), "no balance object found");
    eosio_assert(from.available_vesting() >= value, "overdrawn balance");

    account.modify(from, 0, [&](auto& a) {
        a.vesting -= value;
    });
    notify_balance_change(owner, -value);
}

void vesting::add_balance(account_name owner, asset value, account_name ram_payer) {
    eosio_assert(value.amount >= 0, "add_balance: value.amount < 0");
    tables::account_table account(_self, owner);
    auto to = account.find(value.symbol.name());
//    eosio_assert(to != account.end(), "Not found balance token vesting");
    if(to == account.end()) {
        account.emplace(ram_payer, [&](auto& a) {
            a.vesting = value;
            a.delegate_vesting.symbol = value.symbol;
            a.received_vesting.symbol = value.symbol;
        });
    } else {
        account.modify(to, 0, [&](auto& a) {
            a.vesting += value;
        });
    }
    notify_balance_change(owner, value);
}

const bool vesting::bool_asset(const asset &obj) const {
    return obj.amount;
}

const asset vesting::convert_to_token(const asset &m_token, const structures::vesting_info & vinfo) const {
    uint64_t amount;
    symbol_type symbol = m_token.symbol;

    auto this_balance = token(N(eosio.token)).get_balance(_self, symbol.name());
    eosio_assert(symbol == this_balance.symbol, "The token type does not match.");

    if (!vinfo.supply.amount || !this_balance.amount)
        amount = m_token.amount;
    else {
        amount = (m_token.amount * this_balance.amount) / (vinfo.supply.amount + m_token.amount);
    }

    return asset(amount, symbol);
}

const asset vesting::convert_to_vesting(const asset &m_token, const structures::vesting_info & vinfo) const {
    uint64_t amount;
    symbol_type symbol = m_token.symbol;

    auto this_balance = token(N(eosio.token)).get_balance(_self, symbol.name()) - m_token;
    eosio_assert(m_token.symbol == vinfo.supply.symbol, "The token type does not match.");

    if (!vinfo.supply.amount || !this_balance.amount)
        amount = m_token.amount;
    else {
        amount = static_cast<int64_t>((static_cast<uint128_t>(m_token.amount) * static_cast<uint128_t>(vinfo.supply.amount)) / static_cast<uint128_t>(this_balance.amount));
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
