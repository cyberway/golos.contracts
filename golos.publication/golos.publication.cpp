#include "golos.publication.hpp"
#include <eosio/event.hpp>
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

        if (receiver != code)
            return;

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
        if (NN(closemssgs) == action)
            execute_action(&publication::close_messages);
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
        if (NN(deletevotes) == action)
            execute_action(&publication::deletevotes);
        if (NN(addpermlink) == action)
            execute_action(&publication::addpermlink);
        if (NN(delpermlink) == action)
            execute_action(&publication::delpermlink);
        if (NN(addpermlinks) == action)
            execute_action(&publication::addpermlinks);
        if (NN(delpermlinks) == action)
            execute_action(&publication::delpermlinks);
        if (NN(syncpool) == action)
            execute_action(&publication::syncpool);
    }
#undef NN
}

struct posting_params_setter: set_params_visitor<posting_state> {
    std::optional<st_bwprovider> new_bwprovider;
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

    bool operator()(const bwprovider_prm& p) {
        new_bwprovider = p;
        return set_param(p, &posting_state::bwprovider_param);
    }

    bool operator()(const min_abs_rshares_prm& param) {
        return set_param(param, &posting_state::min_abs_rshares_param);
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

#   ifdef DISABLE_CHARGE_VESTING
    eosio::check(!vestpayment, "vestpayment disabled");
#   endif
    eosio::check(message_id.permlink.length() && message_id.permlink.length() < config::max_length,
            "Permlink length is empty or more than 256.");
    eosio::check(validate_permlink(message_id.permlink), "Permlink contains wrong symbol.");
    eosio::check(headermssg.length() < config::max_length, "Title length is more than 256.");
    eosio::check(bodymssg.length(), "Body is empty.");
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

    // TODO: Temporarily disabled - not all blockings stored in table and now this check anyway should be in client
    // if (parent_id.author) {
    //     if (social_acc_param.account) {
    //         eosio::check(!social::is_blocking(social_acc_param.account, parent_id.author, message_id.author),
    //                 "You are blocked by this account");
    //     }
    // }

    // close after basic checks. or else it can consume CPU on closing and fail later on bad input params
    close_messages(message_id.author);

    tables::reward_pools pools(_self, _self.value);
    auto pool = pools.begin();   // TODO: Reverse iterators doesn't work correctly
    eosio::check(pool != pools.end(), "publication::create_message: [pools] is empty");
    pool = pools.end();
    --pool;
    check_acc_vest_balance(message_id.author, pool->state.funds.symbol);

    if (!!max_payout) {
        eosio::check(max_payout >= asset(0, pool->state.funds.symbol), "max_payout must not be negative.");
    } else {
        max_payout = asset(asset::max_amount, pool->state.funds.symbol);
    }

    auto token_code = pool->state.funds.symbol.code();
    auto issuer = token::get_issuer(config::token_name, token_code);
    tables::limit_table lims(_self, _self.value);
    use_charge(
            lims,
            parent_id.author ? structures::limitparams::COMM : structures::limitparams::POST,
            issuer, message_id.author,
            golos::vesting::get_account_effective_vesting(config::vesting_name, message_id.author, token_code).amount,
            token_code,
            vestpayment);

    tables::permlink_table permlink_table(_self, message_id.author.value);
    tables::message_table message_table(_self, _self.value);
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

    eosio::check((benefic_map.size() <= max_beneficiaries_param.max_beneficiaries),
            "publication::create_message: benafic_map.size() > MAX_BENEFICIARIES");

    auto cur_time = static_cast<uint64_t>(eosio::current_time_point().time_since_epoch().count());
    atmsp::machine<fixp_t> machine;

    eosio::check(cur_time >= pool->created, "publication::create_message: cur_time < pool.created");
    eosio::check(pool->state.msgs < std::numeric_limits<structures::counter_t>::max(),
            "publication::create_message: pool->msgs == max_counter_val");
    pools.modify(*pool, eosio::same_payer, [&](auto &item){ item.state.msgs++; });

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

        parent_table.modify(*parent_itr, eosio::same_payer, [&](auto& item) {
            ++item.childcount;
        });

        parent_pk = parent_itr->id;
        level = 1 + parent_itr->level;
    }
    eosio::check(level <= max_comment_depth_param.max_comment_depth, "publication::create_message: level > MAX_COMMENT_DEPTH");
    eosio::check(tokenprop <= pool->rules.maxtokenprop, "tokenprop must not be greater than pool.rules.maxtokenprop");

    message_table.emplace(message_id.author, [&]( auto &item ) {
        item.author = message_id.author;
        item.id = message_pk;
        item.date = cur_time;
        item.pool_date = pool->created;
        item.tokenprop = tokenprop,
        item.beneficiaries = beneficiaries;
        item.rewardweight = config::_100percent;    // can be corrected later in calcrwrdwt
        item.curators_prcnt = *curators_prcnt;
        item.mssg_reward = asset(0, pool->state.funds.symbol);
        item.max_payout = *max_payout;
        item.cashout_time = cur_time + seconds(cashout_window_param.window).count();
    });

