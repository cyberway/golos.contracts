#include "golos.publication.hpp"
#include <eosiolib/transaction.hpp> 
#include <eosiolib/event.hpp>
#include <golos.social/golos.social.hpp>
#include <golos.vesting/golos.vesting.hpp>
#include <golos.referral/golos.referral.hpp>
#include <golos.charge/golos.charge.hpp>
#include <common/upsert.hpp>
#include "utils.hpp"
#include "objects.hpp"
#include <cyberway.contracts/common/util.hpp>

namespace golos {

using namespace atmsp::storable;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        //publication(receiver).apply(code, action);
        auto execute_action = [&](const auto fn) {
            return eosio::execute_action(eosio::name(receiver), eosio::name(code), fn);
        };

#define NN(x) N(x).value

        if (NN(transfer) == action && config::token_name.value == code)
            execute_action(&publication::on_transfer);

        if (NN(createmssg) == action)
            execute_action(&publication::create_message);
        if (NN(updatemssg) == action)
            execute_action(&publication::update_message);
        if (NN(deletemssg) == action)
            execute_action(&publication::delete_message);
        if (NN(upvote) == action)
            execute_action(&publication::upvote);
        if (NN(downvote) == action)
            execute_action(&publication::downvote);
        if (NN(unvote) == action)
            execute_action(&publication::unvote);
        if (NN(closemssg) == action)
            execute_action(&publication::close_message);
        if (NN(setrules) == action)
            execute_action(&publication::set_rules);
        if (NN(setlimit) == action)
            execute_action(&publication::set_limit);
        if (NN(setparams) == action)
            execute_action(&publication::set_params);
        if (NN(reblog) == action)
            execute_action(&publication::reblog);
        if (NN(erasereblog) == action)
            execute_action(&publication::erase_reblog);
        if (NN(setcurprcnt) == action)
            execute_action(&publication::set_curators_prcnt);
        if (NN(calcrwrdwt) == action)
            execute_action(&publication::calcrwrdwt);
        if (NN(paymssgrwrd) == action)
            execute_action(&publication::paymssgrwrd);
        if (NN(setmaxpayout) == action)
            execute_action(&publication::set_max_payout);
    }
#undef NN
}

struct posting_params_setter: set_params_visitor<posting_state> {
    using set_params_visitor::set_params_visitor;

    bool operator()(const max_vote_changes_prm& param) {
        return set_param(param, &posting_state::max_vote_changes_param);
    }

    bool operator()(const cashout_window_prm& param) {
        return set_param(param, &posting_state::cashout_window_param);
    }

    bool operator()(const max_beneficiaries_prm& param) {
        return set_param(param, &posting_state::max_beneficiaries_param);
    }

    bool operator()(const max_comment_depth_prm& param) {
        return set_param(param, &posting_state::max_comment_depth_param);
    }

    bool operator()(const social_acc_prm& param) {
        return set_param(param, &posting_state::social_acc_param);
    }

    bool operator()(const referral_acc_prm& param) {
        return set_param(param, &posting_state::referral_acc_param);
    }
    bool operator()(const curators_prcnt_prm& param) {
        return set_param(param, &posting_state::curators_prcnt_param);
    }
};

// cached to prevent unneeded db access
const posting_state& publication::params() {
    static const auto cfg = posting_params_singleton(_self, _self.value).get();
    return cfg;
}

