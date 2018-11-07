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
        int64_t max_token_prop,
        const limitsarg& lims = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {0, 0}, {0, 0, 0}}
    ) {
        return push(N(setrules), _code, args()
            ("mainfunc", fn_to_mvo(main_fn))
            ("curationfunc", fn_to_mvo(curation_fn))
            ("timepenalty", fn_to_mvo(time_penalty))
            ("curatorsprop", curators_prop)
            ("maxtokenprop", max_token_prop)
            ("tokensymbol", _symbol)
            ("lims", lims)
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

    action_result upvote(account_name voter, account_name author, std::string permlink, int32_t weight) {
        return vote(voter, author, permlink, weight);
    }
    action_result downvote(account_name voter, account_name author, std::string permlink, int32_t weight) {
        return vote(voter, author, permlink, -static_cast<int32_t>(weight));
    }
    action_result unvote(account_name voter, account_name author, std::string permlink) {
        return vote(voter, author, permlink, 0);
    }

    action_result vote(account_name voter, account_name author, std::string permlink, int32_t weight) {
        BOOST_REQUIRE_MESSAGE(weight >= -32768 && weight < 32768,
            "Test is broken, action cannot be serialized with such weight");
        auto act = N(upvote);
        uint16_t w = weight;
        if (weight == 0) {
            act = N(unvote);
        } else if (weight < 0) {
            act = N(downvote);
            w = -weight;
        }
        return push(act, voter, args()
            ("voter", voter)
            ("author", author)
            ("permlink", permlink)
            ("weight", w)
        );
    }

    //// posting tables
    variant get_message(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(messagetable), id, "message");
    }

    variant get_content(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(contenttable), id, "content");
    }

    variant get_vote(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(votetable), id, "voteinfo");
    }

    std::vector<variant> get_reward_pools() {
        return _tester->get_all_chaindb_rows(_code, _code, N(rewardpools), false);
    }

    std::vector<variant> get_messages(account_name user) {
        return _tester->get_all_chaindb_rows(_code, user, N(messagetable), false);
    }

    //// posting helpers
    mvo make_mvo_fn(const std::string& str, base_t max) {
        return mvo()("str", str)("maxarg", max);
    }
    mvo fn_to_mvo(const funcparams& fn) {
        return make_mvo_fn(fn.str, fn.maxarg);
    }


    asset make_asset(double x = 0, symbol sym = symbol(0)) const {
        if (sym == symbol(0)) sym = _symbol;
        return asset(x * sym.precision(), sym);
    }
    string asset_str(double x = 0) {
        return make_asset(x).to_string();
    }

    variant make_balance(double vesting, double delegated = 0, double received = 0, double unlocked = 0) {
        return mvo()
            ("vesting", asset_str(vesting))
            ("delegate_vesting", asset_str(delegated))
            ("received_vesting", asset_str(received))
            ("unlocked_limit", asset_str(unlocked));
    }

};


}} // eosio::testing