    permlink_table.emplace(message_id.author, [&](auto& item) {
        item.id = message_pk;
        item.parentacc = parent_id.author;
        item.parent_id = parent_pk;
        item.value = message_id.permlink;
        item.level = level;
        item.childcount = 0;
    });
}

void publication::addpermlink(structures::mssgid msg, structures::mssgid parent, uint16_t level, uint32_t childcount) {
    require_auth(_self);
    uint64_t parent_pk = 0;
    if (parent.author) {
        eosio::check(parent.permlink.size() > 0, "Parent permlink must not be empty");
        tables::permlink_table tbl(_self, parent.author.value);
        auto idx = tbl.get_index<"byvalue"_n>();
        auto itr = idx.find(parent.permlink);
        eosio::check(itr != idx.end(), "Parent permlink doesn't exist");
        eosio::check(itr->level + 1 == level, "Parent permlink level mismatch");
        eosio::check(itr->childcount > 0, "Parent permlink should have children");
        // Note: can try to also check (itr.childcount <= actual children), but it's hard due scope
        parent_pk = itr->id;
    } else {
        eosio::check(msg.permlink.size() > 0, "Permlink must not be empty");
        eosio::check(msg.permlink.size() < config::max_length, "Permlink must be less than 256 symbols");
        eosio::check(validate_permlink(msg.permlink), "Permlink must only contain 0-9, a-z and _ symbols");
        eosio::check(0 == level, "Root permlink must have 0 level");
        eosio::check(parent.permlink.size() == 0, "Root permlink must have empty parent");
    }
    eosio::check(is_account(msg.author), "Author account must exist");

    tables::permlink_table tbl(_self, msg.author.value);
    auto idx = tbl.get_index<"byvalue"_n>();
    auto itr = idx.find(msg.permlink);
    eosio::check(itr == idx.end(), "Permlink already exists");

    tbl.emplace(_self, [&](auto& pl) {
        pl.id = tbl.available_primary_key();
        pl.parentacc = parent.author;
        pl.parent_id = parent_pk;
        pl.value = msg.permlink;
        pl.level = level;
        pl.childcount = childcount;
    });
}

void publication::delpermlink(structures::mssgid msg) {
    require_auth(_self);
    tables::permlink_table tbl(_self, msg.author.value);
    auto idx = tbl.get_index<"byvalue"_n>();
    auto itr = idx.find(msg.permlink);
    eosio::check(itr != idx.end(), "Permlink doesn't exist");

    tables::message_table message_table(_self, _self.value);
    eosio::check(message_table.find(itr->id) == message_table.end(), "Message exists in cashout window.");

    idx.erase(itr);
}

void publication::addpermlinks(std::vector<structures::permlink_info> permlinks) {
    eosio::check(permlinks.size() > 0, "`permlinks` must not be empty");
    for (const auto& p: permlinks) {
        addpermlink(p.msg, p.parent, p.level, p.childcount);
    }
}

void publication::delpermlinks(std::vector<structures::mssgid> permlinks) {
    eosio::check(permlinks.size() > 0, "`permlinks` must not be empty");
    for (const auto& p: permlinks) {
        delpermlink(p);
    }
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
    auto pool = pools.find(time);

    eosio::check(pool != pools.end(), "publication::get_pool: can't find an appropriate pool");
    return pool;
}

void publication::delete_message(structures::mssgid message_id) {
    require_auth(message_id.author);

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");
    eosio::check(permlink_itr->childcount == 0, "You can't delete comment with child comments.");

    tables::message_table message_table(_self, _self.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    if (mssg_itr != message_table.end()) {
        eosio::check(FP(mssg_itr->state.netshares) <= 0, "Cannot delete a comment with net positive votes.");
        eosio::check((!mssg_itr->closed() || (mssg_itr->mssg_reward.amount == 0)), "Cannot delete comment with unpaid payout");
    }

    if (permlink_itr->parentacc) {
        tables::permlink_table parent_table(_self, permlink_itr->parentacc.value);
        auto parent_itr = parent_table.find(permlink_itr->parent_id);
        eosio::check(parent_itr != parent_table.end(), "Parent permlink doesn't exist.");

        parent_table.modify(*parent_itr, eosio::same_payer, [&](auto& item) {
            --item.childcount;
        });
    }
    permlink_index.erase(permlink_itr);

    if (mssg_itr == message_table.end()) {
        return;
    }
    
    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->pool_date);
    pools.modify(*pool, eosio::same_payer, [&](auto &item){ item.state.msgs--; });

    cancel_deferred((static_cast<uint128_t>(mssg_itr->id) << 64) | message_id.author.value);

    auto remove_id = mssg_itr->id;
    message_table.erase(mssg_itr);
    send_deletevotes_trx(remove_id, message_id.author, message_id.author);
}

void publication::upvote(name voter, structures::mssgid message_id, uint16_t weight) {
    eosio::check(weight > 0, "weight can't be 0.");
    eosio::check(weight <= config::_100percent, "weight can't be more than 100%.");
    set_vote(voter, message_id, weight);
}

