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
        int64_t curatorsprop, int64_t maxtokenprop, symbol tokensymbol);
    void on_transfer(name from, name to, asset quantity, std::string memo);
    void create_message(name account, std::string permlink, name parentacc, std::string parentprmlnk,
        std::vector<structures::beneficiary> beneficiaries, int64_t tokenprop, bool vestpayment,
        std::string headermssg, std::string bodymssg, std::string languagemssg, std::vector<structures::tag> tags,
        std::string jsonmetadata);
    void update_message(name account, std::string permlink,
        std::string headermssg, std::string bodymssg, std::string languagemssg, std::vector<structures::tag> tags,
        std::string jsonmetadata);
    void delete_message(name account, std::string permlink);
    void upvote(name voter, name author, std::string permlink, uint16_t weight);
    void downvote(name voter, name author, std::string permlink, uint16_t weight);
    void unvote(name voter, name author, std::string permlink);
    void close_message(name account, uint64_t id);
    void set_params(std::vector<posting_params> params);
    void reblog(name rebloger, name author, std::string permlink);
private:
    void close_message_timer(name account, uint64_t id, uint64_t delay_sec);
    void set_vote(name voter, name author, std::string permlink, int16_t weight);
    uint16_t notify_parent(bool increase, name parentacc, uint64_t parent_id);
    void fill_depleted_pool(tables::reward_pools& pools, asset quantity,
        tables::reward_pools::const_iterator excluded);
    auto get_pool(tables::reward_pools& pools, uint64_t time);
    int64_t pay_curators(name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum,
        symbol tokensymbol);
    void payto(name user, asset quantity, enum_t mode);
    void check_account(name user, symbol tokensymbol);
    int64_t pay_delegators(int64_t claim, name voter, 
            eosio::symbol tokensymbol, std::vector<structures::delegate_voter> delegate_list); 

    void send_poolstate_event(const structures::rewardpool& pool);
    void send_poolerase_event(const structures::rewardpool& pool);
    void send_poststate_event(name author, const structures::message& post);
    void send_postclose_event(name author, const structures::message& post);
    void send_votestate_event(name voter, const structures::voteinfo& vote, name author, const structures::message& post);

    static structures::funcinfo load_func(const funcparams& params, const std::string& name,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc);
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val,
        const structures::funcinfo& func);
        
    void remove_postbw_charge(name account, symbol_code token_code, int64_t mssg_id, elaf_t* reward_weight_ptr = nullptr);
    void use_charge(tables::limit_table& lims, structures::limitparams::act_t act, name issuer,
                        name account, int64_t eff_vesting, symbol_code token_code, bool vestpayment, elaf_t weight = elaf_t(1));
    void use_postbw_charge(tables::limit_table& lims, name issuer, name account, symbol_code token_code, int64_t mssg_id);

    fixp_t calc_available_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool);
    void check_upvote_time(uint64_t cur_time, uint64_t mssg_date);
};

} // golos
