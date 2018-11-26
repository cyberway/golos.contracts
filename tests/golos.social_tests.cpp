#define UNIT_TEST_ENV
#include "golos_tester.hpp"
#include "golos.social_test_api.hpp"
#include <common/config.hpp>
#include "contracts.hpp"

using namespace golos;
using namespace eosio::chain;
using namespace eosio::testing;
namespace cfg = golos::config;


class golos_social_tester : public golos_tester {
protected:
    symbol _sym;
    golos_social_api social;

    std::vector<account_name> _users;
public:

    golos_social_tester()
        : golos_tester(N(golos.soc))
        , _sym(0, "DUMMY")
        , social({this, cfg::social_name, _sym})
        , _users{_code, N(dave), N(erin)} {

        produce_block();
        create_accounts(_users);
        create_accounts({cfg::social_name});
        produce_block();

        install_contract(cfg::social_name, contracts::social_wasm(), contracts::social_abi());
    }

    action_result pin(account_name pinner, account_name pinning) {
        auto ret = social.pin(pinner, pinning);
        return ret;
    }

    action_result unpin(account_name pinner, account_name pinning) {
        auto ret = social.unpin(pinner, pinning);
        return ret;
    }

    action_result block(account_name blocker, account_name blocking) {
        auto ret = social.block(blocker, blocking);
        return ret;
    }

    action_result unblock(account_name blocker, account_name blocking) {
        auto ret = social.unblock(blocker, blocking);
        return ret;
    }

    std::vector<variant> get_pinblocks(account_name pinner_blocker) {
        return social.get_pinblocks(pinner_blocker);
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
    BOOST_CHECK_EQUAL(err.cannot_pin_yourself, pin(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unpin_yourself, unpin(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin");
    BOOST_CHECK_EQUAL(success(), pin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_pinned, pin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin");
    BOOST_CHECK_EQUAL(success(), unpin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.cannot_unpin_not_pinned, unpin(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin");
    BOOST_CHECK_EQUAL(success(), pin(N(dave), N(erin)));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_blocking_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos blocking test");

    BOOST_TEST_MESSAGE("--- block: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_block_yourself, block(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unblock_yourself, unblock(N(dave), N(dave)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_blocked, block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin");
    BOOST_CHECK_EQUAL(success(), unblock(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.cannot_unblock_not_blocked, unblock(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), block(N(dave), N(erin)));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin (fail)");
    BOOST_CHECK_EQUAL(err.cannot_pin_blocked, pin(N(dave), N(erin)));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
