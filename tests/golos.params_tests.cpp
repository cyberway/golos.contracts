#include "golos_tester.hpp"
#include "golos.emit_test_api.hpp"
#include "golos.ctrl_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "contracts.hpp"
#include "../common/config.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

using symbol_type = symbol;

static const auto _token = symbol_type(6,"TST");

class golos_params_tester : public golos_tester {
protected:
    golos_emit_api emit;
    golos_ctrl_api ctrl;
    golos_vesting_api vest;
    cyber_token_api token;

public:
    golos_params_tester()
        : golos_tester(cfg::emission_name)
        , emit({this, cfg::emission_name})
        , ctrl({this, cfg::control_name})
        , vest({this, cfg::vesting_name, _token})
        , token({this, cfg::token_name, _token})
    {
        create_accounts({cfg::emission_name, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, //_carol,
            cfg::control_name, cfg::vesting_name, cfg::token_name, cfg::workers_name, N(issuer)});
        produce_block();

        // TODO: maybe create separate parameters tester contract to reduce dependencies
        install_contract(cfg::emission_name, contracts::emit_wasm(), contracts::emit_abi());
        install_contract(cfg::control_name, contracts::ctrl_wasm(), contracts::ctrl_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }


    asset dasset(double val = 0) const {
        return token.make_asset(val);
    }

    // constants
    const uint16_t _max_witnesses = 3;
    const uint16_t _smajor_witn_count = _max_witnesses * 2 / 3 + 1;

    const account_name BLOG = N(blog);
    const account_name _alice = N(alice);
    const account_name _bob = N(bob);
    const uint64_t _w[4] = {N(witn1), N(witn2), N(witn3), N(witn4)};
    const size_t _n_w = sizeof(_w) / sizeof(_w[0]);

    const int emit_params_count = 2;

    struct errors: contract_error_messages {
        const string empty             = amsg("empty params not allowed");
        const string bad_variant_order = amsg("parameters must be ordered by variant index");
        const string duplicates        = amsg("params contain several copies of the same parameter");
        const string incomplete        = amsg("must provide all parameters in initial set");
        // const string incomplete(int n) { return amsg("must provide all "+std::to_string(n)+" parameters in initial set");}   // eosio string bugs
        // const string same_value        = amsg("can't set same parameter value");
        const string nothing_changed   = amsg("at least one parameter must change");
        // TODO: test immutable
    } err;

    void prepare_top_witnesses(bool only_create) {
        BOOST_CHECK_EQUAL(success(),
            ctrl.set_params(ctrl.default_params(BLOG, _token, _max_witnesses, 4)));
        produce_block();
        if (only_create)
            return;
        ctrl.prepare_multisig(BLOG);
        produce_block();

        const string _test_key = string(fc::crypto::config::public_key_legacy_prefix)
            + "6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";
        for (int i = 0; i < _n_w; i++) {
            BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[i], _test_key, "localhost"));
        }
        produce_block();

        prepare_balances();
        vector<std::pair<name,name>> votes = {
            {_alice, _w[0]}, {_alice, _w[1]}, {_alice, _w[2]}, {_alice, _w[3]},
            {_bob, _w[0]},   {_bob, _w[1]},
            {_w[0], _w[0]}
        };
        for (const auto& v : votes) {
            BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(v.first, v.second));
        }
        produce_block();
    }

    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), token.create(BLOG, dasset(100500), {cfg::emission_name}));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(BLOG, _token));
        BOOST_CHECK_EQUAL(success(), vest.open(cfg::vesting_name, _token, cfg::vesting_name));
        vector<std::pair<uint64_t,double>> amounts = {
            {_alice, 800}, {_bob, 700},
            {_w[0], 100}
        };
        for (const auto& p : amounts) {
            auto acc = p.first;
            auto amount = p.second;
            BOOST_CHECK_EQUAL(success(), vest.open(acc, _token, acc));
            if (amount > 0) {
                BOOST_CHECK_EQUAL(success(), token.issue(BLOG, acc, dasset(amount), "issue"));
                BOOST_CHECK_EQUAL(success(), token.transfer(acc, cfg::vesting_name, dasset(amount), "buy vesting"));
            } else {
                BOOST_CHECK_EQUAL(success(), token.open(acc, _token, acc));
            }
        };
        CHECK_MATCHING_OBJECT(vest.get_balance(_alice), vest.make_balance(800));
        CHECK_MATCHING_OBJECT(vest.get_balance(_bob), vest.make_balance(700));
        CHECK_MATCHING_OBJECT(vest.get_balance(_w[0]), vest.make_balance(100));
    }
};