void publication::downvote(name voter, structures::mssgid message_id, uint16_t weight) {
    eosio::check(weight > 0, "weight can't be 0.");
    eosio::check(weight <= config::_100percent, "weight can't be more than 100%.");
    set_vote(voter, message_id, -weight);
}

void publication::unvote(name voter, structures::mssgid message_id) {
    set_vote(voter, message_id, 0);
}

void publication::pay_to(std::vector<eosio::token::recipient>&& arg, bool vesting_mode) {
    std::vector<eosio::token::recipient> recipients = std::move(arg);
    if (recipients.empty()) {
        return;
    }
    auto token_symbol = recipients.at(0).quantity.symbol;
    auto token_code = token_symbol.code();

    std::vector<eosio::token::recipient> closed_vesting_payouts;

    for (auto& recipient_obj : recipients) {
        eosio::check(token_symbol == recipient_obj.quantity.symbol, "publication::pay_to: different tokens in one payment");
        eosio::check(recipient_obj.quantity.amount > 0, "publication::pay_to: quantity.amount <= 0");

        if (vesting_mode) {
            if (golos::vesting::balance_exist(config::vesting_name, recipient_obj.to, token_code)) {
                recipient_obj.memo = config::send_prefix + recipient_obj.to.to_string() + "; " + recipient_obj.memo;
                recipient_obj.to = config::vesting_name;
            }
            else {
                recipient_obj.memo = std::string("unsent vesting for ") + recipient_obj.to.to_string() + "; " + recipient_obj.memo;
                recipient_obj.to = _self;
                closed_vesting_payouts.push_back(recipient_obj);
                recipient_obj.to = name();
            }
        }
        else if (!token::balance_exist(config::token_name, recipient_obj.to, token_code)) {
            recipient_obj.memo = std::string("unsent tokens for ") + recipient_obj.to.to_string() + "; " + recipient_obj.memo;
            recipient_obj.to = _self;
        }
    }

    if (vesting_mode) {
        recipients.erase(
            std::remove_if(recipients.begin(), recipients.end(), [](const eosio::token::recipient& r) { return r.to == name(); }),
            recipients.end());

        if (!recipients.empty()) {
            INLINE_ACTION_SENDER(token, bulktransfer) (config::token_name, {_self, config::code_name}, {_self, recipients});
        }
        if (!closed_vesting_payouts.empty()) {
            INLINE_ACTION_SENDER(token, bulkpayment) (config::token_name, {_self, config::code_name}, {_self, closed_vesting_payouts});
        }
    }
    else {
        INLINE_ACTION_SENDER(token, bulkpayment) (config::token_name, {_self, config::code_name}, {_self, recipients});
    }
}

std::pair<int64_t, bool> publication::fill_curator_payouts(
        std::vector<eosio::token::recipient>& payouts,
        structures::mssgid message_id,
        uint64_t msgid,
        int64_t max_rewards,
        fixp_t weights_sum,
        symbol tokensymbol,
        std::string memo)
{
    tables::vote_table vs(_self, message_id.author.value);
    int64_t paid_sum = 0;
    bool there_is_a_place = true;
    if ((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
        auto idx = vs.get_index<"messageid"_n>();
        auto v = idx.lower_bound(std::make_tuple(msgid, INT64_MAX));
        while ((v != idx.end()) && (v->message_id == msgid) && there_is_a_place) {
            auto claim = int_cast(elai_t(max_rewards) * elaf_t(elap_t(FP(v->curatorsw)) / elap_t(weights_sum)));
            if (claim == 0) {
                break;
            }
            if (payouts.size() >= config::target_payments_per_trx) {
                there_is_a_place = false;
            }
            int64_t now_paid = 0;
            size_t to_del = 0;
            for (auto d = v->delegators.rbegin(); (d != v->delegators.rend()) && (payouts.size() < config::target_payments_per_trx); d++) {
                auto dlg_payout = cyber::safe_pct(claim, d->interest_rate);
                if (dlg_payout > 0) {
                    payouts.push_back({d->delegator, eosio::asset(dlg_payout, tokensymbol), get_memo("delegator", message_id)});
                    now_paid += dlg_payout;
                }
                ++to_del;
            }
            eosio::check(now_paid + v->paid_amount <= claim, "LOGIC ERROR! fill_curator_payouts: wrong paid amount");
            auto to_pay = claim - (now_paid + v->paid_amount);

            if (payouts.size() < config::target_payments_per_trx || !to_pay) {
                if (to_pay) {
                    payouts.push_back({v->voter, eosio::asset(to_pay, tokensymbol), memo});
                    now_paid += to_pay;
                }
                auto& vote = *v;
                ++v;
                vs.erase(vote);
            }
            else {
                there_is_a_place = false;
                idx.modify(v, eosio::same_payer, [&](auto &item) {
                    item.delegators.resize(item.delegators.size() - to_del);
                    item.paid_amount += now_paid;
                });
            }
            paid_sum += now_paid;
        }
    }
    return std::make_pair(paid_sum, there_is_a_place);
}

int16_t publication::use_charge(tables::limit_table& lims, structures::limitparams::act_t act, name issuer,
                            name account, int64_t eff_vesting, symbol_code token_code, bool vestpayment, int16_t weight) {
    auto lim_itr = lims.find(act);
    eosio::check(lim_itr != lims.end(), "publication::use_charge: limit parameters not set");
    eosio::check(eff_vesting >= lim_itr->min_vesting, "insufficient effective vesting amount");

    const auto price = lim_itr->price;
    int16_t k = 1;
    if (act == structures::limitparams::VOTE) {
        k = 200;    // TODO: fix wrong default
        if (price > 0) {
            k = config::_100percent / price;
        }

        auto current_power = charge::get_current_value(config::charge_name, account, token_code, lim_itr->charge_id);
        int64_t charge = config::_100percent - current_power;
        eosio::check(charge >= config::_1percent, "too low voting power");
        weight = static_cast<int16_t>((abs(weight) * charge / config::_100percent + k - 1) / k);
    }

    if (price >= 0) {
        INLINE_ACTION_SENDER(charge, use) (config::charge_name,
            {issuer, config::invoice_name}, {
                account,
                token_code,
                lim_itr->charge_id,
                price * abs(weight)*k / config::_100percent,
                lim_itr->cutoff,
                vestpayment ? lim_itr->vesting_price * abs(weight)*k / config::_100percent : 0
            }
        );
    }
    return weight;      // vote only
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

void publication::providebw_for_trx(transaction& trx, const permission_level& provider) {
    if (provider.actor != name()) {
        trx.actions.emplace_back(action{provider, "cyber"_n, "providebw"_n, std::make_tuple(provider.actor, _self)});
    }
}

void publication::send_postreward_trx(uint64_t id, const structures::mssgid& message_id, const name payer, const permission_level& provider) {
    transaction trx(eosio::current_time_point() + eosio::seconds(config::paymssgrwrd_expiration_sec));
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "paymssgrwrd"_n, message_id});
    providebw_for_trx(trx, provider);
    trx.delay_sec = 0;
    trx.send((static_cast<uint128_t>(id) << 64) | message_id.author.value, payer);
}

