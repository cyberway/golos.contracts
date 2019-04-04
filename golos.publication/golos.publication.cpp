#include "golos.publication.hpp"
#include <eosiolib/transaction.hpp>
#include <eosiolib/event.hpp>
#include <eosiolib/archive.hpp>
#include <golos.social/golos.social.hpp>
#include <golos.vesting/golos.vesting.hpp>
#include <golos.referral/golos.referral.hpp>
#include <golos.charge/golos.charge.hpp>
#include <common/upsert.hpp>
#include "utils.hpp"
#include "objects.hpp"

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
        if (NN(setcurprcnt) == action)
            execute_action(&publication::set_curators_prcnt);
        if (NN(calcrwrdwt) == action)
            execute_action(&publication::calcrwrdwt);
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

void publication::create_message(structures::mssgid message_id,
                              structures::mssgid parent_id,
                              uint64_t parent_recid,
                              std::vector<structures::beneficiary> beneficiaries,
                            //actually, beneficiaries[i].prop _here_ should be interpreted as (share * _100percent)
                            //but not as a raw data for elaf_t, may be it's better to use another type (with uint16_t field for prop).
                              int64_t tokenprop, bool vestpayment,
                              std::string headermssg,
                              std::string bodymssg, std::string languagemssg,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata,
                              std::optional<uint16_t> curators_prcnt = std::nullopt) {
    require_auth(message_id.author);

    int ref_block_num = message_id.ref_block_num % 65536;
    eosio_assert((tapos_block_num()<ref_block_num?65536:0)+tapos_block_num()-ref_block_num < 2*60*60/3, "ref_block_num mismatch");

    eosio_assert(message_id.permlink.length() && message_id.permlink.length() < config::max_length, "Permlink length is empty or more than 256.");
    eosio_assert(check_permlink_correctness(message_id.permlink), "Permlink contains wrong symbol.");
    eosio_assert(headermssg.length() < config::max_length, "Title length is more than 256.");
    eosio_assert(bodymssg.length(), "Body is empty.");

    posting_params_singleton cfg(_self, _self.value);
    const auto &cashout_window_param = cfg.get().cashout_window_param;
    const auto &max_beneficiaries_param = cfg.get().max_beneficiaries_param;
    const auto &max_comment_depth_param = cfg.get().max_comment_depth_param;
    const auto &social_acc_param = cfg.get().social_acc_param;
    const auto &referral_acc_param = cfg.get().referral_acc_param;
    const auto &curators_prcnt_param = cfg.get().curators_prcnt_param;

    if (parent_id.author) {
        if (social_acc_param.account) {
            eosio_assert(!social::is_blocking(social_acc_param.account, parent_id.author, message_id.author), "You are blocked by this account");
        }
    }

    tables::reward_pools pools(_self, _self.value);
    auto pool = pools.begin();   // TODO: Reverse iterators doesn't work correctly
    eosio_assert(pool != pools.end(), "publication::create_message: [pools] is empty");
    pool = --pools.end();
    check_account(message_id.author, pool->state.funds.symbol);
    auto token_code = pool->state.funds.symbol.code();
    auto issuer = token::get_issuer(config::token_name, token_code);
    tables::limit_table lims(_self, _self.value);

    use_charge(lims, parent_id.author ? structures::limitparams::COMM : structures::limitparams::POST, issuer, message_id.author,
        golos::vesting::get_account_effective_vesting(config::vesting_name, message_id.author, token_code).amount, token_code, vestpayment);

    tables::message_table message_table(_self, message_id.author.value);
    uint64_t message_pk = message_table.available_primary_key();
    if (message_pk == 0)
        message_pk = 1;
    if(!parent_id.author)
        use_postbw_charge(lims, issuer, message_id.author, token_code, message_pk);

    std::map<name, int64_t> benefic_map;
    int64_t prop_sum = 0;
    for (auto& ben : beneficiaries) {
        check_account(ben.account, pool->state.funds.symbol);
        eosio_assert(0 < ben.deductprcnt && ben.deductprcnt <= config::_100percent, "publication::create_message: wrong ben.prop value");
        prop_sum += ben.deductprcnt;
        eosio_assert(prop_sum <= config::_100percent, "publication::create_message: prop_sum > 100%");
        benefic_map[ben.account] += ben.deductprcnt; //several entries for one user? ok.
    }

    if (referral_acc_param.account != name()) {
        auto obj_referral = golos::referral::account_referrer( referral_acc_param.account, message_id.author );
        if ( !obj_referral.is_empty() ) {
            auto& referrer = obj_referral.referrer;
            const auto& itr = std::find_if( beneficiaries.begin(), beneficiaries.end(),
                                            [&referrer] (const structures::beneficiary& benef) {
                return benef.account == referrer;
            }
            );

            eosio_assert( itr == beneficiaries.end(), "Comment already has referrer as a referrer-beneficiary." );

            prop_sum += obj_referral.percent;
            eosio_assert(prop_sum <= config::_100percent, "publication::create_message: prop_sum > 100%");
            benefic_map[obj_referral.referrer] += obj_referral.percent;
        }
    }

        //reusing a vector
    beneficiaries.reserve(benefic_map.size());
    beneficiaries.clear();
    for(auto & ben : benefic_map)
        beneficiaries.emplace_back(structures::beneficiary{
            .account = ben.first,
            .deductprcnt = static_cast<base_t>(get_limit_prop(ben.second).data())
        });

    eosio_assert((benefic_map.size() <= max_beneficiaries_param.max_beneficiaries), "publication::create_message: benafic_map.size() > MAX_BENEFICIARIES");

    auto cur_time = current_time();
    atmsp::machine<fixp_t> machine;

    eosio_assert(cur_time >= pool->created, "publication::create_message: cur_time < pool.created");
    eosio_assert(pool->state.msgs < std::numeric_limits<structures::counter_t>::max(), "publication::create_message: pool->msgs == max_counter_val");
    pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });

    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto message_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(message_itr == message_index.end(), "This message already exists.");

    uint64_t parent_pk = 0;
    uint16_t level = 0;
    if(parent_id.author) {
        tables::message_table parent_table(_self, parent_id.author.value);
        auto parent_index = parent_table.get_index<"bypermlink"_n>();
        auto parent_itr = parent_index.find({parent_id.permlink, parent_id.ref_block_num});

        if(parent_itr != parent_index.end()) {
            parent_index.modify(parent_itr, name(), [&]( auto &item ) {
                    ++item.childcount;
                });
            parent_pk = parent_itr->id;
            level = 1 + parent_itr->level;
        } else {
            // Try to found info about post in archive
            structures::archive_record record;
            bool found_in_archive = parent_recid ? eosio::lookup_record(parent_recid, _self, record) : false;
            eosio_assert(found_in_archive, "Parent message doesn't exist");
            std::visit([&](const structures::archive_info_v1& info) {
                eosio_assert(info.id == parent_id, "Parent message is different with archive");
                level = 1 + info.level;
            }, record);
        }
    }
    eosio_assert(level <= max_comment_depth_param.max_comment_depth, "publication::create_message: level > MAX_COMMENT_DEPTH");

    auto mssg_itr = message_table.emplace(message_id.author, [&]( auto &item ) {
        item.id = message_pk;
        item.permlink = message_id.permlink;
        item.ref_block_num = message_id.ref_block_num;
        item.date = cur_time;
        item.parentacc = parent_id.author;
        item.parent_id = parent_pk;
        item.tokenprop = static_cast<base_t>(std::min(get_limit_prop(tokenprop), ELF(pool->rules.maxtokenprop)).data()),
        item.beneficiaries = beneficiaries;
        item.rewardweight = static_cast<base_t>(elaf_t(1).data()); //we will get actual value from charge on post closing
        item.childcount = 0;
        item.level = level;
        item.curators_prcnt = get_checked_curators_prcnt(curators_prcnt);
    });

    structures::archive_info_v1 info{message_id,level};
    uint64_t record_id = eosio::save_record(structures::archive_record{info});

    structures::create_event evt{record_id};
    eosio::event(_self, "postcreate"_n, evt).send();

    close_message_timer(message_id, message_pk, cashout_window_param.window);
}

