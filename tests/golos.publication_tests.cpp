#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.charge_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "golos.social_test_api.hpp"
#include "../golos.publication/types.h"
#include "contracts.hpp"

using namespace golos;
using namespace fixed_point_utils;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using mvo = fc::mutable_variant_object;
namespace cfg = golos::config;


class golos_publication_tester : public golos_tester {
protected:
    symbol _sym;
    golos_posting_api post;
    golos_vesting_api vest;
    golos_charge_api charge;
    cyber_token_api token;

    std::vector<account_name> _users;
public:

    golos_publication_tester()
        : golos_tester(N(golos.pub))
        , _sym(0, "DUMMY")
        , post({this, _code, _sym})
        , vest({this, cfg::vesting_name, _sym})
        , charge({this, cfg::charge_name, _sym})
        , token({this, cfg::token_name, _sym})
        , _users{_code, N(jackiechan), N(brucelee), N(chucknorris)} {

        produce_block();
        create_accounts(_users);
        create_accounts({cfg::charge_name, cfg::token_name, cfg::vesting_name, cfg::emission_name, cfg::control_name, N(dan.larimer)});
        produce_block();

        install_contract(_code, contracts::posting_wasm(), contracts::posting_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::charge_name, contracts::charge_wasm(), contracts::charge_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }

    void init() {
        prepare_balances();
        BOOST_CHECK_EQUAL(success(), token.open(_code, _sym, _code));
        funcparams fn{"0", 1};
        BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , 5000));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("comment"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("vote"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post bandwidth"));

        BOOST_CHECK_EQUAL(success(), post.init_default_params());

        produce_block();
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _sym, u));
        }
    }

    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), token.create(cfg::emission_name, token.make_asset(1e5), {_code}));
        produce_block();

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(cfg::emission_name));
        produce_block();
    }

    void check_equal_post(const variant& a, const variant& b) {
        BOOST_CHECK_EQUAL(true, a.is_object() && b.is_object());
        BOOST_CHECK_EQUAL(a["id"].as<uint64_t>(), b["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(a["tokenprop"].as<uint16_t>(), b["tokenprop"].as<uint16_t>());
        auto a_ben = a["beneficiaries"];
        auto b_ben = b["beneficiaries"];
        BOOST_CHECK_EQUAL(a_ben.size(), b_ben.size());
        for (size_t i = 0, l = a_ben.size(); i < l; i++) {
            CHECK_EQUAL_OBJECTS(a_ben[i], b_ben[i]);
        }
        BOOST_CHECK_EQUAL(a["rewardweight"].as<uint16_t>(), b["rewardweight"].as<uint16_t>());
        CHECK_EQUAL_OBJECTS(a["state"], b["state"]);
        BOOST_CHECK_EQUAL(a["curators_prcnt"].as<uint16_t>(), b["curators_prcnt"].as<uint16_t>());
    }

    void check_equal_content(const variant& a, const variant& b) {
        BOOST_CHECK_EQUAL(true, a.is_object() && b.is_object());
        BOOST_CHECK_EQUAL(a["id"].as<uint64_t>(), b["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(a["headermssg"].as<std::string>(), b["headermssg"].as<std::string>());
        BOOST_CHECK_EQUAL(a["bodymssg"].as<std::string>(), b["bodymssg"].as<std::string>());
        BOOST_CHECK_EQUAL(a["languagemssg"].as<std::string>(), b["languagemssg"].as<std::string>());
        auto a_tags = a["tags"];
        auto b_tags = b["tags"];
        BOOST_CHECK_EQUAL(a_tags.size(), b_tags.size());
        for (size_t i = 0, l = a_tags.size(); i < l; i++) {
            CHECK_EQUAL_OBJECTS(a_tags[i], b_tags[i]);
        }
        BOOST_CHECK_EQUAL(a["jsonmetadata"].as<std::string>(), b["jsonmetadata"].as<std::string>());
    }

protected:
    const mvo _test_msg = mvo()
        ("id", 1)
        ("tokenprop", 5000)
        ("beneficiaries", variants({}
            // mvo()
            // ("account", "golos.pub")
            // ("weight", 777)
        ))
        ("rewardweight", cfg::_100percent)  // TODO: test that Golos rules (charge restorer) works as expected #617
        ("state", mvo()("netshares",0)("voteshares",0)("sumcuratorsw",0))
        ("curators_prcnt", 1000);

    struct errors: contract_error_messages {
        const string msg_exists            = amsg("This message already exists.");
        const string unregistered_user_    = amsg("unregistered user: ");
        const string no_content            = amsg("Content doesn't exist.");

        const string delete_children       = amsg("You can't delete comment with child comments.");
        const string no_permlink           = amsg("Permlink doesn't exist.");
        const string no_message            = amsg("Message doesn't exist in cashout window.");
        const string vote_same_weight      = amsg("Vote with the same weight has already existed.");
        const string vote_weight_0         = amsg("weight can't be 0.");
        const string vote_weight_gt100     = amsg("weight can't be more than 100%.");
        const string no_revote             = amsg("You can't revote anymore.");
        const string upvote_near_close     = amsg("You can't upvote, because publication will be closed soon.");
        const string max_comment_depth     = amsg("publication::create_message: level > MAX_COMMENT_DEPTH");
        const string window_less_0         = amsg("Cashout window must be greater than 0.");
        const string wndw_less_upvt_lckt   = amsg("Cashout window can't be less than upvote lockout.");
        const string max_cmmnt_dpth_less_0 = amsg("Max comment depth must be greater than 0.");
        const string no_social_acc         = amsg("Social account doesn't exist.");
        const string no_referral_acc       = amsg("Referral account doesn't exist.");
        const string wrong_prmlnk_length   = amsg("Permlink length is empty or more than 256.");
        const string wrong_prmlnk          = amsg("Permlink contains wrong symbol.");
        const string wrong_title_length    = amsg("Title length is more than 256.");
        const string wrong_body_length     = amsg("Body is empty.");

        const string parent_no_message     = amsg("Parent message doesn't exist");

        const string wrong_min_cur_prcnt =
            amsg("Min curators percent must be between 0% and 100% (0-10000).");
        const string max_less_min_cur_prcnt =
            amsg("Min curators percent must be less than max curators percent or equal.");
        const string wrong_max_cur_prcnt = amsg("Max curators percent must be less than 100 or equal.");
        const string no_cur_percent = amsg("Curators percent can be changed only before voting.");
        const string cur_prcnt_less_min = amsg("Curators percent is less than min curators percent.");
        const string cur_prcnt_greater_max = amsg("Curators percent is greater than max curators percent.");

        string invalid_percent(string arg = "percent") const {
            return amsg(arg + " must be between 0% and 100% (0-10000)");
        }
        const string gt_maxtokenprop = amsg("tokenprop must not be greater than pool.rules.maxtokenprop");
        const string no_reblog_mssg = amsg("You can't reblog, because this message doesn't exist.");
        const string own_reblog = amsg("You cannot reblog your own content.");
    } err;
};


BOOST_AUTO_TEST_SUITE(golos_publication_tests)

BOOST_FIXTURE_TEST_CASE(set_params, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Test posting parameters");

    produce_block();

    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(post.get_params().is_null());

    post.init_default_params();

    auto obj_params = post.get_params();
    BOOST_TEST_MESSAGE("--- all params were initialized successful");

    BOOST_CHECK_EQUAL(obj_params["max_vote_changes"]["value"], post.max_vote_changes);
    BOOST_CHECK_EQUAL(obj_params["cashout_window"]["window"], post.window);
    BOOST_CHECK_EQUAL(obj_params["cashout_window"]["upvote_lockout"], post.upvote_lockout);
    BOOST_CHECK_EQUAL(obj_params["max_beneficiaries"]["value"], post.max_beneficiaries);
    BOOST_CHECK_EQUAL(obj_params["max_comment_depth"]["value"], post.max_comment_depth);
    BOOST_CHECK_EQUAL(obj_params["social_acc"]["value"].as_string(), "");
    BOOST_CHECK_EQUAL(obj_params["referral_acc"]["value"].as_string(), "");
    BOOST_CHECK_EQUAL(obj_params["curators_prcnt"]["min_curators_prcnt"], post.min_curators_prcnt);
    BOOST_CHECK_EQUAL(obj_params["curators_prcnt"]["max_curators_prcnt"], post.max_curators_prcnt);

    auto params = "[" + post.get_str_cashout_window(0, post.upvote_lockout) + "]";
    BOOST_CHECK_EQUAL(err.window_less_0, post.set_params(params));

    params = "[" + post.get_str_cashout_window(5, post.upvote_lockout) + "]";
    BOOST_CHECK_EQUAL(err.wndw_less_upvt_lckt, post.set_params(params));

    params = "[" + post.get_str_comment_depth(0) + "]";
    BOOST_CHECK_EQUAL(err.max_cmmnt_dpth_less_0, post.set_params(params));

    params = "[" + post.get_str_social_acc(N(gls.social)) + "]";
    BOOST_CHECK_EQUAL(err.no_social_acc, post.set_params(params));

    params = "[" + post.get_str_referral_acc(N(gls.referral)) + "]";
    BOOST_CHECK_EQUAL(err.no_referral_acc, post.set_params(params));

    params = "[" + post.get_str_curators_prcnt(cfg::_100percent+1, 3300) + "]";
    BOOST_CHECK_EQUAL(err.wrong_min_cur_prcnt, post.set_params(params));

    params = "[" + post.get_str_curators_prcnt(5100, 3300) + "]";
    BOOST_CHECK_EQUAL(err.max_less_min_cur_prcnt, post.set_params(params));

    params = "[" + post.get_str_curators_prcnt(5100, cfg::_100percent+1) + "]";
    BOOST_CHECK_EQUAL(err.wrong_max_cur_prcnt, post.set_params(params));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_rules, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Test setrules validation.");
    prepare_balances();
    BOOST_CHECK_EQUAL(success(), token.open(_code, _sym, _code));
    funcparams fn{"0", 1};

    BOOST_TEST_MESSAGE("-- check that wrong max_token_prop fails");
    BOOST_CHECK_EQUAL(err.invalid_percent("maxtokenprop"), post.set_rules(fn ,fn ,fn , -1));
    BOOST_CHECK_EQUAL(err.invalid_percent("maxtokenprop"), post.set_rules(fn ,fn ,fn , cfg::_100percent + 1));
    BOOST_TEST_MESSAGE("-- check that correct max_token_prop succeeds");
    BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , 0));
    BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , cfg::_100percent));

    BOOST_TEST_MESSAGE("-- check that new message token_prop is limited by max_token_prop");
    post.init_default_params();
    BOOST_CHECK_EQUAL(success(), post.set_limit("post"));
    BOOST_CHECK_EQUAL(success(), post.set_limit("comment"));
    BOOST_CHECK_EQUAL(success(), post.set_limit("vote"));
    BOOST_CHECK_EQUAL(success(), post.set_limit("post bandwidth"));
    auto u = N(brucelee);
    vest.open(u, _sym, u);
    produce_block();

    BOOST_CHECK_EQUAL(success(), post.create_msg({u, "100"},{N(),""}, {}, cfg::_100percent));
    BOOST_CHECK_EQUAL(err.invalid_percent("tokenprop"),
        post.create_msg({u, "100p"},{N(),""}, {}, cfg::_100percent+1));
    auto p = 5000;
    BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , p));
    BOOST_CHECK_EQUAL(success(), post.create_msg({u, "0"},{N(),""}, {}, 0));
    BOOST_CHECK_EQUAL(success(), post.create_msg({u, "50-"},{N(),""}, {}, p-1));
    BOOST_CHECK_EQUAL(success(), post.create_msg({u, "50"},{N(),""}, {}, p));
    BOOST_CHECK_EQUAL(err.gt_maxtokenprop, post.create_msg({u, "50p"},{N(),""}, {}, p+1));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Create message testing.");
    init();
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- checking that another user can create a message with the same permlink.");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(chucknorris), "permlink"}));

    check_equal_post(post.get_message({N(brucelee), "permlink"}), _test_msg);

    BOOST_TEST_MESSAGE("--- checking that message wasn't closed.");
    produce_blocks(seconds_to_blocks(post.window));
    auto msg = post.get_message({N(brucelee), "permlink"});
    BOOST_CHECK_EQUAL(msg.is_null(), false);

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "permlink1"}, {N(brucelee), "permlink"}));

    auto perm = post.get_permlink({N(brucelee), "permlink"});
    BOOST_CHECK_EQUAL(perm["childcount"].as<uint32_t>(), 1);

    BOOST_TEST_MESSAGE("--- checking that message was closed.");
    produce_block();
    msg = post.get_message({N(brucelee), "permlink"});
    BOOST_CHECK_EQUAL(msg.is_null(), true);

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "permlink2"}, {N(brucelee), "permlink"}));

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "permlink3"}, {N(brucelee), "permlink"}));

    BOOST_CHECK_EQUAL(err.unregistered_user_ + "dan.larimer", post.create_msg({N(dan.larimer), "hi"}));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Update message testing.");
    init();

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    BOOST_CHECK_EQUAL(success(), post.update_msg({N(brucelee), "permlink"},
        "headermssgnew", "bodymssgnew", "languagemssgnew", {{"tagnew"}}, "jsonmetadatanew"));

    produce_blocks(seconds_to_blocks(post.window) + 1);

    BOOST_CHECK_EQUAL(success(), post.update_msg({N(brucelee), "permlink"},
        "headermssgnew", "bodymssgnew", "languagemssgnew", {{"tagnew"}}, "jsonmetadatanew"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delete_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Delete message testing.");
    init();

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "child"}, {N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- fail then delete non-existing post and post with child");
    BOOST_CHECK_EQUAL(err.no_permlink, post.delete_msg({N(jackiechan), "permlink1"}));
    BOOST_CHECK_EQUAL(err.delete_children, post.delete_msg({N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- success when delete child");
    BOOST_CHECK_EQUAL(success(), post.delete_msg({N(jackiechan), "child"}));
    BOOST_TEST_MESSAGE("--- success delete when no more children");
    BOOST_CHECK_EQUAL(success(), post.delete_msg({N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- fail delete post with child when cashout window has closed");
    auto need_blocks = seconds_to_blocks(post.window);
    auto wait_blocks = 5;
    produce_blocks(wait_blocks);
    need_blocks -= wait_blocks;
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink-done"}));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "child-done"}, {N(brucelee), "permlink-done"}));

    produce_blocks(need_blocks);
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink-done"}).is_null(), false);
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "child-done"}).is_null(), false);
    produce_block();

    produce_blocks(wait_blocks - 1);
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink-done"}).is_null(), false);
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "child-done"}).is_null(), false);

    produce_block();
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink-done"}).is_null(), true);
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "child-done"}).is_null(), true);

    BOOST_CHECK_EQUAL(err.delete_children, post.delete_msg({N(brucelee), "permlink-done"}));

    BOOST_TEST_MESSAGE("--- success when delete child when cashout window has closed");
    BOOST_CHECK_EQUAL(success(), post.delete_msg({N(jackiechan), "child-done"}));

    BOOST_TEST_MESSAGE("--- success delete post when cashout window has closed");
    BOOST_CHECK_EQUAL(success(), post.delete_msg({N(brucelee), "permlink-done"}));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(upvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Upvote testing.");
    init();

    auto permlink = "permlink";
    auto vote_brucelee = [&](auto weight){ return post.upvote(N(brucelee), {N(brucelee), permlink}, weight); };
    auto vote_jackie = [&](auto weight){ return post.upvote(N(jackiechan), {N(brucelee), permlink}, weight); };

    BOOST_TEST_MESSAGE("--- fail on non-existing message");
    BOOST_CHECK_EQUAL(err.no_permlink, vote_brucelee(123));
    BOOST_TEST_CHECK(post.get_vote(N(brucelee), 1).is_null());

    BOOST_TEST_MESSAGE("--- succeed on initial upvote");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), permlink}));
    BOOST_CHECK_EQUAL(success(), vote_brucelee(123));
    auto _vote = mvo()("id",0)("message_id",1)("voter","brucelee")("count",1);   // TODO: time
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 0), _vote);
    produce_block();

    BOOST_TEST_MESSAGE("--- fail on same or wrong weight");
    BOOST_CHECK_EQUAL(err.vote_same_weight, vote_brucelee(123));
    BOOST_CHECK_EQUAL(err.vote_weight_0, vote_brucelee(0));
    BOOST_CHECK_EQUAL(err.vote_weight_gt100, vote_brucelee(cfg::_100percent+1));

    BOOST_TEST_MESSAGE("--- succeed on revote with different weight");
    BOOST_CHECK_EQUAL(success(), vote_brucelee(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- succeed max_vote_changes revotes and fail on next vote");
    for (auto i = 0; i < post.max_vote_changes; i++) {
        BOOST_CHECK_EQUAL(success(), vote_jackie(i+1));
        produce_block();
    }
    _vote = mvo(_vote)("id", 1)("voter", "jackiechan");
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 1), mvo(_vote)("count", post.max_vote_changes));
    BOOST_CHECK_EQUAL(err.no_revote, vote_jackie(cfg::_100percent));
    produce_blocks(seconds_to_blocks(post.window - post.upvote_lockout) - post.max_vote_changes - 1);
    BOOST_CHECK_EQUAL(err.no_revote, vote_jackie(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- fail while upvote lockout");
    produce_block();
    //BOOST_CHECK_EQUAL(err.upvote_near_close, vote_jackie(cfg::_100percent));          // TODO Fix broken test GolosChain/golos-smart#410
    produce_blocks(seconds_to_blocks(post.upvote_lockout) - 1);
    //BOOST_CHECK_EQUAL(err.upvote_near_close, vote_jackie(cfg::_100percent));          // TODO Fix broken test GolosChain/golos-smart#410

    BOOST_TEST_MESSAGE("--- succeed vote after cashout");
    produce_block();
    BOOST_CHECK_EQUAL(err.no_message, vote_jackie(cfg::_100percent));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(downvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Downvote testing.");
    init();
    auto permlink = "permlink";
    auto vote_brucelee = [&](auto weight){ return post.upvote(N(brucelee), {N(brucelee), permlink}, weight); };
    auto vote_jackie = [&](auto weight){ return post.upvote(N(jackiechan), {N(brucelee), permlink}, weight); };

    BOOST_TEST_MESSAGE("--- fail on non-existing message");
    BOOST_CHECK_EQUAL(err.no_permlink, vote_brucelee(123));
    BOOST_TEST_CHECK(post.get_vote(N(brucelee), 1).is_null());

    BOOST_TEST_MESSAGE("--- succeed on initial downvote");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), permlink}));
    BOOST_CHECK_EQUAL(success(), vote_brucelee(123));
    auto _vote = mvo()("id",0)("message_id",1)("voter","brucelee")("count",1);   // TODO: time
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 0), _vote);
    produce_block();

    BOOST_TEST_MESSAGE("--- fail on same or wrong weight");
    BOOST_CHECK_EQUAL(err.vote_same_weight, vote_brucelee(123));
    BOOST_CHECK_EQUAL(err.vote_weight_0, vote_brucelee(0));
    BOOST_CHECK_EQUAL(err.vote_weight_gt100, vote_brucelee(cfg::_100percent+1));

    BOOST_TEST_MESSAGE("--- succeed on revote with different weight");
    BOOST_CHECK_EQUAL(success(), vote_brucelee(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- succeed max_vote_changes revotes and fail on next vote");
    for (auto i = 0; i < post.max_vote_changes; i++) {
        BOOST_CHECK_EQUAL(success(), vote_jackie(i+1));
        produce_block();
    }
    _vote = mvo(_vote)("id", 1)("voter", "jackiechan");
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 1), mvo(_vote)("count", post.max_vote_changes));
    BOOST_CHECK_EQUAL(err.no_revote, vote_jackie(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- succeed vote after cashout");
    produce_blocks(seconds_to_blocks(post.window) - post.max_vote_changes - 1);
    BOOST_CHECK_EQUAL(err.no_revote, vote_jackie(cfg::_100percent));
    produce_block();
    BOOST_CHECK_EQUAL(err.no_message, vote_jackie(cfg::_100percent));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(unvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Unvote testing.");
    init();
    BOOST_CHECK_EQUAL(err.no_permlink, post.unvote(N(brucelee), {N(brucelee), "permlink"}));

    // TODO: test fail on initial unvote
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(brucelee), {N(brucelee), "permlink"}, 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), {N(brucelee), "permlink"}));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), {N(brucelee), "permlink"}, 333));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), {N(brucelee), "permlink"}));
    produce_block();

    BOOST_CHECK_EQUAL(err.vote_same_weight, post.unvote(N(brucelee), {N(brucelee), "permlink"}));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(mixed_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Mixed vote testing.");
    init();
    BOOST_TEST_MESSAGE("--- test that each vote type increases votes count");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), {N(brucelee), "permlink"}, 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), {N(brucelee), "permlink"}));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.upvote(N(brucelee), {N(brucelee), "permlink"}, 321));
    produce_block();

    auto vote = post.get_vote(N(brucelee), 0);
    BOOST_CHECK_EQUAL(vote["count"].as<uint64_t>(), 3);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delete_post_with_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Delete post with vote testing.");   // TODO: move to "delete" test ?
    init();
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "upvote-me"}));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(chucknorris), "downvote-me"}));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(chucknorris), {N(brucelee), "upvote-me"}, 321));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), {N(chucknorris), "downvote-me"}, 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.delete_msg({N(chucknorris), "downvote-me"}));
//    BOOST_CHECK_EQUAL(err.delete_rshares, post.delete_msg({N(brucelee), "upvote-me"}));    // TODO:
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(nesting_level_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("nesting level test.");
    init();
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink0"}));
    size_t i = 0;
    for (; i < post.max_comment_depth; i++) {
        BOOST_CHECK_EQUAL(success(), post.create_msg(
            {N(brucelee), "permlink" + std::to_string(i+1)},
            {N(brucelee), "permlink" + std::to_string(i)}));
    }
    BOOST_CHECK_EQUAL(err.max_comment_depth, post.create_msg(
        {N(brucelee), "permlink" + std::to_string(i+1)},
        {N(brucelee), "permlink" + std::to_string(i)}));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(comments_cashout_time_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("comments_cashout_time_test.");
    init();
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    auto need_blocks = seconds_to_blocks(post.window);
    auto wait_blocks = 5;
    produce_blocks(wait_blocks);
    need_blocks -= wait_blocks;

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(chucknorris), "comment-permlink"}, {N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- creating " << need_blocks << " blocks");
    produce_blocks(need_blocks);

    BOOST_TEST_MESSAGE("--- checking that messages wasn't closed.");
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink"}).is_null(), false);

    produce_block();

    BOOST_TEST_MESSAGE("--- checking that messages was closed.");
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink"}).is_null(), true);

    produce_blocks(wait_blocks-1);
    BOOST_TEST_MESSAGE("--- checking that comment wasn't closed.");
    BOOST_CHECK_EQUAL(post.get_message({N(chucknorris), "comment-permlink"}).is_null(), false);

    produce_block();
    BOOST_TEST_MESSAGE("--- checking that comment was closed.");
    BOOST_CHECK_EQUAL(post.get_message({N(chucknorris), "comment-permlink"}).is_null(), true);

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(jackiechan), "sorry-guys-i-am-late"}, {N(brucelee), "permlink"}));
    produce_blocks(need_blocks);
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "sorry-guys-i-am-late"}).is_null(), false);
    produce_block();

    produce_blocks(wait_blocks - 1);
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "sorry-guys-i-am-late"}).is_null(), false);

    BOOST_TEST_MESSAGE("--- checking that closed message comment was closed.");
    produce_block();
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "sorry-guys-i-am-late"}).is_null(), true);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(data_validation, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("data_validation_test.");
    init();
    std::string str256(256, 'a');

    BOOST_TEST_MESSAGE("--- checking permlink length.");
    BOOST_CHECK_EQUAL(err.wrong_prmlnk_length, post.create_msg({N(brucelee), ""}));
    BOOST_CHECK_EQUAL(err.wrong_prmlnk_length, post.create_msg({N(brucelee), str256}));

    BOOST_TEST_MESSAGE("--- checking permlink naming convension.");
    BOOST_CHECK_EQUAL(err.wrong_prmlnk, post.create_msg({N(brucelee), "ABC"}));
    BOOST_CHECK_EQUAL(err.wrong_prmlnk, post.create_msg({N(brucelee), "АБЦ"}));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "abcdefghijklmnopqrstuvwxyz0123456789-"}));

    BOOST_TEST_MESSAGE("--- checking title length.");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));
    BOOST_CHECK_EQUAL(err.wrong_title_length, post.create_msg({N(brucelee), "test-title"},
                                                              {N(brucelee), "permlink"},
                                                              {},
                                                              5000,
                                                              false,
                                                              str256));

    BOOST_TEST_MESSAGE("--- checking body length.");
    BOOST_CHECK_EQUAL(err.wrong_body_length, post.create_msg({N(brucelee), "test-body"},
                                                             {N(brucelee), "permlink"},
                                                             {},
                                                             5000,
                                                             false,
                                                             "headermssg",
                                                             ""));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(upvote_near_close, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Upvote near close testing.");
    init();

    auto permlink = "permlink";

    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(brucelee), token.make_asset(500), "issue tokens brucelee"));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(jackiechan), token.make_asset(500), "issue tokens jackiechan"));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(chucknorris), token.make_asset(500), "issue tokens chucknorris"));
    produce_block();

    BOOST_CHECK_EQUAL(success(), token.transfer(N(brucelee), cfg::vesting_name, token.make_asset(100), "buy vesting"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(jackiechan), cfg::vesting_name, token.make_asset(100), "buy vesting"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(chucknorris), cfg::vesting_name, token.make_asset(100), "buy vesting"));
    produce_block();

    auto vote_brucelee = [&](auto weight){ return post.upvote(N(brucelee), {N(brucelee), permlink}, weight); };
    auto vote_jackie = [&](auto weight){ return post.upvote(N(jackiechan), {N(brucelee), permlink}, weight); };
    auto vote_chucknorris = [&](auto weight){ return post.upvote(N(chucknorris), {N(brucelee), permlink}, weight); };

    BOOST_TEST_MESSAGE("--- succeed on initial upvote");
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), permlink}));
    BOOST_CHECK_EQUAL(success(), vote_brucelee(123));
    auto _vote = mvo()("id",0)("message_id",1)("voter","brucelee")("count",1);   // TODO: time
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 0), _vote);
    produce_block();

    BOOST_TEST_MESSAGE("--- succeed on revote with different weight");
    BOOST_CHECK_EQUAL(success(), vote_brucelee(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- succeed max_vote_changes revotes and fail on next vote");
    for (auto i = 0; i < post.max_vote_changes - 1; i++) {
        BOOST_CHECK_EQUAL(success(), vote_jackie(i+1));
        produce_block();
    }

    produce_blocks(seconds_to_blocks(post.window - post.upvote_lockout) - post.max_vote_changes + 1);
    BOOST_TEST_MESSAGE("--- fail while upvote lockout");

    BOOST_CHECK_EQUAL(err.upvote_near_close, vote_jackie(cfg::_100percent));          // TODO Fix broken test GolosChain/golos-smart#410
    produce_blocks(seconds_to_blocks(post.upvote_lockout) - 1);
    BOOST_CHECK_EQUAL(err.upvote_near_close, vote_chucknorris(cfg::_100percent));          // TODO Fix broken test GolosChain/golos-smart#410
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_curators_prcnt, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Test curators percent");

    init();

    auto create_msg = [&](optional<uint16_t> curators_prcnt = optional<uint16_t>(), mssgid message_id = {}){
        if (message_id == mssgid())
            message_id = {N(brucelee), "permlink"};
        return post.create_msg(
            message_id,
            {N(), "parentprmlnk"},
            {},
            5000,
            false,
            "headermssg",
            "bodymssg",
            "languagemssg",
            {{"tag"}},
            "jsonmetadata",
            curators_prcnt
            );
    };

    BOOST_TEST_MESSAGE("--- checking that curators percent doesn't fit");
    BOOST_CHECK_EQUAL(err.cur_prcnt_less_min, create_msg(post.min_curators_prcnt-1));
    BOOST_CHECK_EQUAL(err.cur_prcnt_greater_max, create_msg(post.max_curators_prcnt+1));

    BOOST_TEST_MESSAGE("--- checking that curators percent was setted as default");
    BOOST_CHECK_EQUAL(success(), create_msg());
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink"})["curators_prcnt"], post.min_curators_prcnt);

    BOOST_TEST_MESSAGE("--- checking that curators percent was setted correctly");
    BOOST_CHECK_EQUAL(success(), create_msg(7100, {N(jackiechan), "permlink"}));
    BOOST_CHECK_EQUAL(post.get_message({N(jackiechan), "permlink"})["curators_prcnt"], 7100);

    BOOST_TEST_MESSAGE("--- checking that curators percent was changed");
    BOOST_CHECK_EQUAL(success(), post.set_curators_prcnt({N(brucelee), "permlink"}, 7300));
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink"})["curators_prcnt"], 7300);

    BOOST_TEST_MESSAGE("--- checking that curators percent can't be changed");
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(jackiechan), token.make_asset(500), "issue tokens jackiechan"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(jackiechan), cfg::vesting_name, token.make_asset(100), "buy vesting"));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(jackiechan), {N(brucelee), "permlink"}, 123));
    BOOST_CHECK_EQUAL(err.no_cur_percent, post.set_curators_prcnt({N(brucelee), "permlink"}, 7500));
    BOOST_CHECK_EQUAL(post.get_message({N(brucelee), "permlink"})["curators_prcnt"], 7300);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(reblog_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Reblog message testing.");

    init();
    std::string str256(256, 'a');

    BOOST_CHECK_EQUAL(success(), post.create_msg({N(brucelee), "permlink"}));

    BOOST_TEST_MESSAGE("--- checking for own reblog.");
    BOOST_CHECK_EQUAL(err.own_reblog, post.reblog_msg(N(brucelee),
                                                      {N(brucelee), "permlink"},
                                                      "headermssg",
                                                      "bodymssg"));

    BOOST_TEST_MESSAGE("--- checking title length.");
    BOOST_CHECK_EQUAL(err.wrong_title_length, post.reblog_msg(N(chucknorris),
                                                              {N(brucelee), "permlink"},
                                                              str256,
                                                              "bodymssg"));

    BOOST_TEST_MESSAGE("--- checking body length.");
    BOOST_CHECK_EQUAL(err.wrong_body_length, post.reblog_msg(N(chucknorris),
                                                             {N(brucelee), "permlink"},
                                                             "headermssg",
                                                             ""));
    
    BOOST_TEST_MESSAGE("--- checking message for reblog.");
    BOOST_CHECK_EQUAL(err.no_reblog_mssg, post.reblog_msg(N(chucknorris),
                                                          {N(brucelee), "test"},
                                                          "headermssg",
                                                          "bodymssg"));

    BOOST_TEST_MESSAGE("--- checking that message was rebloged successful.");
    BOOST_CHECK_EQUAL(success(), post.reblog_msg(N(chucknorris),
                                                 {N(brucelee), "permlink"},
                                                 "headermssg",
                                                 "bodymssg"));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
