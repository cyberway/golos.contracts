#include "golos_tester.hpp"
#include "golos.vesting_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include "contracts.hpp"
#include "../golos.emit/include/golos.emit/config.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;


static const auto _symbol = symbol(4,"GOLOS");

class golos_vesting_tester : public golos_tester {
protected:
    golos_vesting_api vest;
    eosio_token_api token;

public:

    golos_vesting_tester()
        : golos_tester(cfg::vesting_name, _symbol.to_symbol_code().value)
        , vest({this, cfg::vesting_name})
        , token({this, cfg::token_name})
    {
        vest._symbol =
        token._symbol = _symbol;

        create_accounts({N(sania), N(pasha), N(tania),
            cfg::vesting_name, cfg::control_name, cfg::token_name, cfg::emission_name, N(golos.issuer)});
        produce_blocks(2);

        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        // install_contract(cfg::control_name, contracts::ctrl_wasm(), contracts::ctrl_abi());
    }


    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
        BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(sania), vest.make_asset(500), "issue tokens sania"));
        BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(pasha), vest.make_asset(500), "issue tokens pasha"));
        produce_blocks(1);

        BOOST_CHECK_EQUAL(success(), vest.open(N(sania), _symbol, N(sania)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(pasha), _symbol, N(pasha)));
        produce_blocks(1);

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(golos.issuer), _symbol));
        BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
        BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
        produce_blocks(1);

        CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(400)));
        CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", vest.asset_str(400)));
        CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100));
        CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100));
    }

    void prepare_delegation(double amount = 20, uint16_t rate = 0, uint8_t strategy = 1) {
        prepare_balances();
        BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(amount), rate, strategy));
        CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount));
        CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount));
    }

};

BOOST_AUTO_TEST_SUITE(golos_vesting_tests)