void publication::send_deletevotes_trx(int64_t message_id, name author, name payer) {
    transaction trx(eosio::current_time_point() + eosio::seconds(config::deletevotes_expiration_sec));
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "deletevotes"_n, std::make_tuple(message_id, author)});
    providebw_for_trx(trx, params().bwprovider_param.provider);
    trx.delay_sec = 0;
    trx.send((static_cast<uint128_t>(message_id) << 64) | author.value, payer);
}

void publication::close_messages(name payer) {
    auto cur_time = static_cast<uint64_t>(eosio::current_time_point().time_since_epoch().count());
    auto provider = params().bwprovider_param.provider;

    tables::reward_pools pools_table(_self, _self.value);
    std::map<uint64_t, structures::rewardpool> pools;

    size_t i = 0;
    tables::message_table message_table(_self, _self.value);
    auto message_index = message_table.get_index<"bycashout"_n>();
    for (auto mssg_itr = message_index.begin(); mssg_itr != message_index.end() && mssg_itr->cashout_time <= cur_time; ++mssg_itr) {
        if (i++ >= config::max_closed_posts_per_action) {
            transaction trx(eosio::current_time_point() + eosio::seconds(config::closemssgs_expiration_sec));
            trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "closemssgs"_n, std::make_tuple(_self)});
            providebw_for_trx(trx, provider);
            trx.delay_sec = 0;
            trx.send(static_cast<uint128_t>(config::closemssgs_sender_id) << 64, payer, true);
            break;
        }

        auto pool_kv = pools.find(mssg_itr->pool_date);
        if (pool_kv == pools.end()) {
            pool_kv = pools.emplace(mssg_itr->pool_date, *get_pool(pools_table, mssg_itr->pool_date)).first;
        }
        auto& pool = pool_kv->second;

        eosio::check(pool.state.msgs != 0, "LOGIC ERROR! publication::payrewards: pool.msgs is equal to zero");
        atmsp::machine<fixp_t> machine;
        fixp_t sharesfn = set_and_run(
                machine,
                pool.rules.mainfunc.code,
                {FP(mssg_itr->state.netshares)},
                {{fixp_t(0), FP(pool.rules.mainfunc.maxarg)}});

        asset mssg_reward;
        auto& state = pool.state;
        int64_t payout = 0;
        if(state.msgs == 1) {
            payout = state.funds.amount; //if we have the only message in the pool, the author receives a reward anyway
            eosio::check(state.rshares == mssg_itr->state.netshares,
                    "LOGIC ERROR! publication::payrewards: pool.rshares != mssg_itr->netshares for last message");
            eosio::check(state.rsharesfn == sharesfn.data(),
                    "LOGIC ERROR! publication::payrewards: pool.rsharesfn != sharesfn.data() for last message");
            state.funds.amount = 0;
            state.rshares = 0;
            state.rsharesfn = 0;
        }
        else {
            auto total_rsharesfn = WP(state.rsharesfn);

            if(sharesfn > fixp_t(0)) {
                eosio::check(total_rsharesfn > 0, "LOGIC ERROR! publication::payrewards: total_rshares_fn <= 0");

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

            eosio::check(new_rsharesfn >= 0, "LOGIC ERROR! publication::payrewards: new_rsharesfn < 0");

            state.rshares = new_rshares.data();
            state.rsharesfn = new_rsharesfn.data();
        }

        mssg_reward = asset(payout, state.funds.symbol);

        message_index.modify(mssg_itr, eosio::same_payer, [&]( auto &item ) {
                item.cashout_time = microseconds::maximum().count();
                item.mssg_reward = mssg_reward;
            });

        state.msgs--;

        tables::permlink_table permlink_table(_self, mssg_itr->author.value);
        auto permlink_itr = permlink_table.find(mssg_itr->id);
        send_postreward_trx(mssg_itr->id, structures::mssgid{mssg_itr->author, permlink_itr->value}, payer, provider);
    }

    for (auto& pool_kv : pools) {
        auto pool = pools_table.find(pool_kv.first);
        bool pool_erased = false;
        if(pool_kv.second.state.msgs == 0) {
            auto etr = pools_table.end();
            --etr;
            if(pool != etr) {
                send_poolerase_event(*pool);

                pools_table.erase(pool);
                pool_erased = true;
            }
        }
        if(!pool_erased)
            pools_table.modify(pool, eosio::same_payer, [&](auto &item) {
                item.state = pool_kv.second.state;
                send_poolstate_event(item);
            });
    }
}