void publication::create_message(
    structures::mssgid message_id,
    structures::mssgid parent_id,
    std::vector<structures::beneficiary> beneficiaries,
    uint16_t tokenprop,
    bool vestpayment,
    std::string headermssg,
    std::string bodymssg,
    std::string languagemssg,
    std::vector<std::string> tags,
    std::string jsonmetadata,
    std::optional<uint16_t> curators_prcnt = std::nullopt,
    std::optional<asset> max_payout = std::nullopt
) {
    require_auth(message_id.author);

    eosio_assert(message_id.permlink.length() && message_id.permlink.length() < config::max_length, 
            "Permlink length is empty or more than 256.");
    eosio_assert(validate_permlink(message_id.permlink), "Permlink contains wrong symbol.");
    eosio_assert(headermssg.length() < config::max_length, "Title length is more than 256.");
    eosio_assert(bodymssg.length(), "Body is empty.");
    validate_percent(tokenprop, "tokenprop");

    const auto& cashout_window_param = params().cashout_window_param;
    const auto& max_beneficiaries_param = params().max_beneficiaries_param;
    const auto& max_comment_depth_param = params().max_comment_depth_param;
    const auto& social_acc_param = params().social_acc_param;
    const auto& referral_acc_param = params().referral_acc_param;
    const auto& curators_prcnt_param = params().curators_prcnt_param;
    if (curators_prcnt) {
        curators_prcnt_param.validate_value(*curators_prcnt);
    } else {
        curators_prcnt = curators_prcnt_param.min_curators_prcnt;
    }

    if (parent_id.author) {
        if (social_acc_param.account) {
            eosio_assert(!social::is_blocking(social_acc_param.account, parent_id.author, message_id.author), 
                    "You are blocked by this account");
        }
    }

    tables::reward_pools pools(_self, _self.value);
    auto pool = pools.begin();   // TODO: Reverse iterators doesn't work correctly
    eosio_assert(pool != pools.end(), "publication::create_message: [pools] is empty");
    pool = --pools.end();
    check_acc_vest_balance(message_id.author, pool->state.funds.symbol);
    auto token_code = pool->state.funds.symbol.code();
    auto issuer = token::get_issuer(config::token_name, token_code);
    tables::limit_table lims(_self, _self.value);

    if (!!max_payout) {
        eosio::check(max_payout >= asset(0, pool->state.funds.symbol), "max_payout must not be negative.");
    } else {
        max_payout = asset(asset::max_amount, pool->state.funds.symbol);
    }

    use_charge(
            lims, 
            parent_id.author ? structures::limitparams::COMM : structures::limitparams::POST, 
            issuer, message_id.author,
            golos::vesting::get_account_effective_vesting(config::vesting_name, message_id.author, token_code).amount, 
            token_code, 
            vestpayment);

    tables::permlink_table permlink_table(_self, message_id.author.value);
    tables::message_table message_table(_self, message_id.author.value);
    uint64_t message_pk = permlink_table.available_primary_key();
    if (message_pk == 0)
        message_pk = 1;
    if (!parent_id.author)
        use_postbw_charge(lims, issuer, message_id.author, token_code, message_pk);

    std::map<name, uint16_t> benefic_map;
    uint32_t weights_sum = 0;
    for (auto& ben : beneficiaries) {
        validate_percent_not0(ben.weight, "beneficiary.weight");            // TODO: tests #617
        eosio::check(benefic_map.count(ben.account) == 0, "beneficiaries contain several entries for one account");
        check_acc_vest_balance(ben.account, pool->state.funds.symbol);
        weights_sum += ben.weight;
        validate_percent_le(weights_sum, "weights_sum of beneficiaries");   // TODO: tests #617
        benefic_map[ben.account] = ben.weight;
    }

    if (referral_acc_param.account != name()) {
        auto obj_referral = golos::referral::account_referrer(referral_acc_param.account, message_id.author);
        if (!obj_referral.is_empty()) {
            auto referrer = obj_referral.referrer;
            eosio::check(benefic_map.count(referrer) == 0, "Comment already has referrer as a referrer-beneficiary.");
            weights_sum += obj_referral.percent;
            validate_percent_le(weights_sum, "weights_sum + referral percent"); // TODO: tests #617
            benefic_map[referrer] = obj_referral.percent;
        }
    }

    //reusing a vector
    beneficiaries.reserve(benefic_map.size());
    beneficiaries.clear();
    for (const auto& ben : benefic_map)
        beneficiaries.emplace_back(structures::beneficiary{
            .account = ben.first,
            .weight = ben.second
        });

    eosio_assert((benefic_map.size() <= max_beneficiaries_param.max_beneficiaries), 
            "publication::create_message: benafic_map.size() > MAX_BENEFICIARIES");

    auto cur_time = current_time();
    atmsp::machine<fixp_t> machine;

    eosio_assert(cur_time >= pool->created, "publication::create_message: cur_time < pool.created");
    eosio_assert(pool->state.msgs < std::numeric_limits<structures::counter_t>::max(), 
            "publication::create_message: pool->msgs == max_counter_val");
    pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });

    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr == permlink_index.end(), "This message already exists.");

    uint64_t parent_pk = 0;
    uint16_t level = 0;
    if(parent_id.author) {
        tables::permlink_table parent_table(_self, parent_id.author.value);
        auto parent_index = parent_table.get_index<"byvalue"_n>();
        auto parent_itr = parent_index.find(parent_id.permlink);
        eosio::check((parent_itr != parent_index.end()), "Parent message doesn't exist");

        parent_table.modify(*parent_itr, name(), [&](auto& item) {
            ++item.childcount;
        });

        parent_pk = parent_itr->id;
        level = 1 + parent_itr->level;
    }
    eosio::check(level <= max_comment_depth_param.max_comment_depth, "publication::create_message: level > MAX_COMMENT_DEPTH");
    eosio::check(tokenprop <= pool->rules.maxtokenprop, "tokenprop must not be greater than pool.rules.maxtokenprop");

    message_table.emplace(message_id.author, [&]( auto &item ) {
        item.id = message_pk;
        item.date = cur_time;
        item.pool_date = pool->created;
        item.tokenprop = tokenprop,
        item.beneficiaries = beneficiaries;
        item.rewardweight = config::_100percent;    // can be corrected later in calcrwrdwt
        item.curators_prcnt = *curators_prcnt;
        item.mssg_reward = asset(0, pool->state.funds.symbol);
        item.max_payout = *max_payout;
    });

    permlink_table.emplace(message_id.author, [&]( auto &item) {
        item.id = message_pk;
        item.parentacc = parent_id.author;
        item.parent_id = parent_pk;
        item.value = message_id.permlink;
        item.level = level;
        item.childcount = 0;
    });

    close_message_timer(message_id, message_pk, cashout_window_param.window);
}

void publication::update_message(structures::mssgid message_id,
                              std::string headermssg, std::string bodymssg,
                              std::string languagemssg, std::vector<std::string> tags,
                              std::string jsonmetadata) {
    require_auth(message_id.author);
    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(),
                 "You can't update this message, because this message doesn't exist.");
}

auto publication::get_pool(tables::reward_pools& pools, uint64_t time) {
    eosio_assert(pools.begin() != pools.end(), "publication::get_pool: [pools] is empty");

    auto pool = pools.find(time);

    eosio_assert(pool != pools.end(), "publication::get_pool: can't find an appropriate pool");
    return pool;
}

