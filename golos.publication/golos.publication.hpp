#pragma once
#include "objects.hpp"
#include "parameters.hpp"

namespace golos {

using namespace eosio;

class publication : public contract {
public:
    using contract::contract;
    void set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
        int64_t curatorsprop, int64_t maxtokenprop, symbol tokensymbol, limitsarg lims_arg);
    void setprops(const forumprops& props);
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

    elaf_t apply_limits(atmsp::machine<fixp_t>& machine, name user, const structures::rewardpool& pool,
        structures::limits::kind_t kind, uint64_t cur_time, elaf_t w);

    static structures::funcinfo load_func(const funcparams& params, const std::string& name,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc);
    static atmsp::storable::bytecode load_restorer_func(const std::string str_func,
        const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine);
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val,
        const structures::funcinfo& func);

    void check_upvote_time(uint64_t cur_time, uint64_t mssg_date);
    fixp_t calc_rshares(name voter, int16_t weight, uint64_t cur_time, const structures::rewardpool& pool, atmsp::machine<fixp_t>& machine);
};

} // golos