BOOST_FIXTURE_TEST_CASE(test_create_tokens, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(cfg::token_name, vest.make_asset(100000)));
    CHECK_MATCHING_OBJECT(token.get_stats(), mvo()
        ("supply", vest.asset_str(0))
        ("max_supply", vest.asset_str(100000))
        ("issuer", name(cfg::token_name).to_string())
    );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_create_vesting_for_nonexistent_token, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL("assertion failure with message: unable to find key", vest.create_vesting(N(golos.issuer), _symbol));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_create_vesting_by_not_issuer, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(cfg::token_name, vest.make_asset(100000)));
    BOOST_CHECK_EQUAL("assertion failure with message: Only token issuer can create it", vest.create_vesting(N(golos.issuer), _symbol));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_create_vesting, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(golos.issuer), _symbol));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_issue_tokens_accounts, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(sania), vest.make_asset(500), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(pasha), vest.make_asset(500), "issue tokens pasha"));
    produce_blocks(1);

    CHECK_MATCHING_OBJECT(token.get_stats(), mvo()
        ("supply", vest.asset_str(1000))
        ("max_supply", vest.asset_str(100000))
        ("issuer", "golos.issuer")
    );
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(500)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", vest.asset_str(500)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_convert_token_to_vesting, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(sania), vest.make_asset(500), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(pasha), vest.make_asset(500), "issue tokens pasha"));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.open(N(sania), _symbol, N(sania)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(pasha), _symbol, N(pasha)));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(golos.issuer), _symbol));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
    produce_blocks(1);

    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_convert_vesting_to_token, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(sania), vest.make_asset(500), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(pasha), vest.make_asset(500), "issue tokens pasha"));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.open(N(sania), _symbol, N(sania)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(pasha), _symbol, N(pasha)));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(golos.issuer), _symbol));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, vest.make_asset(100), "buy vesting"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, vest.make_asset(100), "buy vesting"));
    produce_blocks(1);

    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100));

    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(13)));
    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));
    auto delegated_auth = authority(1, {}, {
        {.permission = {cfg::vesting_name, cfg::code_name}, .weight = 1}
    });
    set_authority(cfg::vesting_name, cfg::active_name, delegated_auth);
    produce_blocks(31);

    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(99));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(401)));
    produce_blocks(270);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(90));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(410)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_cancel_convert_vesting_to_token, golos_vesting_tester) try {
    BOOST_CHECK_EQUAL(success(), token.create(N(golos.issuer), vest.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(sania), vest.make_asset(500), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(N(golos.issuer), N(pasha), vest.make_asset(500), "issue tokens pasha"));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.open(N(sania), _symbol, N(sania)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(pasha), _symbol, N(pasha)));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), vest.create_vesting(N(golos.issuer), _symbol));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, vest.make_asset(100), "convert token to vesting"));
    produce_blocks(1);

    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", vest.asset_str(400)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100));

    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(13)));
    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));
    auto delegated_auth = authority(1, {}, {
        {.permission = {cfg::vesting_name, cfg::code_name}, .weight = 1}
    });
    set_authority(cfg::vesting_name, cfg::active_name, delegated_auth);
    produce_blocks(31);

    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(99));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(401)));
    produce_blocks(120);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(95));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(405)));

    BOOST_CHECK_EQUAL(success(), vest.cancel_convert_vesting(N(sania), vest.make_asset(0)));
    produce_blocks(120);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(95));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", vest.asset_str(405)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_sender_equals_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(error("assertion failure with message: You can not delegate to yourself"),
        vest.delegate_vesting(N(sania), N(sania), vest.make_asset(15), 0, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_payout_strategy_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(error("assertion failure with message: not valid value payout_strategy"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(15), 0, -1));
    BOOST_CHECK_EQUAL(error("assertion failure with message: not valid value payout_strategy"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(15), 0, 2));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_zero_quantity_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(error("assertion failure with message: the number of tokens should not be less than 0"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(0), 0, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_min_amount_delegate_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(error("assertion failure with message: Insufficient funds for delegation"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(0.0001), 0, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_interest_rate_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(error("assertion failure with message: Exceeded the percentage of delegated vesting"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(15), 50, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_insufficient_funds_test_delegate_vesting, golos_vesting_tester) try {
    prepare_balances();
    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(tanya), vest.make_asset(15), 0, 0));
    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(80)));
    BOOST_CHECK_EQUAL(error("assertion failure with message: insufficient funds for delegation"),
        vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(15), 0, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_delegate_vesting, golos_vesting_tester) try {
    prepare_delegation(15, 0, 0);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_insufficient_funds_test_undelegate_vesting, golos_vesting_tester) try {
    prepare_delegation();
    produce_blocks(100);
    BOOST_CHECK_EQUAL(error("assertion failure with message: Insufficient funds for undelegation"),
        vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(4)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_frozen_tokens_test_undelegate_vesting, golos_vesting_tester) try {
    prepare_delegation();
    BOOST_CHECK_EQUAL(error("assertion failure with message: Tokens are frozen until the end of the period"),
        vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(5)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_lack_of_funds_test_undelegate_vesting, golos_vesting_tester) try {
    prepare_delegation();
    produce_blocks(100);
    BOOST_CHECK_EQUAL(error("assertion failure with message: There are not enough delegated tools for output"),
                         vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(24)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(error_delegated_vesting_withdrawn_test_undelegate_vesting, golos_vesting_tester) try {
    prepare_delegation();
    produce_blocks(100);
    BOOST_CHECK_EQUAL(error("assertion failure with message: delegated vesting withdrawn"),
                         vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(18)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_undelegate_vesting, golos_vesting_tester) try {
    prepare_delegation();
    produce_blocks(100);
    BOOST_CHECK_EQUAL(success(), vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(5)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, 15));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, 20));

    vest.timeout(cfg::vesting_name);
    auto delegated_auth = authority(1, {}, {
        {.permission = {cfg::vesting_name, cfg::code_name}, .weight = 1}
    });
    set_authority(cfg::vesting_name, cfg::active_name, delegated_auth);
    produce_blocks(31);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, 15));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, 15));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
