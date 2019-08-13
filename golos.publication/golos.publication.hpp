#pragma once
#include "objects.hpp"
#include "parameters.hpp"
#include <eosio/transaction.hpp>

namespace golos {

using namespace eosio;

class publication : public contract {
public:
    using contract::contract;

    void set_limit(std::string act, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff, int64_t vesting_price, int64_t min_vesting);
    void set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
        uint16_t maxtokenprop, symbol tokensymbol);
    void on_transfer(name from, name to, asset quantity, std::string memo = "");
    void create_message(structures::mssgid message_id, structures::mssgid parent_id,
        std::vector<structures::beneficiary> beneficiaries, uint16_t tokenprop, bool vestpayment,
        std::string headermssg, std::string bodymssg, std::string languagemssg, std::vector<std::string> tags,
        std::string jsonmetadata, std::optional<uint16_t> curators_prcnt, std::optional<asset> max_payout);
    void update_message(structures::mssgid message_id, std::string headermssg, std::string bodymssg,
                        std::string languagemssg, std::vector<std::string> tags, std::string jsonmetadata);
    void delete_message(structures::mssgid message_id);
    void upvote(name voter, structures::mssgid message_id, uint16_t weight);
    void downvote(name voter, structures::mssgid message_id, uint16_t weight);
    void unvote(name voter, structures::mssgid message_id);
    void close_messages(name payer);
    void set_params(std::vector<posting_params> params);
    void reblog(name rebloger, structures::mssgid message_id, std::string headermssg, std::string bodymssg);
    void erase_reblog(name rebloger, structures::mssgid message_id);
    void set_curators_prcnt(structures::mssgid message_id, uint16_t curators_prcnt);
    void set_max_payout(structures::mssgid message_id, asset max_payout);
    void calcrwrdwt(name account, int64_t mssg_id, int64_t post_charge);
    void paymssgrwrd(structures::mssgid message_id);
    void deletevotes(int64_t message_id, name author);
private:
    const posting_state& params();
    void set_vote(name voter, const structures::mssgid &message_id, int16_t weight);
    void fill_depleted_pool(tables::reward_pools& pools, asset quantity,
        tables::reward_pools::const_iterator excluded);
    auto get_pool(tables::reward_pools& pools, uint64_t time);
    std::pair<int64_t, bool> fill_curator_payouts(std::vector<eosio::token::recipient>& payouts,
                         structures::mssgid message_id, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum,
                         symbol tokensymbol, std::string memo = "");
    void pay_to(std::vector<eosio::token::recipient>&& recipients, bool vesting_mode);
    static void check_acc_vest_balance(name user, symbol tokensymbol);
    base_t get_checked_curators_prcnt(std::optional<uint16_t> curators_prcnt);

    void send_poolstate_event(const structures::rewardpool& pool);
    void send_poolerase_event(const structures::rewardpool& pool);
    void send_poststate_event(name author, const structures::permlink& permlink, const structures::message& post, fixp_t sharesfn);
    void send_votestate_event(name voter, const structures::voteinfo& vote, name author, const structures::permlink& permlink);
    void send_rewardweight_event(structures::mssgid message_id, uint16_t weight);
    void send_postreward_event(const structures::mssgid& message_id, const asset& author, const asset& benefactor, const asset& curator, const asset& unclaimed);

    static structures::funcinfo load_func(const funcparams& params, const std::string& name,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc);
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val,
        const structures::funcinfo& func);

    int16_t use_charge(tables::limit_table& lims, structures::limitparams::act_t act, name issuer,
                        name account, int64_t eff_vesting, symbol_code token_code, bool vestpayment, int16_t weight = config::_100percent);
    void use_postbw_charge(tables::limit_table& lims, name issuer, name account, symbol_code token_code, int64_t mssg_id);

    fixp_t calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool);
    void check_upvote_time(uint64_t cur_time, uint64_t mssg_date);
    static bool validate_permlink(std::string permlink);

    std::string get_memo(const std::string &type, const structures::mssgid &message_id);
    const auto& get_message(const tables::message_table& messages, const structures::mssgid& message_id);
    void providebw_for_trx(eosio::transaction& trx, const permission_level& provider);
    void send_postreward_trx(uint64_t id, const structures::mssgid& message_id, const name payer, const permission_level& provider);
    void send_deletevotes_trx(int64_t message_id, name author, name payer);
};

} // golos
