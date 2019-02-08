#pragma once
#include "test_api_helper.hpp"
#include "golos.publication_rewards_types.hpp"

namespace eosio { namespace testing {


struct golos_posting_api: base_contract_api {
    golos_posting_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    //// posting actions
    action_result set_rules(
        const funcparams& main_fn,
        const funcparams& curation_fn,
        const funcparams& time_penalty,
        int64_t curators_prop,
        int64_t max_token_prop) {
        return push(N(setrules), _code, args()
            ("mainfunc", fn_to_mvo(main_fn))
            ("curationfunc", fn_to_mvo(curation_fn))
            ("timepenalty", fn_to_mvo(time_penalty))
            ("curatorsprop", curators_prop)
            ("maxtokenprop", max_token_prop)
            ("tokensymbol", _symbol)
        );
    }
    
    action_result set_limit(std::string act, uint8_t charge_id = 0, int64_t price = -1, int64_t cutoff = 0, int64_t vesting_price = 0, int64_t min_vesting = 0) {
        return push(N(setlimit), _code, args()
            ("act", act)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("price", price)
            ("cutoff", cutoff)
            ("vesting_price", vesting_price)
            ("min_vesting", min_vesting)
        );
    }

    action_result create_msg(
        account_name author,
        std::string permlink,
        account_name parent_author = N(),
        std::string parent_permlink = "parentprmlnk",
        std::vector<beneficiary> beneficiaries = {},
        int64_t token_prop = 5000,
        bool vest_payment = false,
        std::string title = "headermssg",
        std::string body = "bodymssg",
        std::string language = "languagemssg",
        std::vector<tags> tags = {{"tag"}},
        std::string json_metadata = "jsonmetadata"
    ) {
        return push(N(createmssg), author, args()
            ("account", author)
            ("permlink", permlink)
            ("parentacc", parent_author)
            ("parentprmlnk", parent_permlink)
            ("beneficiaries", beneficiaries)
            ("tokenprop", token_prop)
            ("vestpayment", vest_payment)
            ("headermssg", title)
            ("bodymssg", body)
            ("languagemssg", language)
            ("tags", tags)
            ("jsonmetadata", json_metadata)
        );
    }

    action_result update_msg(
        account_name author,
        std::string permlink,
        std::string title,
        std::string body,
        std::string language,
        std::vector<tags> tags,
        std::string json_metadata
    ) {
        return push(N(updatemssg), author, args()
            ("account", author)
            ("permlink", permlink)
            ("headermssg", title)
            ("bodymssg", body)
            ("languagemssg", language)
            ("tags",tags)
            ("jsonmetadata", json_metadata)
        );
    }

    action_result delete_msg(account_name author, std::string permlink) {
        return push(N(deletemssg), author, args()
            ("account", author)
            ("permlink", permlink)
        );
    }

    action_result upvote(account_name voter, account_name author, std::string permlink, uint16_t weight) {
        return push(N(upvote), voter, args()
            ("voter", voter)
            ("author", author)
            ("permlink", permlink)
            ("weight", weight)
        );
    }
    action_result downvote(account_name voter, account_name author, std::string permlink, uint16_t weight) {
        return push(N(downvote), voter, args()
            ("voter", voter)
            ("author", author)
            ("permlink", permlink)
            ("weight", weight)
        );
    }
    action_result unvote(account_name voter, account_name author, std::string permlink) {
        return push(N(unvote), voter, args()
            ("voter", voter)
            ("author", author)
            ("permlink", permlink)
        );
    }
    action_result set_params(std::string json_params) {
        return push(N(setparams), _code, args()
            ("params", json_str_to_obj(json_params)));
    }

    action_result init_default_params() {
        auto vote_changes = get_str_vote_changes(max_vote_changes);
        auto cashout_window = get_str_cashout_window(window, upvote_lockout);
        auto beneficiaries = get_str_beneficiaries(max_beneficiaries);
        auto comment_depth = get_str_comment_depth(max_comment_depth);
        auto social = get_str_social_acc(name());
        auto referral = get_str_referral_acc(name());

        auto params = "[" + vote_changes + "," + cashout_window + "," + beneficiaries + "," + comment_depth + 
            "," + social + "," + referral + "]";
        return set_params(params); 
    } 

    variant get_params() const {
        return base_contract_api::get_struct(_code, N(pstngparams), N(pstngparams), "posting_state");
    }

    string get_str_vote_changes(uint8_t max_vote_changes) {
        return string("['st_max_vote_changes', {'value':'") + std::to_string(max_vote_changes) + "'}]";
    }

    string get_str_cashout_window(uint32_t window, uint32_t upvote_lockout) {
        return string("['st_cashout_window', {'window':'") + std::to_string(window) + "','upvote_lockout':'" + std::to_string(upvote_lockout) + "'}]";
    }

    string get_str_beneficiaries(uint8_t max_beneficiaries) {
        return string("['st_max_beneficiaries', {'value':'") + std::to_string(max_beneficiaries) + "'}]";
    }

    string get_str_comment_depth(uint16_t max_comment_depth) {
        return string("['st_max_comment_depth', {'value':'") + std::to_string(max_comment_depth) + "'}]";
    }
    
    string get_str_social_acc(name social_acc) {
        return string("['st_social_acc', {'value':'") + name{social_acc}.to_string() + "'}]";
    }
    
    string get_str_referral_acc(name referral_acc) {
        return string("['st_referral_acc', {'value':'") + name{referral_acc}.to_string() + "'}]";
    }   

    //// posting tables
    variant get_message(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(message), id, "message");
    }

    variant get_content(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(content), id, "content");
    }

    variant get_message(account_name acc, const std::string& permlink) {
        variant obj = _tester->get_chaindb_lower_bound_struct(_code, acc, N(message), N(bypermlink), permlink, "message");
        if (!obj.is_null()) {
            if(obj["permlink"].as<std::string>() != permlink) {
                return variant();
            }
        }
        return obj;
    }

    variant get_content(account_name acc, const std::string& permlink) {
        variant obj = get_message(acc, permlink);
        if(!obj.is_null()) {
            return get_content(acc, obj["id"].as<uint64_t>());
        }
        return variant();
    }

    variant get_vote(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(vote), id, "voteinfo");
    }

    std::vector<variant> get_reward_pools() {
        return _tester->get_all_chaindb_rows(_code, _code, N(rewardpools), false);
    }

    std::vector<variant> get_messages(account_name user) {
        return _tester->get_all_chaindb_rows(_code, user, N(message), false);
    }

    //// posting helpers
    mvo make_mvo_fn(const std::string& str, base_t max) {
        return mvo()("str", str)("maxarg", max);
    }
    mvo fn_to_mvo(const funcparams& fn) {
        return make_mvo_fn(fn.str, fn.maxarg);
    }

    const uint8_t max_vote_changes = 5;
    const uint32_t window = 120;
    const uint32_t upvote_lockout = 15;
    const uint8_t max_beneficiaries = 64;
    const uint16_t max_comment_depth = 127;
};


}} // eosio::testing