BOOST_AUTO_TEST_SUITE(golos_params_tests)

BOOST_FIXTURE_TEST_CASE(set_params, golos_params_tester) try {
    // TODO: Rewrite for golos.cfg
    BOOST_TEST_MESSAGE("Test parameters logic");
    BOOST_TEST_MESSAGE("--- prepare top witnesses");
    prepare_top_witnesses(true);

    BOOST_TEST_MESSAGE("--- check global params initially not set");
    BOOST_TEST_CHECK(emit.get_params().is_null());

    BOOST_TEST_MESSAGE("--- check_params: fail on empty parameters, wrong variant order or several copies of the same parameter");
    BOOST_CHECK_EQUAL(err.incomplete, emit.set_params("[]"));
    auto pools = "['reward_pools',{'pools':[" + emit.pool_json(_bob,0) + "]}]";
    auto pools2 = "['reward_pools',{'pools':[" + emit.pool_json(_alice,1000) + "," + emit.pool_json(_bob,0) + "]}]";
    auto infrate = emit.infrate_json(0,0);
    auto infrate2 = emit.infrate_json(500,500);
    BOOST_CHECK_EQUAL(err.bad_variant_order, emit.set_params("[" + pools + "," + infrate + "]"));
    BOOST_CHECK_EQUAL(err.duplicates, emit.set_params("[" + infrate + "," + infrate + "]"));
    BOOST_CHECK_EQUAL(err.duplicates, emit.set_params("[" + infrate + "," + infrate2 + "]"));
    BOOST_CHECK_EQUAL(err.duplicates, emit.set_params("[" + pools + "," + pools2 + "]"));

    BOOST_TEST_MESSAGE("--- setparams: fail if first call to setparams action contains not all parameters");
    BOOST_CHECK_EQUAL(err.incomplete, emit.set_params("[" + infrate + "]"));
    BOOST_CHECK_EQUAL(err.incomplete, emit.set_params("[" + emit.infrate_json(1,1) + "]"));

    BOOST_TEST_MESSAGE("--- success on valid parameters and changing parameters");
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate + "," + pools + "]"));
    produce_block();
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate2 + "," + pools2 + "]"));
    produce_block();
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + pools + "]"));
    produce_block();
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate + "]"));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail on empty parameters (params exists)");
    BOOST_CHECK_EQUAL(err.empty, emit.set_params("[]"));
/*
    BOOST_TEST_MESSAGE("--- fail if parameter value not changed");
    BOOST_CHECK_EQUAL(err.same_value, emit.set_params("[" + infrate + "]"));
    BOOST_CHECK_EQUAL(err.same_value, emit.set_params("[" + pools + "]"));
    BOOST_CHECK_EQUAL(err.same_value, emit.set_params("[" + infrate + "," + pools + "]"));
    BOOST_CHECK_EQUAL(err.same_value, emit.set_params("[" + infrate + "," + pools2 + "]"));
    BOOST_CHECK_EQUAL(err.same_value, emit.set_params("[" + infrate2 + "," + pools + "]"));
*/
    BOOST_TEST_MESSAGE("--- fail if no parameters actually change state");
    BOOST_CHECK_EQUAL(err.nothing_changed, emit.set_params("[" + infrate + "]"));
    BOOST_CHECK_EQUAL(err.nothing_changed, emit.set_params("[" + pools + "]"));
    BOOST_CHECK_EQUAL(err.nothing_changed, emit.set_params("[" + infrate + "," + pools + "]"));
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate + "," + pools2 + "]"));
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate2 + "," + pools2 + "]"));
    BOOST_CHECK_EQUAL(success(), emit.set_params("[" + infrate2 + "," + pools + "]"));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
