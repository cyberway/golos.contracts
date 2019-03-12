#include "golos_tester.hpp"
#include "golos.emit_test_api.hpp"
#include "golos.ctrl_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "contracts.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

static const auto _token = symbol(3,"GLS");

class golos_emit_tester : public golos_tester {
protected:
    golos_emit_api emit;
    cyber_token_api token;

public:
    golos_emit_tester()
        : golos_tester(cfg::emission_name)
        , emit({this, _code})
        , token({this, cfg::token_name, _token})
    {
        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, _carol,
            cfg::control_name, cfg::token_name, cfg::workers_name, cfg::vesting_name});
        produce_block();

        install_contract(_code, contracts::emit_wasm(), contracts::emit_abi());
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
    const account_name _carol = N(carol);
    const uint64_t _w[5] = {N(witn1), N(witn2), N(witn3), N(witn4), N(witn5)};
    const size_t _n_w = sizeof(_w) / sizeof(_w[0]);
    const uint16_t _interval = 15*60;

    vector<account_name> witness_vect(size_t n) const {
        vector<account_name> r;
        auto l = std::min(n, _n_w);
        r.reserve(l);
        while (l--) {
            r.push_back(_w[l]);
        }
        return r;
    }

    struct errors: contract_error_messages {
        const string no_pool_account   = amsg("pool account must exist");
        const string interval0         = amsg("interval must be positive");
        const string no_vesting_acc    = amsg("Vesting account doesn't exist.");
    } err;

    const string _test_key = string(fc::crypto::config::public_key_legacy_prefix)
        + "6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";

    // prepare; TODO: reuse ctrl code
    enum prep_step {
        step_0,
        step_only_create,
        step_reg_witnesses,
        step_vote_witnesses
    };

    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), token.create(_code, dasset(100500'000)));
        BOOST_CHECK_EQUAL(success(), token.issue(_code, BLOG, dasset(4600'000), "issue"));
    }
};


BOOST_AUTO_TEST_SUITE(golos_emit_tests)

BOOST_FIXTURE_TEST_CASE(start_emission_test, golos_emit_tester) try {
    BOOST_TEST_MESSAGE("Test emission start");

    BOOST_TEST_MESSAGE("--- prepare");
    prepare_balances();
    produce_block();

    auto content_pool = emit.pool_json(BLOG, 6667-667);
    auto vesting_pool = emit.pool_json(cfg::vesting_name, 2667-267);
    auto witness_pool = emit.pool_json(cfg::control_name, 0);
    auto workers_pool = emit.pool_json(cfg::workers_name, 1000);
    auto pools = "{'pools':[" + content_pool + "," + witness_pool + "," + vesting_pool + "," + workers_pool + "]}";
    auto interval = "['emit_interval',{'value':'" + std::to_string(_interval) + "'}]";
    auto vesting_acc = "['vesting_acc',{'value':'" + cfg::vesting_name.to_string() + "'}]";
    auto params = "[" + emit.infrate_json(1500, 95, 250000) + ",['reward_pools'," + pools + "]," + emit.token_symbol_json(_token) + "," + interval + "," + vesting_acc + "]";
    BOOST_CHECK_EQUAL(success(), emit.set_params(params));

    BOOST_TEST_MESSAGE("--- start succeed");
    BOOST_CHECK_EQUAL(success(), emit.start());
    emit.get_state();
    BOOST_TEST_MESSAGE("--- started");
    produce_block();    // `emit` being processed in next tx (deferred), go next block to be sure state updated
    auto obj_params = emit.get_params();
    auto t = emit.get_state();
    auto tx = t["tx_id"];

    BOOST_TEST_MESSAGE("--- go to block just before emission");
    auto emit_interval = obj_params["interval"]["value"].as<uint16_t>() * 1000 / cfg::block_interval_ms;
    produce_blocks(emit_interval - 1);      // actually we go to emission block now; TODO: resolve, why deferred tx sometimes applied before get_state() and sometimes we need to go next block
    BOOST_CHECK_EQUAL(tx, emit.get_state()["tx_id"]);
    BOOST_TEST_MESSAGE("--- next block, check emission");
    produce_block();
    auto tx2 = emit.get_state()["tx_id"];
    BOOST_CHECK_NE(tx, tx2);
    produce_block();
    BOOST_CHECK_EQUAL(tx2, emit.get_state()["tx_id"]);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_params, golos_emit_tester) try {
    BOOST_TEST_MESSAGE("Test emission parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    produce_block();

    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(emit.get_params().is_null());

    string pool0 = "{'name':'zero','percent':0}";
    string pool1 = "{'name':'test','percent':5000}";
    string pool2 = "{'name':'less','percent':4999}";
    string pool3 = "{'name':'more','percent':5001}";
    const auto pools = "{'pools':[" + pool2 + "," + pool1 + "," + pool0 + "]}";
    const string infrate = "{'start':1,'stop':1,'narrowing':0}";
    auto interval = "['emit_interval',{'value':'" + std::to_string(_interval) + "'}]";
    auto interval0 = "['emit_interval',{'value':'" + std::to_string(0) + "'}]";
    auto testacc = "{'value':'testacc'}";
    auto params = "[['inflation_rate'," + infrate + "], ['reward_pools'," + pools + "]," + emit.token_symbol_json(_token) + "," + interval0 + ",['vesting_acc'," + testacc + "]" + "]";
    BOOST_CHECK_EQUAL(err.no_pool_account, emit.set_params(params));
    create_accounts({N(test), N(less), N(zero)});
    produce_block();

    BOOST_CHECK_EQUAL(err.interval0, emit.set_params(params));
    params = "[['inflation_rate'," + infrate + "], ['reward_pools'," + pools + "]," + emit.token_symbol_json(_token) + "," + interval + ",['vesting_acc'," + testacc + "]" + "]";
    BOOST_CHECK_EQUAL(err.no_vesting_acc, emit.set_params(params));
    produce_block();
    
    auto vesting_acc = "{'value':'" + cfg::vesting_name.to_string() + "'}";
    params = "[['inflation_rate'," + infrate + "], ['reward_pools'," + pools + "]," + emit.token_symbol_json(_token) + "," + interval + ",['vesting_acc'," + vesting_acc + "]" + "]";
    BOOST_CHECK_EQUAL(success(), emit.set_params(params));
    auto t = emit.get_params();
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(t));
    BOOST_CHECK_EQUAL(success(), emit.set_params(
        "[['inflation_rate'," + infrate + "], ['reward_pools',{'pools':[" +pool0+ "]}]]"
    ));
    t = emit.get_params();
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(t));

    BOOST_CHECK_EQUAL(success(), emit.set_params(
        "[['inflation_rate',{'start':5,'stop':5,'narrowing':0}], ['reward_pools'," + pools + "]]"
    ));
    t = emit.get_params();
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(t));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
