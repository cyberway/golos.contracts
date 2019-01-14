#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "../golos.publication/types.h"
#include "contracts.hpp"

using namespace golos;
using namespace fixed_point_utils;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using mvo = fc::mutable_variant_object;
namespace cfg = golos::config;

namespace structures {
    struct beneficiaries {
        account_name account;
        int64_t deductprcnt;
    };

    struct tags {
        std::string tag;
    };
}
FC_REFLECT(structures::tags, (tag))
FC_REFLECT(structures::beneficiaries, (account)(deductprcnt))


class golos_publication_tester : public golos_tester {
protected:
    symbol _sym;
    golos_posting_api post;
    golos_vesting_api vest;
    cyber_token_api token;

    std::vector<account_name> _users;
public:

    golos_publication_tester()
        : golos_tester(N(golos.pub))
        , _sym(0, "DUMMY")
        , post({this, _code, _sym})
        , vest({this, cfg::vesting_name, _sym})
        , token({this, cfg::token_name, _sym})
        , _users{_code, N(jackiechan), N(brucelee), N(chucknorris)} {

        produce_block();
        create_accounts(_users);
        create_accounts({cfg::token_name, cfg::vesting_name, cfg::emission_name, N(dan.larimer)});
        produce_block();

        install_contract(_code, contracts::posting_wasm(), contracts::posting_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }

    void init() {
        BOOST_CHECK_EQUAL(success(), token.create(cfg::emission_name, asset(1, _sym)));
        BOOST_CHECK_EQUAL(success(), token.open(_code, _sym, _code));
        funcparams fn{"0", 1};
        BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , 0, 0));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("comment"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("vote"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post bandwidth"));
        produce_block();
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _sym, u));
        }
    }

    void check_equal_post(const variant& a, const variant& b) {
        BOOST_CHECK_EQUAL(true, a.is_object() && b.is_object());
        BOOST_CHECK_EQUAL(a["id"].as<uint64_t>(), b["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(a["parentacc"].as<account_name>(), b["parentacc"].as<account_name>());
        BOOST_CHECK_EQUAL(a["parent_id"].as<uint64_t>(), b["parent_id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(a["tokenprop"].as<uint64_t>(), b["tokenprop"].as<uint64_t>());
        auto a_ben = a["beneficiaries"];
        auto b_ben = b["beneficiaries"];
        BOOST_CHECK_EQUAL(a_ben.size(), b_ben.size());
        for (size_t i = 0, l = a_ben.size(); i < l; i++) {
            CHECK_EQUAL_OBJECTS(a_ben[i], b_ben[i]);
        }
        BOOST_CHECK_EQUAL(a["rewardweight"].as<uint64_t>(), b["rewardweight"].as<uint64_t>());
        CHECK_EQUAL_OBJECTS(a["state"], b["state"]);
        BOOST_CHECK_EQUAL(a["childcount"].as<uint64_t>(), b["childcount"].as<uint64_t>());
        BOOST_CHECK_EQUAL(a["closed"].as<bool>(), b["closed"].as<bool>());
        BOOST_CHECK_EQUAL(a["level"].as<uint16_t>(), b["level"].as<uint16_t>());
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

    void init_params() {
        auto vote_changes = post.get_str_vote_changes(post.max_vote_changes);
        auto cashout_window = post.get_str_cashout_window(post.window, post.upvote_lockout);
        auto beneficiaries = post.get_str_beneficiaries(post.max_beneficiaries);
        auto comment_depth = post.get_str_comment_depth(post.max_comment_depth);

        auto params = "[" + vote_changes + "," + cashout_window + "," + beneficiaries + "," + comment_depth + "]";
        BOOST_CHECK_EQUAL(success(), post.set_params(params)); 
    } 

protected:
    const mvo _test_msg = mvo()
        ("id", hash64("permlink"))
        ("parentacc", "")
        ("parent_id", 0)
        ("tokenprop", 0)
        ("beneficiaries", variants({}
            // mvo()
            // ("account", "golos.pub")
            // ("deductprcnt", static_cast<base_t>(elaf_t(elai_t(777) / elai_t(10000)).data()))}
        ))
        ("rewardweight", 4611686018427387904ull)    // TODO: fix
        ("state", mvo()("netshares",0)("voteshares",0)("sumcuratorsw",0))
        ("childcount", 0)
        ("closed", false)
        ("level", 0);

    const mvo _test_content = mvo()
        ("id", hash64("permlink"))
        ("headermssg", "headermssg")
        ("bodymssg", "bodymssg")
        ("languagemssg", "languagemssg")
        ("tags", variants({mvo()("tag", "tag")}))
        ("jsonmetadata", "jsonmetadata");

    struct errors: contract_error_messages {
        const string msg_exists            = amsg("This message already exists.");
        const string unregistered_user_    = amsg("unregistered user: ");
        const string no_content            = amsg("Content doesn't exist.");

        const string delete_children       = amsg("You can't delete comment with child comments.");
        const string no_message            = amsg("Message doesn't exist.");
        const string vote_same_weight      = amsg("Vote with the same weight has already existed.");
        const string vote_weight_0         = amsg("weight can't be 0.");
        const string vote_weight_gt100     = amsg("weight can't be more than 100%.");
        const string no_revote             = amsg("You can't revote anymore.");
        const string upvote_near_close     = amsg("You can't upvote, because publication will be closed soon.");
        const string max_comment_depth     = amsg("publication::create_message: level > MAX_COMMENT_DEPTH");
        const string window_less_0         = amsg("Cashout window must be greater than 0.");
        const string wndw_less_upvt_lckt   = amsg("Cashout window can't be less than upvote lockout.");
        const string max_cmmnt_dpth_less_0 = amsg("Max comment depth must be greater than 0.");
    } err;
};


BOOST_AUTO_TEST_SUITE(golos_publication_tests)

BOOST_FIXTURE_TEST_CASE(set_params, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Test posting parameters");
    
    produce_block();
    
    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(post.get_params().is_null());

    init_params();

    auto obj_params = post.get_params();
    BOOST_TEST_MESSAGE("--- all params were initialized successful");

    BOOST_CHECK_EQUAL(obj_params["max_vote_changes"]["value"], post.max_vote_changes);
    BOOST_CHECK_EQUAL(obj_params["cashout_window"]["window"], post.window);
    BOOST_CHECK_EQUAL(obj_params["cashout_window"]["upvote_lockout"], post.upvote_lockout);
    BOOST_CHECK_EQUAL(obj_params["max_beneficiaries"]["value"], post.max_beneficiaries);
    BOOST_CHECK_EQUAL(obj_params["max_comment_depth"]["value"], post.max_comment_depth);

    auto params = "[" + post.get_str_cashout_window(0, post.upvote_lockout) + "]";
    BOOST_CHECK_EQUAL(err.window_less_0, post.set_params(params));

    params = "[" + post.get_str_cashout_window(5, post.upvote_lockout) + "]";
    BOOST_CHECK_EQUAL(err.wndw_less_upvt_lckt, post.set_params(params));

    params = "[" + post.get_str_comment_depth(0) + "]";
    BOOST_CHECK_EQUAL(err.max_cmmnt_dpth_less_0, post.set_params(params));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Create message testing.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));

    BOOST_TEST_MESSAGE("--- checking that another user can create a message with the same permlink.");
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(chucknorris), "permlink"));

    auto id = hash64("permlink");
    check_equal_post(post.get_message(N(brucelee), id), _test_msg);
    check_equal_content(post.get_content(N(brucelee), id), _test_content);

    BOOST_TEST_MESSAGE("--- checking that message wasn't closed.");
    produce_blocks(seconds_to_blocks(post.window));
    auto msg = post.get_message(N(brucelee), id);
    BOOST_CHECK_EQUAL(msg["closed"].as<bool>(), false);

    BOOST_TEST_MESSAGE("--- checking that message was closed.");
    produce_block();
    msg = post.get_message(N(brucelee), id);
    BOOST_CHECK_EQUAL(msg["closed"].as<bool>(), true);

    BOOST_CHECK_EQUAL(success(), post.create_msg(N(jackiechan), "permlink1", N(brucelee), "permlink"));
    msg = post.get_message(N(brucelee), id);
    BOOST_CHECK_EQUAL(msg["childcount"].as<uint64_t>(), 1);

    BOOST_CHECK_EQUAL(err.msg_exists, post.create_msg(N(brucelee), "permlink"));
    BOOST_CHECK_EQUAL(err.unregistered_user_ + "dan.larimer", post.create_msg(N(dan.larimer), "Hi"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Update message testing.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));
    BOOST_CHECK_EQUAL(success(), post.update_msg(N(brucelee), "permlink",
        "headermssgnew", "bodymssgnew", "languagemssgnew", {{"tagnew"}}, "jsonmetadatanew"));

    check_equal_content(post.get_content(N(brucelee), hash64("permlink")), mvo()
        ("id", hash64("permlink"))
        ("headermssg", "headermssgnew")
        ("bodymssg", "bodymssgnew")
        ("languagemssg", "languagemssgnew")
        ("tags", variants({mvo()("tag", "tagnew")}))
        ("jsonmetadata", "jsonmetadatanew"));
    BOOST_CHECK_EQUAL(err.no_content, post.update_msg(N(brucelee), "permlinknew",
        "headermssgnew", "bodymssgnew", "languagemssgnew", {{"tagnew"}}, "jsonmetadatanew"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delete_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Delete message testing.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(jackiechan), "child", N(brucelee), "permlink"));

    BOOST_TEST_MESSAGE("--- fail then delete non-existing post and post with child");
    BOOST_CHECK_EQUAL(err.no_message, post.delete_msg(N(jackiechan), "permlink1"));
    BOOST_CHECK_EQUAL(err.delete_children, post.delete_msg(N(brucelee), "permlink"));

    BOOST_TEST_MESSAGE("--- success when delete child");
    BOOST_CHECK_EQUAL(success(), post.delete_msg(N(jackiechan), "child"));
    BOOST_TEST_MESSAGE("--- success delete when no more children");
    BOOST_CHECK_EQUAL(success(), post.delete_msg(N(brucelee), "permlink"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(upvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Upvote testing.");
    init();
    init_params();

    auto permlink = "permlink";
    auto vote_brucelee = [&](auto weight){ return post.upvote(N(brucelee), N(brucelee), permlink, weight); };
    auto vote_jackie = [&](auto weight){ return post.upvote(N(jackiechan), N(brucelee), permlink, weight); };

    BOOST_TEST_MESSAGE("--- fail on non-existing message");
    BOOST_CHECK_EQUAL(err.no_message, vote_brucelee(123));
    BOOST_TEST_CHECK(post.get_vote(N(brucelee), 1).is_null());

    BOOST_TEST_MESSAGE("--- succeed on initial upvote");
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), permlink));
    BOOST_CHECK_EQUAL(success(), vote_brucelee(123));
    auto _vote = mvo()("id",0)("message_id",hash64(permlink))("voter","brucelee")("count",1);   // TODO: time
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
    BOOST_CHECK_EQUAL(err.upvote_near_close, vote_jackie(cfg::_100percent));
    produce_blocks(seconds_to_blocks(post.upvote_lockout) - 1);
    BOOST_CHECK_EQUAL(err.upvote_near_close, vote_jackie(cfg::_100percent));

    BOOST_TEST_MESSAGE("--- succeed vote after cashout");
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_jackie(cfg::_100percent));
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 1), mvo(_vote)("count", -1));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(downvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Downvote testing.");
    init();
    init_params();
    auto permlink = "permlink";
    auto vote_brucelee = [&](auto weight){ return post.downvote(N(brucelee), N(brucelee), permlink, weight); };
    auto vote_jackie = [&](auto weight){ return post.downvote(N(jackiechan), N(brucelee), permlink, weight); };

    BOOST_TEST_MESSAGE("--- fail on non-existing message");
    BOOST_CHECK_EQUAL(err.no_message, vote_brucelee(123));
    BOOST_TEST_CHECK(post.get_vote(N(brucelee), 1).is_null());

    BOOST_TEST_MESSAGE("--- succeed on initial downvote");
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), permlink));
    BOOST_CHECK_EQUAL(success(), vote_brucelee(123));
    auto _vote = mvo()("id",0)("message_id",hash64(permlink))("voter","brucelee")("count",1);   // TODO: time
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
    BOOST_CHECK_EQUAL(success(), vote_jackie(cfg::_100percent));
    CHECK_MATCHING_OBJECT(post.get_vote(N(brucelee), 1), mvo(_vote)("count", -1));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(unvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Unvote testing.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(err.no_message, post.unvote(N(brucelee), N(brucelee), "permlink"));

    // TODO: test fail on initial unvote
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(brucelee), N(brucelee), "permlink", 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), N(brucelee), "permlink"));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), N(brucelee), "permlink", 333));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), N(brucelee), "permlink"));
    produce_block();

    BOOST_CHECK_EQUAL(err.vote_same_weight, post.unvote(N(brucelee), N(brucelee), "permlink"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(mixed_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Mixed vote testing.");
    init();
    init_params();
    BOOST_TEST_MESSAGE("--- test that each vote type increases votes count");
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), N(brucelee), "permlink", 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.unvote(N(brucelee), N(brucelee), "permlink"));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.upvote(N(brucelee), N(brucelee), "permlink", 321));
    produce_block();

    auto vote = post.get_vote(N(brucelee), 0);
    BOOST_CHECK_EQUAL(vote["count"].as<uint64_t>(), 3);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delete_post_with_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Delete post with vote testing.");   // TODO: move to "delete" test ?
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "upvote-me"));
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(chucknorris), "downvote-me"));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(chucknorris), N(brucelee), "upvote-me", 321));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(brucelee), N(chucknorris), "downvote-me", 123));
    produce_block();
    BOOST_CHECK_EQUAL(success(), post.delete_msg(N(chucknorris), "downvote-me"));
    // BOOST_CHECK_EQUAL(err.delete_rshares, post.delete_msg(N(brucelee), "upvote-me"));    // TODO:
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(nesting_level_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("nesting level test.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink0"));
    size_t i = 0;
    for (; i < post.max_comment_depth; i++) {
        BOOST_CHECK_EQUAL(success(), post.create_msg(
            N(brucelee), "permlink" + std::to_string(i+1),
            N(brucelee), "permlink" + std::to_string(i)));
    }
    BOOST_CHECK_EQUAL(err.max_comment_depth, post.create_msg(
        N(brucelee), "permlink" + std::to_string(i+1),
        N(brucelee), "permlink" + std::to_string(i)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(comments_cashout_time_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("comments_cashout_time_test.");
    init();
    init_params();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(brucelee), "permlink"));
    auto need_blocks = seconds_to_blocks(post.window);
    auto wait_blocks = 5;
    produce_blocks(wait_blocks);
    need_blocks -= wait_blocks;
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(chucknorris), "comment_permlink", N(brucelee), "permlink"));
        
    BOOST_TEST_MESSAGE("--- creating " << need_blocks << " blocks");    
    produce_blocks(need_blocks);
    
    BOOST_TEST_MESSAGE("--- checking that messages wasn't closed.");
    BOOST_CHECK_EQUAL(post.get_message(N(brucelee), hash64("permlink"))["closed"].as<bool>(), false);
    BOOST_CHECK_EQUAL(post.get_message(N(chucknorris), hash64("comment_permlink"))["closed"].as<bool>(), false);
    
    produce_block();
    
    BOOST_TEST_MESSAGE("--- checking that messages was closed.");
    BOOST_CHECK_EQUAL(post.get_message(N(brucelee), hash64("permlink"))["closed"].as<bool>(), true);
    BOOST_CHECK_EQUAL(post.get_message(N(chucknorris), hash64("comment_permlink"))["closed"].as<bool>(), true);
    
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(jackiechan), "sorry guys i'm late", N(brucelee), "permlink"));
    produce_block();
    
    BOOST_TEST_MESSAGE("--- checking that closed message comment was closed.");
    BOOST_CHECK_EQUAL(post.get_message(N(jackiechan), hash64("sorry guys i'm late"))["closed"].as<bool>(), true);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