void publication::delete_message(structures::mssgid message_id) {
    require_auth(message_id.author);

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");
    eosio::check(permlink_itr->childcount == 0, "You can't delete comment with child comments.");

    tables::message_table message_table(_self, message_id.author.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    if (mssg_itr != message_table.end()) {
        eosio::check(FP(mssg_itr->state.netshares) <= 0, "Cannot delete a comment with net positive votes.");
        eosio_assert((!mssg_itr->closed || (mssg_itr->mssg_reward.amount == 0)), "Cannot delete comment with unpaid payout");
    }

    if (permlink_itr->parentacc) {
        tables::permlink_table parent_table(_self, permlink_itr->parentacc.value);
        auto parent_itr = parent_table.find(permlink_itr->parent_id);
        eosio::check(parent_itr != parent_table.end(), "Parent permlink doesn't exist.");

        parent_table.modify(*parent_itr, name(), [&](auto& item) {
            --item.childcount;
        });
    }
    permlink_index.erase(permlink_itr);

    if (mssg_itr == message_table.end()) {
        return;
    }

    cancel_deferred((static_cast<uint128_t>(mssg_itr->id) << 64) | message_id.author.value);

    auto remove_id = mssg_itr->id;
    message_table.erase(mssg_itr);

    tables::vote_table vote_table(_self, message_id.author.value);
    auto votetable_index = vote_table.get_index<"messageid"_n>();
    auto vote_itr = votetable_index.lower_bound(std::make_tuple(remove_id, INT64_MAX));

    while ((vote_itr != votetable_index.end()) && (vote_itr->message_id == remove_id)) {
        auto& vote = *vote_itr;
        ++vote_itr;
        vote_table.erase(vote);
    }
}

void publication::upvote(name voter, structures::mssgid message_id, uint16_t weight) {
    eosio_assert(weight > 0, "weight can't be 0.");
    eosio_assert(weight <= config::_100percent, "weight can't be more than 100%.");
    set_vote(voter, message_id, weight);
}

void publication::downvote(name voter, structures::mssgid message_id, uint16_t weight) {
    eosio_assert(weight > 0, "weight can't be 0.");
    eosio_assert(weight <= config::_100percent, "weight can't be more than 100%.");
    set_vote(voter, message_id, -weight);
}

void publication::unvote(name voter, structures::mssgid message_id) {
    set_vote(voter, message_id, 0);
}

void publication::payto(name user, eosio::asset quantity, enum_t mode, std::string memo) {
    require_auth(_self);
    eosio_assert(quantity.amount >= 0, "LOGIC ERROR! publication::payto: quantity.amount < 0");
    if(quantity.amount == 0)
        return;

    if(static_cast<payment_t>(mode) == payment_t::TOKEN) {
        if (token::balance_exist(config::token_name, user, quantity.symbol.code())) {
            INLINE_ACTION_SENDER(token, payment) (config::token_name, {_self, config::code_name}, {_self, user, quantity, memo});
        } 
        else {
            INLINE_ACTION_SENDER(token, payment) (config::token_name, {_self, config::code_name}, {_self, _self, quantity, memo});
        }
    }
    else if(static_cast<payment_t>(mode) == payment_t::VESTING) {
        if (golos::vesting::balance_exist(config::vesting_name, user, quantity.symbol.code())) {
            INLINE_ACTION_SENDER(token, transfer) (config::token_name, {_self, config::code_name},
            {_self, config::vesting_name, quantity, std::string(config::send_prefix + name{user}.to_string()  + "; " + memo)});
        } 
        else {
            INLINE_ACTION_SENDER(token, payment) (config::token_name, {_self, config::code_name}, {_self, _self, quantity, memo});
        }
    }
    else
        eosio_assert(false, "publication::payto: unknown kind of payment");
}

int64_t publication::pay_curators(
        structures::mssgid message_id, 
        uint64_t msgid, 
        int64_t max_rewards, 
        fixp_t weights_sum, 
        symbol tokensymbol, 
        std::string memo) 
{
    tables::vote_table vs(_self, message_id.author.value);
    int64_t unclaimed_rewards = max_rewards;

    if ((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
        auto idx = vs.get_index<"messageid"_n>();
        auto v = idx.lower_bound(std::make_tuple(msgid, INT64_MAX));
        while ((v != idx.end()) && (v->message_id == msgid)) {
            auto claim = int_cast(elai_t(max_rewards) * elaf_t(elap_t(FP(v->curatorsw)) / elap_t(weights_sum)));
            eosio_assert(claim <= unclaimed_rewards, "LOGIC ERROR! publication::pay_curators: claim > unclaimed_rewards");
            if (claim == 0) {
                break;
            }
            unclaimed_rewards -= claim;
            claim -= pay_delegators(claim, v->voter, tokensymbol, v->delegators, message_id);
            payto(v->voter, eosio::asset(claim, tokensymbol), static_cast<enum_t>(payment_t::VESTING), memo);
            //v = idx.erase(v);
            ++v;
        }
    }
    return unclaimed_rewards;
}

void publication::use_charge(tables::limit_table& lims, structures::limitparams::act_t act, name issuer,
                            name account, int64_t eff_vesting, symbol_code token_code, bool vestpayment, elaf_t weight) {
    auto lim_itr = lims.find(act);
    eosio_assert(lim_itr != lims.end(), "publication::use_charge: limit parameters not set");
    eosio_assert(eff_vesting >= lim_itr->min_vesting, "insufficient effective vesting amount");
    if(lim_itr->price >= 0)
        INLINE_ACTION_SENDER(charge, use) (config::charge_name,
            {issuer, config::invoice_name}, {
                account,
                token_code,
                lim_itr->charge_id,
                int_cast(elai_t(lim_itr->price) * weight),
                lim_itr->cutoff,
                vestpayment ? int_cast(elai_t(lim_itr->vesting_price) * weight) : 0
            });
}

void publication::use_postbw_charge(
        tables::limit_table& lims, 
        name issuer, name account, 
        symbol_code token_code, 
        int64_t mssg_id) 
{
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    if(bw_lim_itr->price >= 0)
        INLINE_ACTION_SENDER(charge, usenotifygt) (config::charge_name,
            {issuer, config::invoice_name}, {
                account,
                token_code,
                bw_lim_itr->charge_id,
                bw_lim_itr->price,
                mssg_id,
                _self,
                "calcrwrdwt"_n,
                bw_lim_itr->cutoff
            });
}

void publication::close_message(structures::mssgid message_id) {
    require_auth(_self);
    
    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    // rare case - not critical for performance
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    tables::message_table message_table(_self, message_id.author.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    eosio::check(mssg_itr != message_table.end(), "Message doesn't exist in cashout window.");
    eosio::check(!mssg_itr->closed, "Message is closed.");

    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->pool_date);

    eosio_assert(pool->state.msgs != 0, "LOGIC ERROR! publication::payrewards: pool.msgs is equal to zero");
    atmsp::machine<fixp_t> machine;
    fixp_t sharesfn = set_and_run(
            machine, 
            pool->rules.mainfunc.code, 
            {FP(mssg_itr->state.netshares)}, 
            {{fixp_t(0), FP(pool->rules.mainfunc.maxarg)}});

    asset mssg_reward;
    auto state = pool->state;
    int64_t payout = 0;
    if(state.msgs == 1) {
        payout = state.funds.amount; //if we have the only message in the pool, the author receives a reward anyway
        eosio_assert(state.rshares == mssg_itr->state.netshares, 
                "LOGIC ERROR! publication::payrewards: pool->rshares != mssg_itr->netshares for last message");
        eosio_assert(state.rsharesfn == sharesfn.data(), 
                "LOGIC ERROR! publication::payrewards: pool->rsharesfn != sharesfn.data() for last message");
        state.funds.amount = 0;
        state.rshares = 0;
        state.rsharesfn = 0;
    }
    else {
        auto total_rsharesfn = WP(state.rsharesfn);

        if(sharesfn > fixp_t(0)) {
            eosio_assert(total_rsharesfn > 0, "LOGIC ERROR! publication::payrewards: total_rshares_fn <= 0");

            auto numer = sharesfn;
            auto denom = total_rsharesfn;
            narrow_down(numer, denom);

            elaf_t reward_weight(elai_t(mssg_itr->rewardweight) / elai_t(config::_100percent));
            payout = int_cast(
                reward_weight * elai_t(elai_t(state.funds.amount) * elaf_t(elap_t(numer) / elap_t(denom))));

            eosio::check(mssg_itr->max_payout.symbol == state.funds.symbol, "LOGIC ERROR! publication::payrewards: mssg_itr->max_payout.symbol != state.funds.symbol");
            payout = std::min(payout, mssg_itr->max_payout.amount);

            state.funds.amount -= payout;
            eosio::check(state.funds.amount >= 0, "LOGIC ERROR! publication::payrewards: state.funds < 0");
        }

        auto rshares = wdfp_t(FP(mssg_itr->state.netshares));
        auto new_rshares = WP(state.rshares) - rshares;

        auto rsharesfn = wdfp_t(sharesfn);
        auto new_rsharesfn = WP(state.rsharesfn) - rsharesfn;

        eosio_assert(new_rshares >= 0, "LOGIC ERROR! publication::payrewards: new_rshares < 0");
        eosio_assert(new_rsharesfn >= 0, "LOGIC ERROR! publication::payrewards: new_rsharesfn < 0");

        state.rshares = new_rshares.data();
        state.rsharesfn = new_rsharesfn.data();
    }

    mssg_reward = asset(payout, state.funds.symbol); 

    message_table.modify(mssg_itr, name(), [&]( auto &item ) {
            item.closed = true;
            item.mssg_reward = mssg_reward;
        });

    bool pool_erased = false;
    state.msgs--;
    if(state.msgs == 0) {
        if(pool != --pools.end()) {
            send_poolerase_event(*pool);

            pools.erase(pool);
            pool_erased = true;
        }
    }
    if(!pool_erased)
        pools.modify(pool, _self, [&](auto &item) {
            item.state = state;
            send_poolstate_event(item);
        });

    transaction trx(time_point_sec(now() + config::paymssgrwrd_expiration_sec));
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "paymssgrwrd"_n, message_id});
    trx.delay_sec = 0;
    trx.send((static_cast<uint128_t>(mssg_itr->id) << 64) | message_id.author.value, _self);
}