void publication::update_message(structures::mssgid message_id,
                              std::string headermssg, std::string bodymssg,
                              std::string languagemssg, std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(message_id.author);
    tables::message_table message_table(_self, message_id.author.value);
    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto message_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(message_itr != message_index.end(), "Message doesn't exist.");
}

auto publication::get_pool(tables::reward_pools& pools, uint64_t time) {
    eosio_assert(pools.begin() != pools.end(), "publication::get_pool: [pools] is empty");

    auto pool = pools.upper_bound(time);

    eosio_assert(pool != pools.begin(), "publication::get_pool: can't find an appropriate pool");
    return (--pool);
}

void publication::delete_message(structures::mssgid message_id) {
    require_auth(message_id.author);

    tables::message_table message_table(_self, message_id.author.value);
    tables::vote_table vote_table(_self, message_id.author.value);

    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto mssg_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(mssg_itr != message_index.end(), "Message doesn't exist.");
    eosio_assert((mssg_itr->childcount) == 0, "You can't delete comment with child comments.");
    eosio_assert(FP(mssg_itr->state.netshares) <= 0, "Cannot delete a comment with net positive votes.");

    if(mssg_itr->parentacc) {
        tables::message_table parent_table(_self, mssg_itr->parentacc.value);
        auto parent_itr = parent_table.find(mssg_itr->parent_id);
        if(parent_itr != parent_table.end()) {
            parent_table.modify(parent_itr, name(), [&]( auto &item ) {
                --item.childcount;
            });
        }
    } else {
        tables::reward_pools pools(_self, _self.value);
        remove_postbw_charge(message_id.author, get_pool(pools, mssg_itr->date)->state.funds.symbol.code(), mssg_itr->id);
    }

    cancel_deferred((static_cast<uint128_t>(mssg_itr->id) << 64) | message_id.author.value);

    message_index.erase(mssg_itr);

    auto votetable_index = vote_table.get_index<"messageid"_n>();
    auto vote_itr = votetable_index.lower_bound(mssg_itr->id);
    while ((vote_itr != votetable_index.end()) && (vote_itr->message_id == mssg_itr->id))
        vote_itr = votetable_index.erase(vote_itr);
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

void publication::payto(name user, eosio::asset quantity, enum_t mode) {
    require_auth(_self);
    eosio_assert(quantity.amount >= 0, "LOGIC ERROR! publication::payto: quantity.amount < 0");
    if(quantity.amount == 0)
        return;

    if(static_cast<payment_t>(mode) == payment_t::TOKEN)
        INLINE_ACTION_SENDER(token, payment) (config::token_name, {_self, config::active_name}, {_self, user, quantity, ""});
    else if(static_cast<payment_t>(mode) == payment_t::VESTING)
        INLINE_ACTION_SENDER(token, transfer) (config::token_name, {_self, config::active_name},
            {_self, config::vesting_name, quantity, config::send_prefix + name{user}.to_string()});
    else
        eosio_assert(false, "publication::payto: unknown kind of payment");
}

int64_t publication::pay_curators(name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum, eosio::symbol tokensymbol) {
    tables::vote_table vs(_self, author.value);
    int64_t unclaimed_rewards = max_rewards;

    auto idx = vs.get_index<"messageid"_n>();
    auto v = idx.lower_bound(msgid);
    while ((v != idx.end()) && (v->message_id == msgid)) {
        if((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
            auto claim = int_cast(elai_t(max_rewards) * elaf_t(FP(v->curatorsw) / weights_sum));
            eosio_assert(claim <= unclaimed_rewards, "LOGIC ERROR! publication::pay_curators: claim > unclaimed_rewards");
            if(claim > 0) {
                unclaimed_rewards -= claim;
                claim -= pay_delegators(claim, v->voter, tokensymbol, v->delegators);
                payto(v->voter, eosio::asset(claim, tokensymbol), static_cast<enum_t>(payment_t::VESTING));
            }
        }
        //v = idx.erase(v);
        ++v;
    }
    return unclaimed_rewards;
}

void publication::remove_postbw_charge(name account, symbol_code token_code, int64_t mssg_id, elaf_t* reward_weight_ptr) {
    auto issuer = token::get_issuer(config::token_name, token_code);
    tables::limit_table lims(_self, _self.value);
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    eosio_assert(bw_lim_itr != lims.end(), "publication::remove_postbw_charge: limit parameters not set");
    auto post_charge = golos::charge::get_stored(config::charge_name, account, token_code, bw_lim_itr->charge_id, mssg_id);
    if(post_charge >= 0)
        INLINE_ACTION_SENDER(charge, removestored) (config::charge_name,
            {issuer, config::invoice_name}, {
                account,
                token_code,
                bw_lim_itr->charge_id,
                mssg_id
            });
    if(reward_weight_ptr)
        *reward_weight_ptr = (post_charge > bw_lim_itr->cutoff) ?
            static_cast<elaf_t>(elai_t(bw_lim_itr->cutoff) / elai_t(post_charge)) : elaf_t(1);
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

void publication::use_postbw_charge(tables::limit_table& lims, name issuer, name account, symbol_code token_code, int64_t mssg_id) {
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    if(bw_lim_itr->price >= 0)
        INLINE_ACTION_SENDER(charge, useandnotify) (config::charge_name,
            {issuer, config::invoice_name}, {
                account,
                token_code,
                bw_lim_itr->charge_id,
                mssg_id,
                bw_lim_itr->price,
                _self
            });
}

void publication::close_message(structures::mssgid message_id) {
    require_auth(_self);
    tables::message_table message_table(_self, message_id.author.value);
    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto mssg_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(mssg_itr != message_index.end(), "Message doesn't exist.");

    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->date);

    eosio_assert(pool->state.msgs != 0, "LOGIC ERROR! publication::payrewards: pool.msgs is equal to zero");
    atmsp::machine<fixp_t> machine;
    fixp_t sharesfn = set_and_run(machine, pool->rules.mainfunc.code, {FP(mssg_itr->state.netshares)}, {{fixp_t(0), FP(pool->rules.mainfunc.maxarg)}});

    auto state = pool->state;
    auto reward_weight = elaf_t(1);
    int64_t payout = 0;
    if(state.msgs == 1) {
        payout = state.funds.amount;
        eosio_assert(state.rshares == mssg_itr->state.netshares, "LOGIC ERROR! publication::payrewards: pool->rshares != mssg_itr->netshares for last message");
        eosio_assert(state.rsharesfn == sharesfn.data(), "LOGIC ERROR! publication::payrewards: pool->rsharesfn != sharesfn.data() for last message");
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

            if(!mssg_itr->parentacc)
                remove_postbw_charge(message_id.author, pool->state.funds.symbol.code(), mssg_itr->id, &reward_weight);

            payout = int_cast(reward_weight * elai_t(elai_t(state.funds.amount) * static_cast<elaf_t>(elap_t(numer) / elap_t(denom))));
            state.funds.amount -= payout;
            eosio_assert(state.funds.amount >= 0, "LOGIC ERROR! publication::payrewards: state.funds < 0");
        }

        auto new_rshares = WP(state.rshares) - wdfp_t(FP(mssg_itr->state.netshares));
        auto new_rsharesfn = WP(state.rsharesfn) - wdfp_t(sharesfn);

        eosio_assert(new_rshares >= 0, "LOGIC ERROR! publication::payrewards: new_rshares < 0");
        eosio_assert(new_rsharesfn >= 0, "LOGIC ERROR! publication::payrewards: new_rsharesfn < 0");

        state.rshares = new_rshares.data();
        state.rsharesfn = new_rsharesfn.data();
    }

    auto curation_payout = int_cast(ELF(mssg_itr->curators_prcnt) * elai_t(payout));

    eosio_assert((curation_payout <= payout) && (curation_payout >= 0), "publication::payrewards: wrong curation_payout");

    auto unclaimed_rewards = pay_curators(message_id.author, mssg_itr->id, curation_payout, FP(mssg_itr->state.sumcuratorsw), state.funds.symbol);

    eosio_assert(unclaimed_rewards >= 0, "publication::payrewards: unclaimed_rewards < 0");

    state.funds.amount += unclaimed_rewards;
    payout -= curation_payout;

    int64_t ben_payout_sum = 0;
    for(auto& ben : mssg_itr->beneficiaries) {
        auto ben_payout = int_cast(elai_t(payout) * ELF(ben.deductprcnt));
        eosio_assert((0 <= ben_payout) && (ben_payout <= payout - ben_payout_sum), "LOGIC ERROR! publication::payrewards: wrong ben_payout value");
        payto(ben.account, eosio::asset(ben_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));
        ben_payout_sum += ben_payout;
    }
    payout -= ben_payout_sum;

    auto token_payout = int_cast(elai_t(payout) * ELF(mssg_itr->tokenprop));
    eosio_assert(payout >= token_payout, "publication::payrewards: wrong token_payout value");

    payto(message_id.author, eosio::asset(token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::TOKEN));
    payto(message_id.author, eosio::asset(payout - token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));

    tables::vote_table vote_table(_self, message_id.author.value);
    auto votetable_index = vote_table.get_index<"messageid"_n>();
    auto vote_itr = votetable_index.lower_bound(mssg_itr->id);
    for (auto vote_etr = votetable_index.end(); vote_itr != vote_etr;) {
       auto& vote = *vote_itr;
       ++vote_itr;
       if (vote.message_id != mssg_itr->id) break;
       vote_table.erase(vote);
    }

    message_index.erase(mssg_itr);

    bool pool_erased = false;
    state.msgs--;
    if(state.msgs == 0) {
        if(pool != --pools.end()) {//there is a pool after, so we can delete this one
            eosio_assert(state.funds.amount == unclaimed_rewards, "LOGIC ERROR! publication::payrewards: state.funds != unclaimed_rewards");
            fill_depleted_pool(pools, state.funds, pool);
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
}

void publication::close_message_timer(structures::mssgid message_id, uint64_t id, uint64_t delay_sec) {
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, config::active_name), _self, "closemssg"_n, message_id});
    trx.delay_sec = delay_sec;
    trx.send((static_cast<uint128_t>(id) << 64) | message_id.author.value, _self);
}