void publication::deletevotes(int64_t message_id, name author) {
    require_auth(_self);

    tables::vote_table vote_table(_self, author.value);
    auto votetable_index = vote_table.get_index<"messageid"_n>();
    auto vote_itr = votetable_index.lower_bound(std::make_tuple(message_id, INT64_MAX));
    size_t i = 0;
    for (auto vote_etr = votetable_index.end(); vote_itr != vote_etr;) {
        if (config::max_deletions_per_trx <= i++) {
            break;
        }
        auto& vote = *vote_itr;
        ++vote_itr;
        if (vote.message_id != message_id) {
            vote_itr = votetable_index.end();
            break;
        }
        vote_table.erase(vote);
    }

    if (vote_itr != votetable_index.end()) {
        send_deletevotes_trx(message_id, author, _self);
    }
}

void publication::paymssgrwrd(structures::mssgid message_id) {
    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    // rare case - not critical for performance
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    tables::message_table message_table(_self, _self.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    eosio::check(mssg_itr != message_table.end(), "Message doesn't exist in cashout window.");
    eosio::check(mssg_itr->closed(), "Message doesn't closed.");

    auto payout = mssg_itr->mssg_reward;
    tables::reward_pools pools(_self, _self.value);

    elaf_t percent(elai_t(mssg_itr->curators_prcnt) / elai_t(config::_100percent));
    auto curation_payout = int_cast(percent * elai_t(payout.amount));

    eosio::check((curation_payout <= payout.amount) && (curation_payout >= 0),
            "publication::payrewards: wrong curation_payout");

    std::vector<eosio::token::recipient> vesting_payouts;
    bool completed = false;
    int64_t paid = 0;
    std::tie(paid, completed) = fill_curator_payouts(
            vesting_payouts,
            message_id,
            mssg_itr->id,
            curation_payout,
            FP(mssg_itr->state.sumcuratorsw),
            payout.symbol,
            get_memo("curators", message_id));

    auto max_add_payouts = mssg_itr->beneficiaries.size() + 2; //beneficiaries, author tokens and author vesting
    bool last_payment = completed && ((vesting_payouts.size() + max_add_payouts <= config::max_payments_per_trx) || vesting_payouts.empty());

    if (last_payment) {
        auto actual_curation_payout = paid + mssg_itr->paid_amount;
        auto unclaimed_rewards = curation_payout - actual_curation_payout;
        eosio::check(unclaimed_rewards >= 0, "publication::payrewards: unclaimed_rewards < 0");

        payout.amount -= curation_payout;

        int64_t ben_payout_sum = 0;
        for (auto& ben: mssg_itr->beneficiaries) {
            auto ben_payout = cyber::safe_pct(payout.amount, ben.weight);
            eosio::check((0 <= ben_payout) && (ben_payout <= payout.amount - ben_payout_sum),
                    "LOGIC ERROR! publication::payrewards: wrong ben_payout value");
            if (ben_payout > 0) {
                vesting_payouts.push_back({ben.account, eosio::asset(ben_payout, payout.symbol), get_memo("benefeciary", message_id)});
                ben_payout_sum += ben_payout;
            }
        }

        payout.amount -= ben_payout_sum;
        auto token_payout = cyber::safe_pct(mssg_itr->tokenprop, payout.amount);
        eosio::check(payout.amount >= token_payout, "publication::payrewards: wrong token_payout value");
        if (token_payout > 0) {
            pay_to({{message_id.author, eosio::asset(token_payout, payout.symbol), get_memo("author", message_id)}}, false);
        }
        if (payout.amount - token_payout > 0) {
            vesting_payouts.push_back({message_id.author, eosio::asset(payout.amount - token_payout, payout.symbol), get_memo("author", message_id)});
        }
        pay_to(std::move(vesting_payouts), true);

        auto remove_id = mssg_itr->id;
        message_table.erase(mssg_itr);

        fill_depleted_pool(pools, asset(unclaimed_rewards, payout.symbol), pools.end());

        send_postreward_event(message_id, payout, asset(ben_payout_sum, payout.symbol), asset(actual_curation_payout, payout.symbol), asset(unclaimed_rewards, payout.symbol));
        send_deletevotes_trx(remove_id, message_id.author, _self);
    }
    else {
        pay_to(std::move(vesting_payouts), true);
        message_table.modify(mssg_itr, eosio::same_payer, [&]( auto &item ) { item.paid_amount += paid; });
        send_postreward_trx(mssg_itr->id, message_id, _self, params().bwprovider_param.provider);
    }
}

void publication::check_upvote_time(uint64_t cur_time, uint64_t mssg_date) {
    const auto& cashout_window_param = params().cashout_window_param;
    eosio::check(
        (cur_time <= mssg_date + ((cashout_window_param.window - cashout_window_param.upvote_lockout) * seconds(1).count())) ||
        (cur_time > mssg_date + (cashout_window_param.window * seconds(1).count())),
        "You can't upvote, because publication will be closed soon.");
}

fixp_t publication::calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool) {
    tables::limit_table lims(_self, _self.value);
    auto token_code = pool.state.funds.symbol.code();
    int64_t eff_vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, voter, token_code).amount;
    auto used_power = use_charge(lims, structures::limitparams::VOTE, token::get_issuer(config::token_name, token_code),
        voter, eff_vesting, token_code, false, weight);
    fixp_t abs_rshares = FP(eff_vesting) * elaf_t(elai_t(used_power) / elai_t(config::_100percent));
    eosio::check(!weight || abs_rshares >= FP(params().min_abs_rshares_param.value), "too low vote weight");
    return (weight < 0) ? -abs_rshares : abs_rshares;
}

