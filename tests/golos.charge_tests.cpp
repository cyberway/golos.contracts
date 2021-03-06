#include "golos_tester.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "contracts.hpp"
#include "../golos.vesting/config.hpp"
#include "golos.charge_test_api.hpp"
#define UNIT_TEST_ENV
#include "../golos.publication/types.h"

namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;
using namespace fixed_point_utils;

static const auto _token_name = "GOLOS";
static const auto _token_precision = 3;
static const auto _vesting_precision = 6;
static const auto _token_sym = symbol(_token_precision, _token_name);
static const auto _vesting_sym = symbol(_vesting_precision, _token_name);

class golos_charge_tester : public golos_tester {
protected:
    golos_vesting_api vest;
    cyber_token_api token;
    golos_charge_api charge;
    vector<account_name> _users;

public:

    golos_charge_tester()
        : golos_tester(cfg::charge_name)
        , vest({this, cfg::vesting_name, _vesting_sym})
        , token({this, cfg::token_name, _token_sym})
        , charge({this, _code, _token_sym})
        , _users{N(alice), N(bob)}
    {
        create_accounts({_code, cfg::token_name, cfg::vesting_name, cfg::control_name, cfg::issuer_name, cfg::publish_name});
        create_accounts(_users);

        set_authority(cfg::issuer_name, cfg::invoice_name, create_code_authority({cfg::charge_name}), "active");
        link_authority(cfg::issuer_name, vest._code, cfg::invoice_name, N(retire));

        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        vest.add_changevest_auth_to_issuer(cfg::issuer_name, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        charge.initialize_contract();
    }

    void init(int64_t issuer_funds, int64_t user_vesting_funds) {
        auto total_funds = issuer_funds + _users.size() * user_vesting_funds;
        // token.create_invoice_authority(cfg::issuer_name, {cfg::charge_name});
        BOOST_CHECK_EQUAL(success(), token.create(cfg::issuer_name, asset(total_funds, _token_sym)));
        produce_block();

        BOOST_CHECK_EQUAL(success(), token.issue(cfg::issuer_name, cfg::issuer_name, asset(total_funds, _token_sym), "HERE COULD BE YOUR ADVERTISEMENT"));
        produce_block();

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(cfg::issuer_name, _token_sym, cfg::control_name));
        produce_block();
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _token_sym, u));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.transfer(cfg::issuer_name, u, asset(user_vesting_funds, _token_sym)));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.transfer(u, cfg::vesting_name, asset(user_vesting_funds, _token_sym)));
            BOOST_CHECK_EQUAL(success(), vest.unlock_limit(u, asset(user_vesting_funds * 10, _token_sym)));
            produce_block();
        }
        produce_block();
    }

protected:
    struct errors: contract_error_messages {
        const string cutoff = amsg("not enough power");
        const string stored_not_found = amsg("itr == storedvals_index.end()");
        const string insufficient_vesting = amsg("insufficient vesting amount");

        const string vesting_var = amsg("vesting disabled, function must not contain `v` variable");
        const string use_vesting = amsg("can't use vesting for charge");
    } err;

};

BOOST_AUTO_TEST_SUITE(golos_charge_tests)

