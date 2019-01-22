#include "golos_tester.hpp"
#include "golos.referral_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "contracts.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

class golos_referral_tester : public extended_tester {
public:
    golos_referral_tester()
        : extended_tester(cfg::referral_name)
        , referral( {this, cfg::referral_name} )
        , token({this, cfg::token_name, symbol(3, "GLS")})
    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), N(issuer), _code, cfg::token_name, cfg::emission_name});
        step(2);

        install_contract(_code, contracts::referral_wasm(), contracts::referral_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }

    void init_params() {
        auto breakout_parametrs = referral.breakout_parametrs(min_breakout, max_breakout);
        auto expire_parametrs   = referral.expire_parametrs(max_expire);
        auto percent_parametrs  = referral.percent_parametrs(max_percent);
        auto delay_parametrs    = referral.delay_parametrs(delay_clear_old_ref);

        auto params = "[" + breakout_parametrs + "," + expire_parametrs + "," + percent_parametrs + "," + delay_parametrs + "]";
        BOOST_CHECK_EQUAL(success(), referral.set_params(cfg::referral_name, params));
    }

protected:
    // TODO: make contract error messages more clear
    struct errors: contract_error_messages {
        const string referral_exist = amsg("A referral with the same name already exists");
        const string referral_equal = amsg("referral can not be referrer");
        const string min_expire     = amsg("expire < current block time");
        const string max_expire     = amsg("expire > current block time + max_expire");
        const string min_breakout   = amsg("breakout <= min_breakout");
        const string max_breakout   = amsg("breakout > max_breakout");
        const string persent        = amsg("specified parameter is greater than limit");

        const string min_more_than_max = amsg("min_breakout > max_breakout");
        const string negative_minimum  = amsg("min_breakout < 0");
        const string negative_maximum  = amsg("max_breakout < 0");
        const string limit_persent     = amsg("max_percent > 100.00%");

        const string referral_not_exist      = amsg("A referral with this name doesn't exist.");
        const string funds_not_equal         = amsg("Amount of funds doesn't equal.");
    } err;

    golos_referral_api referral;
    cyber_token_api token;

    const asset min_breakout = token.make_asset(10);
    const asset max_breakout = token.make_asset(100);
    const uint64_t max_expire = 600; // 600 sec
    const uint32_t max_percent = 5000; // 50.00%
    const uint32_t delay_clear_old_ref = 650; // 650 sec

    };