void publication::check_upvote_time(uint64_t cur_time, uint64_t mssg_date) {
    posting_params_singleton cfg(_self, _self.value);
    const auto &cashout_window_param = cfg.get().cashout_window_param;

    eosio_assert((cur_time <= mssg_date + ((cashout_window_param.window - cashout_window_param.upvote_lockout) * seconds(1).count())) ||
                 (cur_time > mssg_date + (cashout_window_param.window * seconds(1).count())),
                 "You can't upvote, because publication will be closed soon.");
}

fixp_t publication::calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool) {
    elaf_t abs_w = get_limit_prop(abs(weight));
    tables::limit_table lims(_self, _self.value);
    auto token_code = pool.state.funds.symbol.code();
    int64_t eff_vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, voter, token_code).amount;
    use_charge(lims, structures::limitparams::VOTE, token::get_issuer(config::token_name, token_code),
        voter, eff_vesting, token_code, false, abs_w);
    fixp_t abs_rshares = fp_cast<fixp_t>(eff_vesting, false) * abs_w;
    return (weight < 0) ? -abs_rshares : abs_rshares;
}

void publication::set_vote(name voter, const structures::mssgid& message_id, int16_t weight) {
    require_auth(voter);

    auto get_calc_sharesfn = [&](auto mainfunc_code, auto netshares, auto mainfunc_maxarg) {
        atmsp::machine<fixp_t> machine;
        return set_and_run(machine, mainfunc_code, {netshares}, {{fixp_t(0), mainfunc_maxarg}}).data();
    };

    posting_params_singleton cfg(_self, _self.value);
    const auto &max_vote_changes_param = cfg.get().max_vote_changes_param;
    const auto &social_acc_param = cfg.get().social_acc_param;

    tables::message_table message_table(_self, message_id.author.value);
    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto mssg_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(mssg_itr != message_index.end(), "Message doesn't exist.");

    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->date);
    check_account(voter, pool->state.funds.symbol);
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

        fixp_t new_mssg_rshares = (FP(mssg_itr->state.netshares) - FP(vote_itr->rshares)) + rshares;
        auto rsharesfn_delta = get_delta(machine, FP(mssg_itr->state.netshares), new_mssg_rshares, pool->rules.mainfunc);

        pools.modify(*pool, _self, [&](auto &item) {
            item.state.rshares = ((WP(item.state.rshares) - wdfp_t(FP(vote_itr->rshares))) + wdfp_t(rshares)).data();
            item.state.rsharesfn = (WP(item.state.rsharesfn) + wdfp_t(rsharesfn_delta)).data();
            send_poolstate_event(item);
        });
        
        message_index.modify(mssg_itr, name(), [&]( auto &item ) {
            item.state.netshares = new_mssg_rshares.data();
            item.state.sumcuratorsw = (FP(item.state.sumcuratorsw) - FP(vote_itr->curatorsw)).data();
            send_poststate_event(
                message_id.author, 
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
            send_votestate_event(voter, item, message_id.author, *mssg_itr);
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
    eosio_assert(WP(pool->state.rshares) >= 0, "pool state rshares overflow");
    eosio_assert(WP(pool->state.rsharesfn) >= 0, "pool state rsharesfn overflow");

    auto sumcuratorsw_delta = get_delta(machine, FP(mssg_itr->state.voteshares), FP(msg_new_state.voteshares), pool->rules.curationfunc);
    msg_new_state.sumcuratorsw = (FP(mssg_itr->state.sumcuratorsw) + sumcuratorsw_delta).data();
    message_index.modify(mssg_itr, _self, [&](auto &item) {
        item.state = msg_new_state;
        send_poststate_event(
            message_id.author,
            item,
            get_calc_sharesfn(
                pool->rules.mainfunc.code, 
                FP(msg_new_state.netshares),
                FP(pool->rules.mainfunc.maxarg)
            )
        );
    });

    auto time_delta = static_cast<int64_t>((cur_time - mssg_itr->date) / seconds(1).count());
    auto curatorsw_factor =
        std::max(std::min(
        set_and_run(machine, pool->rules.timepenalty.code, {fp_cast<fixp_t>(time_delta, false)}, {{fixp_t(0), FP(pool->rules.timepenalty.maxarg)}}),
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

        delegators.push_back({record.delegator, record.quantity, interest_rate, record.payout_strategy});
    }

    vote_table.emplace(voter, [&]( auto &item ) {
        item.id = vote_table.available_primary_key();
        item.message_id = mssg_itr->id;
        item.voter = voter;
        item.weight = weight;
        item.time = cur_time;
        item.count += 1;
        item.delegators = delegators;
        item.curatorsw = (fixp_t(sumcuratorsw_delta * curatorsw_factor)).data();
        item.rshares = rshares.data();
        send_votestate_event(voter, item, message_id.author, *mssg_itr);
    });

    if (social_acc_param.account) {
        INLINE_ACTION_SENDER(golos::social, changereput)
            (social_acc_param.account, {social_acc_param.account, config::active_name},
            {voter, message_id.author, (rshares.data() >> 6)});
    }
}

void publication::fill_depleted_pool(tables::reward_pools& pools, eosio::asset quantity, tables::reward_pools::const_iterator excluded) {
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

void publication::set_limit(std::string act_str, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff, int64_t vesting_price, int64_t min_vesting) {
    using namespace tables;
    using namespace structures;
    require_auth(_self);
    eosio_assert(price < 0 || golos::charge::exist(config::charge_name, token_code, charge_id), "publication::set_limit: charge doesn't exist");
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
    int64_t maxtokenprop, eosio::symbol tokensymbol) {
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

    newrules.maxtokenprop = static_cast<base_t>(get_limit_prop(maxtokenprop).data());

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
    structures::pool_event data{pool.created, pool.state.msgs, pool.state.funds, pool.state.rshares};
    eosio::event(_self, "poolstate"_n, data).send();
}

void publication::send_poolerase_event(const structures::rewardpool& pool) {
    eosio::event(_self, "poolerase"_n, pool.created).send();
}

void publication::send_poststate_event(name author, const structures::message& post, base_t sharesfn) {
    structures::post_event data{author, post.permlink, post.state.netshares, post.state.voteshares,
        post.state.sumcuratorsw, sharesfn};
    eosio::event(_self, "poststate"_n, data).send();
}

void publication::send_votestate_event(name voter, const structures::voteinfo& vote, name author, const structures::message& post) {
    structures::vote_event data{voter, author, post.permlink, vote.weight, vote.curatorsw, vote.rshares};
    eosio::event(_self, "votestate"_n, data).send();
}

void publication::send_rewardweight_event(structures::mssgid message_id, base_t weight) {
    structures::reward_weight_event data{message_id, weight};
    eosio::event(_self, "rewardweight"_n, data).send();
}

structures::funcinfo publication::load_func(const funcparams& params, const std::string& name, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc) {
    eosio_assert(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
    structures::funcinfo ret;
    ret.maxarg = fp_cast<fixp_t>(params.maxarg).data();
    pa(machine, params.str, "x");
    check_positive_monotonic(machine, FP(ret.maxarg), name, inc);
    ret.code.from_machine(machine);
    return ret;
}

fixp_t publication::get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val, const structures::funcinfo& func) {
    func.code.to_machine(machine);
    elap_t old_fn = machine.run({old_val}, {{fixp_t(0), FP(func.maxarg)}});
    elap_t new_fn = machine.run({new_val}, {{fixp_t(0), FP(func.maxarg)}});
    return fp_cast<fixp_t>(new_fn - old_fn, false);
}

void publication::check_account(name user, eosio::symbol tokensymbol) {
    eosio_assert(golos::vesting::balance_exist(config::vesting_name, user, tokensymbol.code()),
        ("unregistered user: " + name{user}.to_string()).c_str());
}

void publication::set_params(std::vector<posting_params> params) {
    require_auth(_self);
    posting_params_singleton cfg(_self, _self.value);
    param_helper::check_params(params, cfg.exists());
    param_helper::set_parameters<posting_params_setter>(params, cfg, _self);
}

void publication::reblog(name rebloger, structures::mssgid message_id) {
    require_auth(rebloger);
    tables::message_table message_table(_self, message_id.author.value);
    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto mssg_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(mssg_itr != message_index.end(),
            "You can't reblog, because this message doesn't exist.");
}

int64_t publication::pay_delegators(int64_t claim, name voter,
        eosio::symbol tokensymbol, std::vector<structures::delegate_voter> delegate_list) {
    int64_t dlg_payout_sum = 0;
    for (auto delegate_obj : delegate_list) {
        auto dlg_payout = claim * delegate_obj.interest_rate / config::_100percent;
        INLINE_ACTION_SENDER(golos::vesting, paydelegator) (config::vesting_name,
            {config::vesting_name, config::active_name},
            {voter, eosio::asset(dlg_payout, tokensymbol), delegate_obj.delegator,
            delegate_obj.payout_strategy});
        dlg_payout_sum += dlg_payout;
    }
    return dlg_payout_sum;
}

bool publication::check_permlink_correctness(std::string permlink) {
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

base_t publication::get_checked_curators_prcnt(std::optional<uint16_t> curators_prcnt) {
    posting_params_singleton cfg(_self, _self.value);
    const auto &curators_prcnt_param = cfg.get().curators_prcnt_param;

    if (curators_prcnt.has_value()) {
        eosio_assert(curators_prcnt.value() >= curators_prcnt_param.min_curators_prcnt,
                     "Curators percent is less than min curators percent.");
        eosio_assert(curators_prcnt.value() <= curators_prcnt_param.max_curators_prcnt,
                     "Curators percent is greater than max curators percent.");
            return static_cast<base_t>(get_limit_prop(static_cast<int64_t>(curators_prcnt.value())).data());
    }
    return static_cast<base_t>(get_limit_prop(static_cast<int64_t>(curators_prcnt_param.min_curators_prcnt)).data());
}

void publication::set_curators_prcnt(structures::mssgid message_id, uint16_t curators_prcnt) {
    require_auth(message_id.author);
    tables::message_table message_table(_self, message_id.author.value);
    auto message_index = message_table.get_index<"bypermlink"_n>();
    auto message_itr = message_index.find({message_id.permlink, message_id.ref_block_num});
    eosio_assert(message_itr != message_index.end(), "Message doesn't exist.");

    eosio_assert(message_itr->state.voteshares == 0,
            "Curators percent can be changed only before voting.");

    message_index.modify(message_itr, name(), [&]( auto &item ) {
            item.curators_prcnt = get_checked_curators_prcnt(curators_prcnt);
        });
}

void publication::calcrwrdwt(name account, int64_t mssg_id, base_t post_charge) {
    require_auth(_self);
    tables::limit_table lims(_self, _self.value);
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    eosio_assert(bw_lim_itr != lims.end(), "publication::calc_reward_weight: limit parameters not set");
    if (post_charge > bw_lim_itr->cutoff) {
        auto weight = static_cast<elaf_t>(elai_t(bw_lim_itr->cutoff) / elai_t(post_charge));
        auto reward_weight = static_cast<base_t>(weight.data()); 

        tables::message_table message_table(_self, account.value);
        auto message_index = message_table.get_index<"id"_n>();
        auto message_itr = message_index.find(mssg_id);
        eosio_assert(message_itr != message_index.end(), "Message doesn't exist.");
        message_index.modify(message_itr, name(), [&]( auto &item ) {
            item.rewardweight = reward_weight;
        });

        send_rewardweight_event(structures::mssgid{account, message_itr->permlink, message_itr->ref_block_num}, reward_weight);
    }
}

} // golos
