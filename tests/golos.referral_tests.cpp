#include "golos_tester.hpp"
#include "golos.referral_test_api.hpp"
#include "eosio.token_test_api.hpp"
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
        , token({this, cfg::token_name, symbol(4, "GLS")})
    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), N(issuer), _code, cfg::token_name, cfg::emission_name});
        step(2);

        install_contract(_code, contracts::referral_wasm(), contracts::referral_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }

    void init_params() {
        auto breakout_parametrs = referral.breakout_parametrs(min_breakout, max_breakout);
        auto expire_parametrs   = referral.expire_parametrs(max_expire);
        auto percent_parametrs  = referral.percent_parametrs(max_persent);
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
        const string limit_persent     = amsg("max_perсent > 100.00%");

        const string referral_not_exist      = amsg("A referral with this name doesn't exist.");
        const string funds_not_equal         = amsg("Amount of funds doesn't equal.");
    } err;

    const asset min_breakout = asset(10000,  symbol(4,"GLS"));
    const asset max_breakout = asset(100000, symbol(4,"GLS"));
    const uint64_t max_expire = 600; // 600 sec
    const uint32_t max_persent = 5000; // 50.00%
    const uint32_t delay_clear_old_ref = 650; // 650 sec

    golos_referral_api referral;
    eosio_token_api token;
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
     BOOST_TEST_CHECK(obj_params["percent_params"]["max_perсent"].as<uint32_t>() == max_persent);

     auto params = "[" + referral.breakout_parametrs(max_breakout, min_breakout) + "]";
     BOOST_CHECK_EQUAL(err.min_more_than_max, referral.set_params(cfg::referral_name, params));

     params = "[" + referral.breakout_parametrs(asset(-1, symbol(4,"GLS")), max_breakout) + "]";
     BOOST_CHECK_EQUAL(err.negative_minimum, referral.set_params(cfg::referral_name, params));

     params = "[" + referral.percent_parametrs(10001) + "]";
     BOOST_CHECK_EQUAL(err.limit_persent, referral.set_params(cfg::referral_name, params));

 } FC_LOG_AND_RETHROW()

 BOOST_FIXTURE_TEST_CASE(create_referral_tests, golos_referral_tester) try {
     BOOST_TEST_MESSAGE("Test creating referral");

     init_params();
     step();

     auto time_now = static_cast<uint32_t>(time(nullptr));
     BOOST_CHECK_EQUAL(err.referral_equal, referral.create_referral(N(issuer), N(issuer), time_now, 0, asset(10000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.min_expire, referral.create_referral(N(issuer), N(sania), time_now, 0, asset(10000, symbol(4, "GLS"))));
     BOOST_CHECK_EQUAL(err.max_expire, referral.create_referral(N(issuer), N(sania), time_now + 5 /*5 sec*/, 999999999999, asset(10000, symbol(4, "GLS"))));

     auto expire = 8; // sec
     BOOST_CHECK_EQUAL(err.min_breakout, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, asset(0, symbol(4, "GLS"))));
     BOOST_CHECK_EQUAL(err.max_breakout, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, asset(110000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.persent, referral.create_referral(N(issuer), N(sania), 9500, cur_time().to_seconds() + expire, asset(50000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, asset(50000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.referral_exist, referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, asset(50000, symbol(4, "GLS"))));

 } FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Transfer testing");
    
    init_params(); 
    auto expire = 8;
    auto breakout = 10;

    BOOST_CHECK(!referral.get_referral(cfg::referral_name));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), cfg::referral_name, 500, cur_time().to_seconds() + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(cfg::referral_name)["referral"].as<name>(), cfg::referral_name);
    BOOST_CHECK(!referral.get_referral(N(vania)));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(vania), 500, cur_time().to_seconds() + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(N(vania))["referral"].as<name>(), N(vania));
    BOOST_CHECK_EQUAL(success(), token.create(cfg::emission_name, token.make_asset(10000)));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(vania), token.make_asset(30), "issue 30 tokens for vania"));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(tania), token.make_asset(30), "issue 30 tokens for tania"));
    BOOST_CHECK_EQUAL(err.referral_not_exist, token.transfer(N(tania), cfg::referral_name, token.make_asset(breakout), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout-1), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout+1), ""));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout), ""));
    BOOST_CHECK(!referral.get_referral(N(vania)));
} FC_LOG_AND_RETHROW()
  
BOOST_FIXTURE_TEST_CASE(close_referral_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test close referral");

    init_params();

    auto expire = 8; // sec
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, cur_time().to_seconds() + expire, asset(50000, symbol(4, "GLS"))));
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