void publication::paymssgrwrd(structures::mssgid message_id) {
    require_auth(_self);

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    // rare case - not critical for performance
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    tables::message_table message_table(_self, message_id.author.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    eosio::check(mssg_itr != message_table.end(), "Message doesn't exist in cashout window.");
    eosio::check(mssg_itr->closed, "Message doesn't closed.");

    auto payout = mssg_itr->mssg_reward;
    tables::reward_pools pools(_self, _self.value);

    elaf_t percent(elai_t(mssg_itr->curators_prcnt) / elai_t(config::_100percent));
    auto curation_payout = int_cast(percent * elai_t(payout.amount));

    eosio_assert((curation_payout <= payout.amount) && (curation_payout >= 0), 
            "publication::payrewards: wrong curation_payout");
    auto unclaimed_rewards = pay_curators(
            message_id, 
            mssg_itr->id, 
            curation_payout, 
            FP(mssg_itr->state.sumcuratorsw), 
            payout.symbol, 
            get_memo("curators", message_id));

    eosio_assert(unclaimed_rewards >= 0, "publication::payrewards: unclaimed_rewards < 0");
    
    payout.amount -= curation_payout;
    
    int64_t ben_payout_sum = 0;
    for (auto& ben: mssg_itr->beneficiaries) {
        auto ben_payout = cyber::safe_pct(payout.amount, ben.weight);
        eosio_assert((0 <= ben_payout) && (ben_payout <= payout.amount - ben_payout_sum), 
                "LOGIC ERROR! publication::payrewards: wrong ben_payout value");
        payto(
                ben.account, 
                eosio::asset(ben_payout, payout.symbol), 
                static_cast<enum_t>(payment_t::VESTING), 
                get_memo("benefeciary", message_id));
        ben_payout_sum += ben_payout;
    }
    payout.amount -= ben_payout_sum;

    elaf_t token_prop(elai_t(mssg_itr->tokenprop) / elai_t(config::_100percent));
    auto token_payout = int_cast(elai_t(payout.amount) * token_prop);
    eosio_assert(payout.amount >= token_payout, "publication::payrewards: wrong token_payout value");
    payto(
            message_id.author, 
            eosio::asset(token_payout, payout.symbol), 
            static_cast<enum_t>(payment_t::TOKEN), 
            get_memo("author", message_id));
    payto(
            message_id.author, 
            eosio::asset(payout.amount - token_payout, payout.symbol), 
            static_cast<enum_t>(payment_t::VESTING), 
            get_memo("author", message_id));

    tables::vote_table vote_table(_self, message_id.author.value);
    auto votetable_index = vote_table.get_index<"messageid"_n>();
    auto vote_itr = votetable_index.lower_bound(std::make_tuple(mssg_itr->id, INT64_MAX));
    for (auto vote_etr = votetable_index.end(); vote_itr != vote_etr;) {
       auto& vote = *vote_itr;
       ++vote_itr;
       if (vote.message_id != mssg_itr->id) break;
       vote_table.erase(vote);
    }

    message_table.erase(mssg_itr);
    permlink_table.modify(*permlink_itr, _self, [&](auto&){}); // change payer to contract
    permlink_table.move_to_archive(*permlink_itr);

    fill_depleted_pool(pools, asset(unclaimed_rewards, payout.symbol), pools.end());

    send_postreward_event(message_id, payout, asset(ben_payout_sum, payout.symbol), asset(curation_payout, payout.symbol));
}

void publication::close_message_timer(structures::mssgid message_id, uint64_t id, uint64_t delay_sec) {
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "closemssg"_n, message_id});
    trx.delay_sec = delay_sec;
    trx.send((static_cast<uint128_t>(id) << 64) | message_id.author.value, _self);
}

