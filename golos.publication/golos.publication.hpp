#pragma once
#include "objects.hpp"
#include "parameters.hpp"

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
        std::string jsonmetadata, std::optional<uint16_t> curators_prcnt);
    void update_message(structures::mssgid message_id, std::string headermssg, std::string bodymssg,
                        std::string languagemssg, std::vector<std::string> tags, std::string jsonmetadata);
    void delete_message(structures::mssgid message_id);
    void upvote(name voter, structures::mssgid message_id, uint16_t weight);
    void downvote(name voter, structures::mssgid message_id, uint16_t weight);
    void unvote(name voter, structures::mssgid message_id);
    void close_message(structures::mssgid message_id);
    void set_params(std::vector<posting_params> params);
    void reblog(name rebloger, structures::mssgid message_id);
    void set_curators_prcnt(structures::mssgid message_id, uint16_t curators_prcnt);
    void calcrwrdwt(name account, int64_t mssg_id, int64_t post_charge);

private:
    const posting_state& params();
    void close_message_timer(structures::mssgid message_id, uint64_t id, uint64_t delay_sec);
    void set_vote(name voter, const structures::mssgid &message_id, int16_t weight);
    void fill_depleted_pool(tables::reward_pools& pools, asset quantity,
        tables::reward_pools::const_iterator excluded);
    auto get_pool(tables::reward_pools& pools, uint64_t time);
    int64_t pay_curators(name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum,
                         symbol tokensymbol, std::string memo = "");
    void payto(name user, asset quantity, enum_t mode, std::string memo = "");
    static void check_account(name user, symbol tokensymbol);
    int64_t pay_delegators(int64_t claim, name voter,
            eosio::symbol tokensymbol, std::vector<structures::delegate_voter> delegate_list);
    base_t get_checked_curators_prcnt(std::optional<uint16_t> curators_prcnt);

    void send_poolstate_event(const structures::rewardpool& pool);
    void send_poolerase_event(const structures::rewardpool& pool);
    void send_poststate_event(name author, const structures::message& post, fixp_t sharesfn);
    void send_votestate_event(name voter, const structures::voteinfo& vote, name author, const structures::message& post);
    void send_rewardweight_event(structures::mssgid message_id, uint16_t weight);

    static structures::funcinfo load_func(const funcparams& params, const std::string& name,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc);
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val,
        const structures::funcinfo& func);

    void use_charge(tables::limit_table& lims, structures::limitparams::act_t act, name issuer,
                        name account, int64_t eff_vesting, symbol_code token_code, bool vestpayment, elaf_t weight = elaf_t(1));
    void use_postbw_charge(tables::limit_table& lims, name issuer, name account, symbol_code token_code, int64_t mssg_id);

    fixp_t calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool);
    void check_upvote_time(uint64_t cur_time, uint64_t mssg_date);
    static bool validate_permlink(std::string permlink);

    std::string get_memo(const std::string &type, const structures::mssgid &message_id);
};

} // golos