BOOST_AUTO_TEST_SUITE(golos_referral_tests)

 BOOST_FIXTURE_TEST_CASE(set_params, golos_referral_tester) try {
     BOOST_TEST_MESSAGE("Test vesting parameters");
     BOOST_TEST_MESSAGE("--- prepare");
     step();

     BOOST_TEST_MESSAGE("--- check that global params not exist");
     BOOST_TEST_CHECK(referral.get_params().is_null());

     init_params();
     step();

     auto obj_params = referral.get_params();
     BOOST_TEST_MESSAGE("--- " + fc::json::to_string(obj_params));
     BOOST_TEST_CHECK(obj_params["breakout_params"]["min_breakout"].as<asset>() == min_breakout);
     BOOST_TEST_CHECK(obj_params["breakout_params"]["max_breakout"].as<asset>() == max_breakout);
     BOOST_TEST_CHECK(obj_params["expire_params"]["max_expire"].as<uint64_t>() == max_expire);
     BOOST_TEST_CHECK(obj_params["percent_params"]["max_percent"].as<uint32_t>() == max_percent);

     auto params = "[" + referral.breakout_parametrs(max_breakout, min_breakout) + "]";
     BOOST_CHECK_EQUAL(err.min_more_than_max, referral.set_params(cfg::referral_name, params));

     params = "[" + referral.breakout_parametrs(token.make_asset(-1), max_breakout) + "]";
     BOOST_CHECK_EQUAL(err.negative_minimum, referral.set_params(cfg::referral_name, params));

     params = "[" + referral.percent_parametrs(10001) + "]";
     BOOST_CHECK_EQUAL(err.limit_persent, referral.set_params(cfg::referral_name, params));

 } FC_LOG_AND_RETHROW()

 BOOST_FIXTURE_TEST_CASE(create_referral_tests, golos_referral_tester) try {
     BOOST_TEST_MESSAGE("Test creating referral");

     init_params();
     step();

     auto time_now = static_cast<uint32_t>(time(nullptr));
     BOOST_CHECK_EQUAL(err.referral_equal, referral.create_referral(N(issuer), N(issuer), time_now, 0, token.make_asset(10)));

     BOOST_CHECK_EQUAL(err.min_expire, referral.create_referral(N(issuer), N(sania), time_now, 0, token.make_asset(10)));
     BOOST_CHECK_EQUAL(err.max_expire, referral.create_referral(N(issuer), N(sania), time_now + 5 /*5 sec*/, 999999999999, token.make_asset(10)));

     auto expire = 8; // sec
     BOOST_CHECK_EQUAL(err.min_breakout, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, token.make_asset(0)));
     BOOST_CHECK_EQUAL(err.max_breakout, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, token.make_asset(110)));

     BOOST_CHECK_EQUAL(err.persent, referral.create_referral(N(issuer), N(sania), 9500, cur_time().to_seconds() + expire, token.make_asset(50)));

     BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, token.make_asset(50)));

     step();
     BOOST_CHECK_EQUAL(err.referral_exist, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, token.make_asset(50)));

 } FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Transfer testing");
    
    init_params(); 
    auto expire = 8;
    auto breakout = 100;

    update_cur_time();

    BOOST_TEST_MESSAGE("--- creating referral 'gls.referral'");
    BOOST_CHECK(!referral.get_referral(cfg::referral_name));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), cfg::referral_name, 500, cur_time().to_seconds() + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(cfg::referral_name)["referral"].as<name>(), cfg::referral_name);

    BOOST_TEST_MESSAGE("--- creating referral 'vania'");
    BOOST_CHECK(!referral.get_referral(N(vania)));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(vania), 500, cur_time().to_seconds() + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(N(vania))["referral"].as<name>(), N(vania));

    BOOST_TEST_MESSAGE("--- issue tokens for users");
    BOOST_CHECK_EQUAL(success(), token.create(cfg::emission_name, token.make_asset(10000)));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(vania), token.make_asset(300), "issue 300 tokens for vania"));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(tania), token.make_asset(300), "issue 300 tokens for tania"));

    BOOST_TEST_MESSAGE("--- checking for asserts");
    BOOST_CHECK_EQUAL(err.referral_not_exist, token.transfer(N(tania), cfg::referral_name, token.make_asset(breakout), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout-1), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout+1), ""));

    BOOST_TEST_MESSAGE("--- checking that transfer was successful");
    BOOST_CHECK_EQUAL(success(), token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout), ""));

    BOOST_TEST_MESSAGE("--- checking that record about referral was removed");
    BOOST_CHECK(!referral.get_referral(N(vania)));
} FC_LOG_AND_RETHROW()
  
BOOST_FIXTURE_TEST_CASE(close_referral_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test close referral");

    init_params();

    auto expire = 8; // sec

    update_cur_time();

    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, token.make_asset(50)));
    BOOST_CHECK_EQUAL(success(), referral.close_old_referrals(cur_time().to_seconds()));
    step();

    auto v_referrals = referral.get_referrals();
    BOOST_TEST_CHECK(v_referrals.size() == 1);
    BOOST_TEST_CHECK(v_referrals.at(0)["referral"].as<name>() == N(sania));
    BOOST_TEST_CHECK(v_referrals.at(0)["referrer"].as<name>() == N(issuer));

    step( golos::seconds_to_blocks(delay_clear_old_ref) );
    v_referrals = referral.get_referrals();
    BOOST_TEST_CHECK(v_referrals.size() == 1);
    step();

    v_referrals = referral.get_referrals();
    BOOST_TEST_CHECK(v_referrals.size() == 0);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