void publication::check_upvote_time(uint64_t cur_time, uint64_t mssg_date) {
    const auto& cashout_window_param = params().cashout_window_param;
    eosio_assert(
        (cur_time <= mssg_date + ((cashout_window_param.window - cashout_window_param.upvote_lockout) * seconds(1).count())) ||
        (cur_time > mssg_date + (cashout_window_param.window * seconds(1).count())),
        "You can't upvote, because publication will be closed soon.");
}

fixp_t publication::calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool) {
    elaf_t abs_w(elai_t(abs(weight)) / elai_t(config::_100percent));
    tables::limit_table lims(_self, _self.value);
    auto token_code = pool.state.funds.symbol.code();
    int64_t eff_vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, voter, token_code).amount;
    use_charge(lims, structures::limitparams::VOTE, token::get_issuer(config::token_name, token_code),
        voter, eff_vesting, token_code, false, abs_w);
    fixp_t abs_rshares = FP(eff_vesting) * abs_w;
    return (weight < 0) ? -abs_rshares : abs_rshares;
}

void publication::set_vote(name voter, const structures::mssgid& message_id, int16_t weight) {
    require_auth(voter);

    auto get_calc_sharesfn = [&](auto mainfunc_code, auto netshares, auto mainfunc_maxarg) {
        atmsp::machine<fixp_t> machine;
        return set_and_run(machine, mainfunc_code, {netshares}, {{fixp_t(0), mainfunc_maxarg}}).data();
    };

    const auto& max_vote_changes_param = params().max_vote_changes_param;
    const auto& social_acc_param = params().social_acc_param;

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    tables::message_table message_table(_self, message_id.author.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    eosio::check(mssg_itr != message_table.end(), "Message doesn't exist in cashout window.");
    eosio::check(!mssg_itr->closed, "Message is closed.");

    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->pool_date);
    check_acc_vest_balance(voter, pool->state.funds.symbol);
    tables::vote_table vote_table(_self, message_id.author.value);

    auto cur_time = current_time();

    auto votetable_index = vote_table.get_index<"byvoter"_n>();
    auto vote_itr = votetable_index.find(std::make_tuple(mssg_itr->id, voter));
    if (vote_itr != votetable_index.end()) {
        eosio_assert(weight != vote_itr->weight, "Vote with the same weight has already existed.");
        eosio_assert(vote_itr->count != max_vote_changes_param.max_vote_changes, "You can't revote anymore.");

        atmsp::machine<fixp_t> machine;
        fixp_t rshares = calc_available_rshares(voter, weight, cur_time, *pool);
        if(rshares > FP(vote_itr->rshares))
            check_upvote_time(cur_time, mssg_itr->date);

        fixp_t new_mssg_rshares = add_cut(FP(mssg_itr->state.netshares) - FP(vote_itr->rshares), rshares);
        auto rsharesfn_delta = get_delta(machine, FP(mssg_itr->state.netshares), new_mssg_rshares, pool->rules.mainfunc);

        pools.modify(*pool, _self, [&](auto &item) {
            item.state.rshares = ((WP(item.state.rshares) - wdfp_t(FP(vote_itr->rshares))) + wdfp_t(rshares)).data();
            item.state.rsharesfn = (WP(item.state.rsharesfn) + wdfp_t(rsharesfn_delta)).data();
            send_poolstate_event(item);
        });

        message_table.modify(mssg_itr, name(), [&]( auto &item ) {
            item.state.netshares = new_mssg_rshares.data();
            item.state.sumcuratorsw = (FP(item.state.sumcuratorsw) - FP(vote_itr->curatorsw)).data();
            send_poststate_event(
                message_id.author,
                *permlink_itr,
                item,
                get_calc_sharesfn(
                    pool->rules.mainfunc.code,
                    FP(new_mssg_rshares.data()),
                    FP(pool->rules.mainfunc.maxarg)
                )
            );
        });

        votetable_index.modify(vote_itr, voter, [&]( auto &item ) {
            item.weight = weight;
            item.time = cur_time;
            item.curatorsw = fixp_t(0).data();
            item.rshares = rshares.data();
            ++item.count;
            send_votestate_event(voter, item, message_id.author, *permlink_itr);
        });

        return;
    }
    atmsp::machine<fixp_t> machine;
    fixp_t rshares = calc_available_rshares(voter, weight, cur_time, *pool);
    if(rshares > 0)
        check_upvote_time(cur_time, mssg_itr->date);

    structures::messagestate msg_new_state = {
        .netshares = add_cut(FP(mssg_itr->state.netshares), rshares).data(),
        .voteshares = ((rshares > fixp_t(0)) ?
            add_cut(FP(mssg_itr->state.voteshares), rshares) :
            FP(mssg_itr->state.voteshares)).data()
        //.sumcuratorsw = see below
    };

    auto rsharesfn_delta = get_delta(machine, FP(mssg_itr->state.netshares), FP(msg_new_state.netshares), pool->rules.mainfunc);

    pools.modify(*pool, _self, [&](auto &item) {
         item.state.rshares = (WP(item.state.rshares) + wdfp_t(rshares)).data();
         item.state.rsharesfn = (WP(item.state.rsharesfn) + wdfp_t(rsharesfn_delta)).data();
         send_poolstate_event(item);
    });
    eosio_assert(WP(pool->state.rsharesfn) >= 0, "pool state rsharesfn overflow");

    auto sumcuratorsw_delta = get_delta(
            machine, 
            FP(mssg_itr->state.voteshares), 
            FP(msg_new_state.voteshares), 
            pool->rules.curationfunc);
    msg_new_state.sumcuratorsw = (FP(mssg_itr->state.sumcuratorsw) + sumcuratorsw_delta).data();
    message_table.modify(mssg_itr, _self, [&](auto &item) {
        item.state = msg_new_state;
        send_poststate_event(
            message_id.author,
            *permlink_itr,
            item,
            get_calc_sharesfn(
                pool->rules.mainfunc.code,
                FP(msg_new_state.netshares),
                FP(pool->rules.mainfunc.maxarg)
            )
        );
    });

    auto time_delta = static_cast<int64_t>((cur_time - mssg_itr->date) / seconds(1).count());
    elap_t curatorsw_factor =
        std::max(std::min(
        set_and_run(
            machine, 
            pool->rules.timepenalty.code, 
            {fp_cast<fixp_t>(time_delta, false)}, 
            {{fixp_t(0), FP(pool->rules.timepenalty.maxarg)}}),
        fixp_t(1)), fixp_t(0));

    std::vector<structures::delegate_voter> delegators;
    auto token_code = pool->state.funds.symbol.code();
    auto list_delegate_voter = golos::vesting::get_account_delegators(config::vesting_name, voter, token_code);
    auto effective_vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, voter, token_code);

    for (auto record : list_delegate_voter) {
        auto interest_rate = static_cast<uint16_t>(static_cast<uint128_t>(record.quantity.amount) *
                    record.interest_rate / effective_vesting.amount);

        if (interest_rate == 0)
            continue;

        delegators.push_back({record.delegator, interest_rate});
    }

    vote_table.emplace(voter, [&]( auto &item ) {
        item.id = vote_table.available_primary_key();
        item.message_id = mssg_itr->id;
        item.voter = voter;
        item.weight = weight;
        item.time = cur_time;
        item.count += 1;
        item.delegators = delegators;
        item.curatorsw = (fp_cast<fixp_t>(elap_t(sumcuratorsw_delta) * curatorsw_factor)).data();
        item.rshares = rshares.data();
        send_votestate_event(voter, item, message_id.author, *permlink_itr);
    });
}

