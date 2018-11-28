#define UNIT_TEST_ENV
#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include "golos.social_test_api.hpp"
#include <common/config.hpp>
#include "contracts.hpp"

using namespace golos;
using namespace eosio::chain;
using namespace eosio::testing;
namespace cfg = golos::config;

static const auto _token_name = "GOLOS";
static const auto _token_precision = 3;
static const auto _token_sym = symbol(_token_precision, _token_name);
static const auto _vesting_precision = 6;
static const auto _vesting_sym = symbol(_vesting_precision, _token_name);

class golos_social_tester : public golos_tester {
protected:
    golos_posting_api post;
    golos_vesting_api vest;
    eosio_token_api token;
    golos_social_api social;

    std::vector<account_name> _users;
public:

    golos_social_tester()
        : golos_tester(N(golos.soc))
        , post({this, _code, _token_sym})
        , vest({this, cfg::vesting_name, _vesting_sym})
        , token({this, cfg::token_name, _token_sym})
        , social({this, cfg::social_name, _token_sym})
        , _users{_code, N(dave), N(erin)} {

        produce_block();
        create_accounts(_users);
        create_accounts({N(issuer), cfg::control_name, cfg::token_name, cfg::vesting_name, cfg::social_name});
        produce_block();

        install_contract(_code, contracts::posting_wasm(), contracts::posting_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::social_name, contracts::social_wasm(), contracts::social_abi());
    }

    void init(int64_t issuer_funds, int64_t user_vesting_funds) {
        BOOST_CHECK_EQUAL(success(), token.open(_code, _token_sym, _code));

        funcparams fn{"0", 1};
        BOOST_CHECK_EQUAL(success(), post.set_rules(fn, fn, fn, 0, 0));

        BOOST_CHECK_EQUAL(success(), token.create(N(issuer), token.make_asset(issuer_funds)));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(issuer)));
        produce_block();

        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _vesting_sym, u));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.issue(N(issuer), u, token.make_asset(user_vesting_funds), "add funds to erin"));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.transfer(u, cfg::vesting_name, token.make_asset(user_vesting_funds), "buy vesting for erin"));
            produce_block();
        }
    }

protected:

    struct errors: contract_error_messages {
        const string cannot_pin_yourself        = amsg("You cannot pin yourself");
        const string cannot_unpin_yourself      = amsg("You cannot unpin yourself");
        const string cannot_block_yourself      = amsg("You cannot block yourself");
        const string cannot_unblock_yourself    = amsg("You cannot unblock yourself");
        const string cannot_pin_blocked         = amsg("You have blocked this account. Unblock it before pinning");
        const string cannot_unpin_not_pinned    = amsg("You have not pinned this account");
        const string already_pinned             = amsg("You already have pinned this account");
        const string cannot_unblock_not_blocked = amsg("You have not blocked this account");
        const string already_blocked            = amsg("You already have blocked this account");
    } err;
};


BOOST_AUTO_TEST_SUITE(social_tests)

BOOST_FIXTURE_TEST_CASE(golos_pinning_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos pinning test");

    BOOST_TEST_MESSAGE("--- pin: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_pin_yourself, social.pin(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unpin_yourself, social.unpin(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.pin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_pinned, social.pin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unpin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.cannot_unpin_not_pinned, social.unpin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.pin(N(dave), N(erin)));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_blocking_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos blocking test");

    BOOST_TEST_MESSAGE("--- block: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_block_yourself, social.block(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unblock_yourself, social.unblock(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), social.block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_blocked, social.block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unblock(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.cannot_unblock_not_blocked, social.unblock(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), social.block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin (fail)");
    BOOST_CHECK_EQUAL(err.cannot_pin_blocked, social.pin(N(dave), N(erin)));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_reputation_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos reputation test");

    auto bignum = 1000000;
    init(bignum, 10);

    produce_block();

    int perm_i = 1;
    std::string permlink;
    auto new_permlink = [&]() {
        permlink = "permlink" + std::to_string(++perm_i);
    };

    BOOST_TEST_MESSAGE("--- hack attempt: erin wants to increase his reputation, it should not be success");
    auto hack_attempt = social.push(N(changereput), N(erin), social.args()
        ("voter", N(dave))
        ("author", N(erin))
        ("rhares", 1000000)
    );
    BOOST_CHECK_NE(success(), hack_attempt);

    BOOST_TEST_MESSAGE("--- upvote: dave erin 1000");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(erin), permlink));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(dave), N(erin), permlink, 1000));
    produce_block();

    auto erin_rep = social.get_reputation(N(erin));
    BOOST_CHECK(!erin_rep.is_null());
    BOOST_CHECK_GT(erin_rep["reputation"].as<int64_t>(), 0);

    BOOST_TEST_MESSAGE("--- downvote: erin dave 200");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(dave), permlink));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(erin), N(dave), permlink, 200));
    produce_block();

    BOOST_TEST_MESSAGE("--- downvote: dave erin 200 (check rule #1)");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(erin), permlink));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(dave), N(erin), permlink, 10));
    produce_block();

    auto new_erin_rep = social.get_reputation(N(erin));
    BOOST_CHECK_EQUAL(erin_rep["reputation"].as<int64_t>(), new_erin_rep["reputation"].as<int64_t>());

    BOOST_TEST_MESSAGE("--- upvote: erin dave 100");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(dave), permlink));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(erin), N(dave), permlink, 100));
    produce_block();

    BOOST_TEST_MESSAGE("--- downvote: dave erin 200 (check rule #2)");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(erin), permlink));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(dave), N(erin), permlink, 10));
    produce_block();

    auto new_erin_rep2 = social.get_reputation(N(erin));
    BOOST_CHECK_EQUAL(erin_rep["reputation"].as<int64_t>(), new_erin_rep2["reputation"].as<int64_t>());

    BOOST_TEST_MESSAGE("--- upvote: erin dave 100");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(dave), permlink));
    BOOST_CHECK_EQUAL(success(), post.upvote(N(erin), N(dave), permlink, 10000));
    produce_block();

    BOOST_TEST_MESSAGE("--- downvote: dave erin 200 (conforming rules)");
    new_permlink();
    BOOST_CHECK_EQUAL(success(), post.create_msg(N(erin), permlink));
    BOOST_CHECK_EQUAL(success(), post.downvote(N(dave), N(erin), permlink, 10));
    produce_block();

    auto new_erin_rep3 = social.get_reputation(N(erin));
    BOOST_CHECK_GT(erin_rep["reputation"].as<int64_t>(), new_erin_rep3["reputation"].as<int64_t>());

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