BOOST_FIXTURE_TEST_CASE(basic_tests, golos_charge_tester) try {
    BOOST_TEST_MESSAGE("Basic charge tests");
    init(1, 100);
    const auto issuer = cfg::issuer_name;
    const auto user = _users[0];
    const uint8_t c_id = 'c';
    const int64_t price = 100;
    const int64_t cutoff = 1000;
    const int64_t raw_cutoff = cutoff << 12;;
    const auto max_arg = static_cast<uint64_t>(std::numeric_limits<fixp_t>::max());
    BOOST_CHECK_EQUAL(success(), charge.set_restorer(issuer, c_id, "t*" + std::to_string(cutoff), max_arg, max_arg, max_arg));

    auto use = [&](auto price, auto vesting_price) {
        return charge.use(issuer, user, c_id, price << 12, raw_cutoff, vesting_price);
    };
    auto use0 = [&](auto price) {
        return use(price, 0);
    };
    for (size_t i = 0; i < 10; i++) {
        BOOST_TEST_MESSAGE("--- use_" << i);
        BOOST_CHECK_EQUAL(success(), use0(price));
        produce_block();
    }
    BOOST_TEST_MESSAGE("--- try to use");
    BOOST_CHECK_EQUAL(err.cutoff, use0(price));
    produce_block();
    BOOST_TEST_MESSAGE("--- waiting 900");
    _produce_block(seconds(900));
    BOOST_TEST_MESSAGE("--- try to use (cutoff == 1000)");
    BOOST_CHECK_EQUAL(err.cutoff, use0(price*10));
    produce_block();

#   ifndef CHARGE_DISABLE_VESTING
    BOOST_TEST_MESSAGE("--- try to use (cutoff == 1000, vesting_price = 101)");
    BOOST_CHECK_EQUAL(err.insufficient_vesting, use(price*10, 101));
    produce_block();
    BOOST_TEST_MESSAGE("--- use (cutoff == 1000, vesting_price = 100)");
    BOOST_CHECK_EQUAL(success(), use(price*10, 100));
    produce_block();
    BOOST_TEST_MESSAGE("--- try to use (cutoff == 1000, vesting_price = 1)");
    BOOST_CHECK_EQUAL(err.insufficient_vesting, use(price*10, 1));
    produce_block();
#   endif

    BOOST_TEST_MESSAGE("--- waiting 100");
    _produce_block(seconds(100));
    BOOST_TEST_MESSAGE("--- use (cutoff == 1000)");
    BOOST_CHECK_EQUAL(success(), use0(price));
    produce_block();

#   ifndef CHARGE_DISABLE_STORABLE
    BOOST_TEST_MESSAGE("--- use_and_store (42)");
    BOOST_CHECK_EQUAL(success(), charge.use_and_store(issuer, user, c_id, 42, price*10 << 12));
    produce_block();
    BOOST_TEST_MESSAGE("--- try to remove_stored (43)");
    BOOST_CHECK_EQUAL(err.stored_not_found, charge.remove_stored(issuer, user, c_id, 43));
    produce_block();
    BOOST_TEST_MESSAGE("--- remove_stored (42)");
    BOOST_CHECK_EQUAL(success(), charge.remove_stored(issuer, user, c_id, 42));
    produce_block();
    BOOST_TEST_MESSAGE("--- try to remove_stored (42)");
    BOOST_CHECK_EQUAL(err.stored_not_found, charge.remove_stored(issuer, user, c_id, 42));
    produce_block();
#   endif
} FC_LOG_AND_RETHROW()

#ifdef CHARGE_DISABLE_VESTING
BOOST_FIXTURE_TEST_CASE(disabled_vesting, golos_charge_tester) try {
    BOOST_TEST_MESSAGE("Test disabled vesting");
    init(1, 100);
    const auto issuer = cfg::issuer_name;
    const uint8_t c_id = 'c';
    const auto max_arg = static_cast<uint64_t>(std::numeric_limits<fixp_t>::max());

    BOOST_TEST_MESSAGE("--- test set restorer action");
    BOOST_CHECK_EQUAL(err.vesting_var, charge.set_restorer(issuer, c_id, "v", max_arg, max_arg, max_arg));
    BOOST_CHECK_EQUAL(err.vesting_var, charge.set_restorer(issuer, c_id, "t*v", max_arg, max_arg, max_arg));
    BOOST_CHECK_EQUAL(err.vesting_var, charge.set_restorer(issuer, c_id, "v*t", max_arg, max_arg, max_arg));
    BOOST_CHECK_EQUAL(err.vesting_var, charge.set_restorer(issuer, c_id, "5*v+t", max_arg, max_arg, max_arg));
    BOOST_CHECK_EQUAL(err.vesting_var, charge.set_restorer(issuer, c_id, "contains-v-char", max_arg, max_arg, max_arg));

    BOOST_TEST_MESSAGE("--- test use action");
    BOOST_CHECK_EQUAL(err.use_vesting, charge.use(issuer, _users[0], c_id, 100, 1000, 1));
    BOOST_CHECK_EQUAL(err.use_vesting, charge.use(issuer, _users[0], c_id, 100, 1000, -1));
} FC_LOG_AND_RETHROW()
#endif

BOOST_AUTO_TEST_SUITE_END()