void publication::set_vote(name voter, const structures::mssgid& message_id, int16_t weight) {
    require_auth(voter);
    close_messages(voter);

    auto get_calc_sharesfn = [&](auto mainfunc_code, auto netshares, auto mainfunc_maxarg) {
        atmsp::machine<fixp_t> machine;
        return set_and_run(machine, mainfunc_code, {netshares}, {{fixp_t(0), mainfunc_maxarg}});
    };

    const auto& max_vote_changes_param = params().max_vote_changes_param;

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(), "Permlink doesn't exist.");

    tables::message_table message_table(_self, _self.value);
    auto mssg_itr = message_table.find(permlink_itr->id);
    eosio::check(mssg_itr != message_table.end(), "Message doesn't exist in cashout window.");
    eosio::check(!mssg_itr->closed(), "Message is closed.");

    tables::reward_pools pools(_self, _self.value);
    auto pool = get_pool(pools, mssg_itr->pool_date);
    check_acc_vest_balance(voter, pool->state.funds.symbol);
    tables::vote_table vote_table(_self, message_id.author.value);

    auto cur_time = static_cast<uint64_t>(eosio::current_time_point().time_since_epoch().count());

    auto votetable_index = vote_table.get_index<"byvoter"_n>();
    auto vote_itr = votetable_index.find(std::make_tuple(mssg_itr->id, voter));
    if (vote_itr != votetable_index.end()) {
        eosio::check(weight != vote_itr->weight, "Vote with the same weight has already existed.");
        eosio::check(vote_itr->count != max_vote_changes_param.max_vote_changes, "You can't revote anymore.");

        atmsp::machine<fixp_t> machine;
        fixp_t rshares = calc_available_rshares(voter, weight, cur_time, *pool);
        if(rshares > FP(vote_itr->rshares))
            check_upvote_time(cur_time, mssg_itr->date);

        fixp_t new_mssg_rshares = add_cut(FP(mssg_itr->state.netshares) - FP(vote_itr->rshares), rshares);
        auto rsharesfn_delta = get_delta(machine, FP(mssg_itr->state.netshares), new_mssg_rshares, pool->rules.mainfunc);

        pools.modify(*pool, eosio::same_payer, [&](auto &item) {
            item.state.rshares = ((WP(item.state.rshares) - wdfp_t(FP(vote_itr->rshares))) + wdfp_t(rshares)).data();
            item.state.rsharesfn = (WP(item.state.rsharesfn) + wdfp_t(rsharesfn_delta)).data();
        });
        send_poolstate_event(*pool);

        message_table.modify(mssg_itr, eosio::same_payer, [&]( auto &item ) {
            item.state.netshares = new_mssg_rshares.data();
            item.state.sumcuratorsw = (FP(item.state.sumcuratorsw) - FP(vote_itr->curatorsw)).data();
        });
        send_poststate_event(
            message_id.author,
            *permlink_itr,
            *mssg_itr,
            get_calc_sharesfn(
                pool->rules.mainfunc.code,
                FP(new_mssg_rshares.data()),
                FP(pool->rules.mainfunc.maxarg)
            )
        );

        votetable_index.modify(vote_itr, eosio::same_payer, [&]( auto &item ) {
            item.weight = weight;
            item.time = cur_time;
            item.curatorsw = fixp_t(0).data();
            item.rshares = rshares.data();
            ++item.count;
        });
        send_votestate_event(voter, *vote_itr, message_id.author, *permlink_itr);

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

    pools.modify(*pool, eosio::same_payer, [&](auto &item) {
         item.state.rshares = (WP(item.state.rshares) + wdfp_t(rshares)).data();
         item.state.rsharesfn = (WP(item.state.rsharesfn) + wdfp_t(rsharesfn_delta)).data();
    });
    send_poolstate_event(*pool);
    eosio::check(WP(pool->state.rsharesfn) >= 0, "pool state rsharesfn overflow");

    auto sumcuratorsw_delta = get_delta(
            machine,
            FP(mssg_itr->state.voteshares),
            FP(msg_new_state.voteshares),
            pool->rules.curationfunc);
    msg_new_state.sumcuratorsw = (FP(mssg_itr->state.sumcuratorsw) + sumcuratorsw_delta).data();
    message_table.modify(mssg_itr, eosio::same_payer, [&](auto &item) {
        item.state = msg_new_state;
    });
    send_poststate_event(
        message_id.author,
        *permlink_itr,
        *mssg_itr,
        get_calc_sharesfn(
            pool->rules.mainfunc.code,
            FP(msg_new_state.netshares),
            FP(pool->rules.mainfunc.maxarg)
        )
    );

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
    eosio::check(quantity.amount >= 0, "fill_depleted_pool: quantity.amount < 0");
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
        pools.modify(*choice, eosio::same_payer, [&](auto &item){
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

    close_messages(_self);
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
#   ifdef DISABLE_CHARGE_VESTING
    eosio::check(vesting_price == 0 && min_vesting == 0, "set_limit: vesting disabled in charge");
#   endif
    eosio::check(
            price < 0 ||
            golos::charge::exist(config::charge_name, token_code, charge_id),
            "publication::set_limit: charge doesn't exist");
    auto act = limitparams::act_from_str(act_str);
    eosio::check(act != limitparams::VOTE || charge_id == 0, "publication::set_limit: charge_id for VOTE should be 0");
    //? should we require cutoff to be _100percent if act == VOTE (to synchronize with vesting)?
    eosio::check(act != limitparams::POSTBW || min_vesting == 0, "publication::set_limit: min_vesting for POSTBW should be 0");
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

// TODO: move maxtokenprop to setparams #828
void publication::set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
    uint16_t maxtokenprop, eosio::symbol tokensymbol
) {
    eosio::check(tokensymbol == token::get_supply(config::token_name, tokensymbol.code()).symbol, "symbol precision mismatch");
    validate_percent(maxtokenprop, "maxtokenprop");
    //TODO: machine's constants
    using namespace tables;
    using namespace structures;
    require_auth(_self);
    reward_pools pools(_self, _self.value);
    uint64_t created = static_cast<uint64_t>(eosio::current_time_point().time_since_epoch().count());

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
    eosio::check(pools.find(created) == pools.end(), "rules with this key already exist");

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
        post.state.sumcuratorsw, sharesfn.data() };
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

void publication::send_postreward_event(const structures::mssgid& message_id, const asset& author, const asset& benefactor, const asset& curator, const asset& unclaimed) {
    structures::post_reward_event data{message_id, author, benefactor, curator, unclaimed};
    eosio::event(_self, "postreward"_n, data).send();
}

structures::funcinfo publication::load_func(
        const funcparams& params,
        const std::string& name,
        const atmsp::parser<fixp_t>& pa,
        atmsp::machine<fixp_t>& machine,
        bool inc)
{
    eosio::check(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
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
    eosio::check(golos::vesting::balance_exist(config::vesting_name, user, tokensymbol.code()),
        ("vesting balance doesn't exist for " + name{user}.to_string()).c_str());
}

void publication::set_params(std::vector<posting_params> params) {
    require_auth(_self);
    posting_params_singleton cfg(_self, _self.value);
    param_helper::check_params(params, cfg.exists());
    auto setter = param_helper::set_parameters<posting_params_setter>(params, cfg, _self);
    if (setter.new_bwprovider) {
        auto provider = setter.new_bwprovider->provider;
        if (provider.actor != name()) {
            dispatch_inline("cyber"_n, "providebw"_n, {provider}, std::make_tuple(provider.actor, _self));
        }
    }
}

void publication::reblog(name rebloger, structures::mssgid message_id, std::string headermssg, std::string bodymssg) {
    require_auth(rebloger);

    eosio::check(rebloger != message_id.author, "You cannot reblog your own content.");
    eosio::check(headermssg.length() < config::max_length, "Title length is more than 256.");
    eosio::check(
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
    eosio::check(rebloger != message_id.author, "You cannot erase reblog your own content.");

    tables::permlink_table permlink_table(_self, message_id.author.value);
    auto permlink_index = permlink_table.get_index<"byvalue"_n>();
    auto permlink_itr = permlink_index.find(message_id.permlink);
    eosio::check(permlink_itr != permlink_index.end(),
                 "You can't erase reblog, because this message doesn't exist.");
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

    tables::message_table message_table(_self, _self.value);
    const auto& mssg = get_message(message_table, message_id);

    eosio::check(mssg.state.voteshares == 0, "Curators percent can be changed only before voting.");
    eosio::check(
            mssg.curators_prcnt != curators_prcnt,
            "Same curators percent value is already set."); // TODO: tests #617

    message_table.modify(mssg, eosio::same_payer, [&](auto& item) {
        item.curators_prcnt = curators_prcnt;
    });
}

void publication::set_max_payout(structures::mssgid message_id, asset max_payout) {
    require_auth(message_id.author);

    tables::message_table message_table(_self, _self.value);
    const auto& mssg = get_message(message_table, message_id);

    eosio::check(mssg.state.voteshares == 0, "Max payout can be changed only before voting.");

    eosio::check(max_payout != mssg.max_payout, "Same max payout is already set.");
    eosio::check(max_payout >= asset(0, mssg.max_payout.symbol), "max_payout must not be negative.");
    eosio::check(max_payout < mssg.max_payout, "Cannot replace max payout with greater one.");

    message_table.modify(mssg, eosio::same_payer, [&](auto& item) {
        item.max_payout = max_payout;
    });
}

void publication::calcrwrdwt(name account, int64_t mssg_id, int64_t post_charge) {
    require_auth(_self);
    tables::limit_table lims(_self, _self.value);
    auto bw_lim_itr = lims.find(structures::limitparams::POSTBW);
    eosio::check(bw_lim_itr != lims.end(), "publication::calc_reward_weight: limit parameters not set");
    if (post_charge > bw_lim_itr->cutoff) {
        elaf_t weight(elai_t(bw_lim_itr->cutoff * bw_lim_itr->cutoff) / elai_t(post_charge * post_charge));
        auto reward_weight = int_cast(elai_t(config::_100percent) * weight);
        validate_percent(reward_weight, "calculated reward weight");        // should never fail in normal conditions

        tables::message_table message_table(_self, _self.value);
        auto message_itr = message_table.find(mssg_id);
        eosio::check(message_itr != message_table.end(), "Message doesn't exist in cashout window.");
        message_table.modify(message_itr, eosio::same_payer, [&]( auto &item ) {
            item.rewardweight = reward_weight;
        });

        tables::permlink_table permlink_table(_self, account.value);
        auto permlink_itr = permlink_table.find(mssg_id);
        eosio::check(permlink_itr != permlink_table.end(), "Permlink doesn't exist.");

        send_rewardweight_event(structures::mssgid{account, permlink_itr->value}, reward_weight);
    }
}

void publication::syncpool(std::optional<symbol> tokensymbol) {
    require_auth(_self);
    
    symbol token_symbol;
    {
        tables::reward_pools pools(_self, _self.value);
        eosio::check(pools.begin() != pools.end(), "nothing to sync");
        auto last_pool = pools.end();
        --last_pool;
        
        // support for multiple tokens is actually redundant in this contract.
        // perhaps it makes sense to remove this logic.
        token_symbol = tokensymbol.value_or(last_pool->state.funds.symbol);

        auto date_of_oldest_mssg = last_pool->created;
        tables::message_table message_table(_self, _self.value);
        auto oldest_mssg_itr = message_table.begin();
        if (oldest_mssg_itr != message_table.end() && oldest_mssg_itr->pool_date < date_of_oldest_mssg) {
            date_of_oldest_mssg = oldest_mssg_itr->pool_date;
        }
        auto pool = pools.begin();
        while (pool->created < date_of_oldest_mssg) {
            send_poolerase_event(*pool);
            pool = pools.erase(pool);
            eosio::check(pool != pools.end(), "SYSTEM: no pools left");
        }
    } //? invalidation of pools after erasing ?
    tables::reward_pools pools(_self, _self.value);
    
    wide_t rshares_sum = 0;
    for (auto pool = pools.begin(); pool != pools.end(); ++pool) {
        if (token_symbol == pool->state.funds.symbol) {
            rshares_sum += pool->state.rshares;
        }
    }
    
    int64_t total_amount = token::get_balance(config::token_name, _self, token_symbol.code()).amount;
    int64_t left_amount = total_amount;
    for (auto pool = pools.begin(); pool != pools.end(); ++pool) {
        if (token_symbol == pool->state.funds.symbol) {
            pools.modify(pool, eosio::same_payer, [&](auto &item) {
                auto cur_amount = safe_prop_from_wide(total_amount, item.state.rshares, rshares_sum);
                    
                item.state.funds.amount = cur_amount;
                left_amount -= cur_amount;
                send_poolstate_event(item);
            });
        }
    }
    
    eosio::check(left_amount >= 0, "SYSTEM: incorrect left_amount");
    if (left_amount > 0) {
         pools.modify(pools.begin(), eosio::same_payer, [&](auto &item) {
            item.state.funds.amount += left_amount;
            send_poolstate_event(item);
        });
    }
}

} // golos
