#include "golos_tester.hpp"
#include "golos.referral_test_api.hpp"
#include "contracts.hpp"
#include "../golos.referral/include/golos.referral/config.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;


class golos_referral_tester : public golos_tester {
protected:
    golos_referral_api referral;

public:

    golos_referral_tester()
        : golos_tester(cfg::referral_name)
        , referral({this, cfg::referral_name})
    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), N(issuer), _code});
        produce_blocks(2);

        install_contract(_code, contracts::referral_wasm(), contracts::referral_abi());
    }

protected:

    // TODO: make contract error messages more clear
    struct errors: contract_error_messages {
        const string referral_exist = amsg("This referral exits");
        const string referral_equal = amsg("referrer is not equal to referral");
        const string min_expire     = amsg("expire < current block time");
        const string max_expire     = amsg("expire > current block time + max_expire");
        const string min_breakout   = amsg("breakout <= min_breakout");
        const string max_breakout   = amsg("breakout > min_breakout");
        const string persent        = amsg("specified parameter is greater than limit");
    } err;
};

BOOST_AUTO_TEST_SUITE(golos_referral_tests)

 BOOST_FIXTURE_TEST_CASE(create_referral_tests, golos_referral_tester) try {
     BOOST_TEST_MESSAGE("Test creating referral");
     BOOST_TEST_MESSAGE("--- create referral");

     auto time_now = static_cast<uint32_t>(time(nullptr));

     BOOST_CHECK_EQUAL(err.referral_equal, referral.create_referral(N(issuer), N(issuer), time_now, 0, asset(10000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.min_expire, referral.create_referral(N(issuer), N(sania), time_now, 0, asset(10000, symbol(4, "GLS"))));
     BOOST_CHECK_EQUAL(err.max_expire, referral.create_referral(N(issuer), N(sania), time_now + 5 /*5 sec*/, 999999999999, asset(10000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.min_breakout, referral.create_referral(N(issuer), N(sania), 500, time_now + 60 /*60 sec*/, asset(0, symbol(4, "GLS"))));
     BOOST_CHECK_EQUAL(err.max_breakout, referral.create_referral(N(issuer), N(sania), 500, time_now + 60 /*60 sec*/, asset(110000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.persent, referral.create_referral(N(issuer), N(sania), 9500, time_now + 60 /*60 sec*/, asset(110000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, time_now + 60 /*60 sec*/, asset(50000, symbol(4, "GLS"))));

     BOOST_CHECK_EQUAL(err.referral_exist, referral.create_referral(N(issuer), N(sania), 500, time_now + 60 /*60 sec*/, asset(50000, symbol(4, "GLS"))));

 } FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
