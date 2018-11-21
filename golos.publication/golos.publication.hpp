#pragma once
#include "objects.hpp"

namespace golos {

using namespace eosio;

class publication : public contract {
public:
    publication(account_name self);
    void apply(uint64_t code, uint64_t action);

private:
    void set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
        int64_t curatorsprop, int64_t maxtokenprop, symbol_type tokensymbol, limitsarg lims_arg);
    void on_transfer(account_name from, account_name to, asset quantity, std::string memo);
    void create_message(account_name account, std::string permlink, account_name parentacc, std::string parentprmlnk,
        std::vector<structures::beneficiary> beneficiaries, int64_t tokenprop, bool vestpayment,
        std::string headermssg, std::string bodymssg, std::string languagemssg, std::vector<structures::tag> tags,
        std::string jsonmetadata);
    void update_message(account_name account, std::string permlink,
        std::string headermssg, std::string bodymssg, std::string languagemssg, std::vector<structures::tag> tags,
        std::string jsonmetadata);
    void delete_message(account_name account, std::string permlink);
    void upvote(account_name voter, account_name author, std::string permlink, uint16_t weight);
    void downvote(account_name voter, account_name author, std::string permlink, uint16_t weight);
    void unvote(account_name voter, account_name author, std::string permlink);

    void close_message(account_name account, uint64_t id);
    void close_message_timer(account_name account, uint64_t id, uint64_t delay_sec);
    void set_vote(account_name voter, account_name author, std::string permlink, int16_t weight);
    uint16_t notify_parent(bool increase, account_name parentacc, uint64_t parent_id);
    void fill_depleted_pool(tables::reward_pools& pools, asset quantity,
        tables::reward_pools::const_iterator excluded);
    auto get_pool(tables::reward_pools& pools, uint64_t time);
    int64_t pay_curators(account_name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum,
        symbol_type tokensymbol);
    void payto(account_name user, asset quantity, enum_t mode);
    void check_account(account_name user, symbol_type tokensymbol);

    elaf_t apply_limits(atmsp::machine<fixp_t>& machine, account_name user, const structures::rewardpool& pool,
        structures::limits::kind_t kind, uint64_t cur_time, elaf_t w);

    static structures::funcinfo load_func(const funcparams& params, const std::string& name,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc);
    static atmsp::storable::bytecode load_restorer_func(const std::string str_func,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine);
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val,
        const structures::funcinfo& func);
    
    static void check_upvote_time(uint64_t cur_time, uint64_t mssg_date);
    fixp_t calc_rshares(account_name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool, atmsp::machine<fixp_t>& machine);
};

} // golos