void publication::fill_depleted_pool(
        tables::reward_pools& pools, 
        eosio::asset quantity, 
        tables::reward_pools::const_iterator excluded) 
{
    using namespace tables;
    using namespace structures;
    eosio_assert(quantity.amount >= 0, "fill_depleted_pool: quantity.amount < 0");
    if(quantity.amount == 0)
        return;
    auto choice = pools.end();
    auto min_ratio = std::numeric_limits<poolstate::ratio_t>::max();
    for(auto pool = pools.begin(); pool != pools.end(); ++pool)
        if((pool->state.funds.symbol == quantity.symbol) && (pool != excluded)) {
            auto cur_ratio = pool->state.get_ratio();
            if(cur_ratio <= min_ratio) {
                min_ratio = cur_ratio;
                choice = pool;
            }
        }
    //sic. we don't need assert here
    if(choice != pools.end())
        pools.modify(*choice, _self, [&](auto &item){
            item.state.funds += quantity;
            send_poolstate_event(item);
        });
}

void publication::on_transfer(name from, name to, eosio::asset quantity, std::string memo) {
    (void)memo;
    require_auth(from);
    if(_self != to)
        return;

    tables::reward_pools pools(_self, _self.value);
    fill_depleted_pool(pools, quantity, pools.end());
}

void publication::set_limit(
        std::string act_str, 
        symbol_code token_code, 
        uint8_t charge_id, 
        int64_t price, 
        int64_t cutoff, 
        int64_t vesting_price, 
        int64_t min_vesting) 
{
    using namespace tables;
    using namespace structures;
    require_auth(_self);
    eosio_assert(
            price < 0 || 
            golos::charge::exist(config::charge_name, token_code, charge_id), 
            "publication::set_limit: charge doesn't exist");
    auto act = limitparams::act_from_str(act_str);
    eosio_assert(act != limitparams::VOTE || charge_id == 0, "publication::set_limit: charge_id for VOTE should be 0");
    //? should we require cutoff to be _100percent if act == VOTE (to synchronize with vesting)?
    eosio_assert(act != limitparams::POSTBW || min_vesting == 0, "publication::set_limit: min_vesting for POSTBW should be 0");
    golos::upsert_tbl<limit_table>(_self, _self.value, _self, act, [&](bool exists) {
        return [&](limitparams& item) {
            item.act = act;
            item.charge_id = charge_id;
            item.price = price;
            item.cutoff = cutoff;
            item.vesting_price = vesting_price;
            item.min_vesting = min_vesting;
        };
    });
}

void publication::set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
    uint16_t maxtokenprop, eosio::symbol tokensymbol
) {
    validate_percent(maxtokenprop, "maxtokenprop");
    //TODO: machine's constants
    using namespace tables;
    using namespace structures;
    require_auth(_self);
    reward_pools pools(_self, _self.value);
    uint64_t created = current_time();

    eosio::asset unclaimed_funds = token::get_balance(config::token_name, _self, tokensymbol.code());

    auto old_pool = pools.begin();
    while(old_pool != pools.end())
        if(!old_pool->state.msgs)
            old_pool = pools.erase(old_pool);
        else {
            if(old_pool->state.funds.symbol == tokensymbol)
                unclaimed_funds -= old_pool->state.funds;
            ++old_pool;
        }
    eosio_assert(pools.find(created) == pools.end(), "rules with this key already exist");

    rewardrules newrules;
    atmsp::parser<fixp_t> pa;
    atmsp::machine<fixp_t> machine;

    newrules.mainfunc     = load_func(mainfunc, "reward func", pa, machine, true);
    newrules.curationfunc = load_func(curationfunc, "curation func", pa, machine, true);
    newrules.timepenalty  = load_func(timepenalty, "time penalty func", pa, machine, true);
    newrules.maxtokenprop = maxtokenprop;

    pools.emplace(_self, [&](auto &item) {
        item.created = created;
        item.rules = newrules;
        item.state.msgs = 0;
        item.state.funds = unclaimed_funds;
        item.state.rshares = (wdfp_t(0)).data();
        item.state.rsharesfn = (wdfp_t(0)).data();
        send_poolstate_event(item);
    });
}

void publication::send_poolstate_event(const structures::rewardpool& pool) {
    structures::pool_event data{pool.created, pool.state.msgs, pool.state.funds, pool.state.rshares, pool.state.rsharesfn};
    eosio::event(_self, "poolstate"_n, data).send();
}

void publication::send_poolerase_event(const structures::rewardpool& pool) {
    eosio::event(_self, "poolerase"_n, pool.created).send();
}

void publication::send_poststate_event(
        name author, 
        const structures::permlink& permlink, 
        const structures::message& post, 
        fixp_t sharesfn) 
{
    structures::post_event data{ author, permlink.value, post.state.netshares, post.state.voteshares,
        post.state.sumcuratorsw, static_cast<base_t>(sharesfn) };
    eosio::event(_self, "poststate"_n, data).send();
}

void publication::send_votestate_event(
        name voter, 
        const structures::voteinfo& vote, 
        name author, 
        const structures::permlink& permlink) 
{
    structures::vote_event data{voter, author, permlink.value, vote.weight, vote.curatorsw, vote.rshares};
    eosio::event(_self, "votestate"_n, data).send();
}

void publication::send_rewardweight_event(structures::mssgid message_id, uint16_t weight) {
    structures::reward_weight_event data{message_id, weight};
    eosio::event(_self, "rewardweight"_n, data).send();
}

void publication::send_postreward_event(const structures::mssgid& message_id, const asset& author, const asset& benefactor, const asset& curator) {
    structures::post_reward_event data{message_id, author, benefactor, curator};
    eosio::event(_self, "postreward"_n, data).send();
}

structures::funcinfo publication::load_func(
        const funcparams& params, 
        const std::string& name, 
        const atmsp::parser<fixp_t>& pa, 
        atmsp::machine<fixp_t>& machine, 
        bool inc) 
{
    eosio_assert(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
    structures::funcinfo ret;
    ret.maxarg = fp_cast<fixp_t>(params.maxarg).data();
    pa(machine, params.str, "x");
    check_positive_monotonic(machine, FP(ret.maxarg), name, inc);
    ret.code.from_machine(machine);
    return ret;
}

fixp_t publication::get_delta(
        atmsp::machine<fixp_t>& machine, 
        fixp_t old_val, 
        fixp_t new_val, 
        const structures::funcinfo& func) 
{
    func.code.to_machine(machine);
    elap_t old_fn = machine.run({old_val}, {{fixp_t(0), FP(func.maxarg)}});
    elap_t new_fn = machine.run({new_val}, {{fixp_t(0), FP(func.maxarg)}});
    return fp_cast<fixp_t>(new_fn - old_fn, false);
}

void publication::check_acc_vest_balance(name user, eosio::symbol tokensymbol) {
    eosio_assert(golos::vesting::balance_exist(config::vesting_name, user, tokensymbol.code()),
        ("vesting balance doesn't exist for " + name{user}.to_string()).c_str());
}

void publication::set_params(std::vector<posting_params> params) {
    require_auth(_self);
    posting_params_singleton cfg(_self, _self.value);
    param_helper::check_params(params, cfg.exists());
    param_helper::set_parameters<posting_params_setter>(params, cfg, _self);
}

void publication::reblog(name rebloger, structures::mssgid message_id, std::string headermssg, std::string bodymssg) {
    require_auth(rebloger);

    eosio_assert(rebloger != message_id.author, "You cannot reblog your own content.");
    eosio_assert(headermssg.length() < config::max_length, "Title length is more than 256.");
    eosio_assert(
        !headermssg.length() || (headermssg.length() && bodymssg.length()),
        "Body must be set if title is set."
    );

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(),
                 "You can't reblog, because this message doesn't exist.");
}

void publication::erase_reblog(name rebloger, structures::mssgid message_id) {
    require_auth(rebloger);
    eosio_assert(rebloger != message_id.author, "You cannot erase reblog your own content.");

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(),
                 "You can't erase reblog, because this message doesn't exist.");
}

int64_t publication::pay_delegators(int64_t claim, name voter,
        eosio::symbol tokensymbol, std::vector<structures::delegate_voter> delegate_list, structures::mssgid message_id) {
    int64_t dlg_payout_sum = 0;
    for (auto delegate_obj : delegate_list) {
        auto dlg_payout = claim * delegate_obj.interest_rate / config::_100percent;
        payto(
                delegate_obj.delegator, 
                eosio::asset(dlg_payout, tokensymbol), 
                static_cast<enum_t>(payment_t::VESTING), 
                get_memo("delegator", message_id));

        dlg_payout_sum += dlg_payout;
    }
    return dlg_payout_sum;
}

bool publication::validate_permlink(std::string permlink) {
    for (auto symbol : permlink) {
        if ((symbol >= '0' && symbol <= '9') ||
            (symbol >= 'a' && symbol <= 'z') ||
             symbol == '-') {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

std::string publication::get_memo(const std::string &type, const structures::mssgid &message_id) {
    return std::string(type + " reward for post " + name{message_id.author}.to_string() + ":"
                                                  + message_id.permlink);
}

const auto& publication::get_message(const tables::message_table& messages, const structures::mssgid& message_id) {
    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    // rare case - not critical for performance
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    auto message_itr = messages.find(permlink_itr->id);
    eosio::check(message_itr != messages.end(), "Message doesn't exist in cashout window.");
    return *message_itr;
}

void publication::set_curators_prcnt(structures::mssgid message_id, uint16_t curators_prcnt) {
    require_auth(message_id.author);

    const auto& param = params().curators_prcnt_param;
    param.validate_value(curators_prcnt);

    tables::message_table message_table(_self, message_id.author.value);
    const auto& mssg = get_message(message_table, message_id);

    eosio::check(mssg.state.voteshares == 0, "Curators percent can be changed only before voting.");
    eosio::check(
            mssg.curators_prcnt != curators_prcnt,
            "Same curators percent value is already set."); // TODO: tests #617

    message_table.modify(mssg, name(), [&](auto& item) {
        item.curators_prcnt = curators_prcnt;
    });
}

void publication::set_max_payout(structures::mssgid message_id, asset max_payout) {
    require_auth(message_id.author);

    tables::message_table message_table(_self, message_id.author.value);
    const auto& mssg = get_message(message_table, message_id);

    eosio::check(mssg.state.voteshares == 0, "Max payout can be changed only before voting.");

    eosio::check(max_payout != mssg.max_payout, "Same max payout is already set.");
    eosio::check(max_payout >= asset(0, mssg.max_payout.symbol), "max_payout must not be negative.");
    eosio::check(max_payout < mssg.max_payout, "Cannot replace max payout with greater one.");

    message_table.modify(mssg, name(), [&](auto& item) {
        item.max_payout = max_payout;
    });
}

void publication::calcrwrdwt(name account, int64_t mssg_id, int64_t post_charge) {
    require_auth(_self);
    tables::limit_table lims(_self, _self.value);
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    eosio_assert(bw_lim_itr != lims.end(), "publication::calc_reward_weight: limit parameters not set");
    if (post_charge > bw_lim_itr->cutoff) {
        elaf_t weight(elai_t(bw_lim_itr->cutoff * bw_lim_itr->cutoff) / elai_t(post_charge * post_charge));
        auto reward_weight = int_cast(elai_t(config::_100percent) * weight);
        validate_percent(reward_weight, "calculated reward weight");        // should never fail in normal conditions

        tables::message_table message_table(_self, account.value);
        auto message_itr = message_table.find(mssg_id);
        eosio::check(message_itr != message_table.end(), "Message doesn't exist in cashout window.");
        message_table.modify(message_itr, name(), [&]( auto &item ) {
            item.rewardweight = reward_weight;
        });

        tables::permlink_table permlink_table(_self, account.value);
        auto permlink_itr = permlink_table.find(mssg_id);
        eosio::check(permlink_itr != permlink_table.end(), "Permlink doesn't exist.");

        send_rewardweight_event(structures::mssgid{account, permlink_itr->value}, reward_weight);
    }
}

} // golos
