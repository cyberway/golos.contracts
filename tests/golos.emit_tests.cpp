#include "golos_tester.hpp"
#include "golos.emit_test_api.hpp"
#include "golos.ctrl_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include "contracts.hpp"
#include "../golos.emit/include/golos.emit/config.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

using symbol_type = symbol;

static const auto _token = symbol_type(6,"TST");

class golos_emit_tester : public golos_tester {
protected:
    golos_emit_api emit;
    golos_ctrl_api ctrl;
    golos_vesting_api vest;
    eosio_token_api token;

public:
    golos_emit_tester()
        : golos_tester(cfg::emission_name)
        , emit({this, _code, _token})
        , ctrl({this, cfg::control_name, _token})
        , vest({this, cfg::vesting_name, _token})
        , token({this, cfg::token_name, _token})
    {
        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, _carol,
            cfg::control_name, cfg::vesting_name, cfg::token_name, cfg::workers_name});
        produce_block();

        install_contract(_code, contracts::emit_wasm(), contracts::emit_abi());
        install_contract(cfg::control_name, contracts::ctrl_wasm(), contracts::ctrl_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
    }


    asset dasset(double val = 0) const {
        return token.make_asset(val);
    }

    // constants
    const uint16_t _max_witnesses = 2;
    const uint16_t _smajor_witn_count = _max_witnesses * 2 / 3 + 1;

    const account_name BLOG = N(blog);
    const account_name _alice = N(alice);
    const account_name _bob = N(bob);
    const account_name _carol = N(carol);
    const uint64_t _w[5] = {N(witn1), N(witn2), N(witn3), N(witn4), N(witn5)};
    const size_t _n_w = sizeof(_w) / sizeof(_w[0]);

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
        // const string no_symbol          = amsg("symbol not found");
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

    void prepare_ctrl(prep_step state) {
        if (state == step_0) return;
        BOOST_CHECK_EQUAL(success(),
            ctrl.create(BLOG, ctrl.default_params(_max_witnesses, 4, cfg::workers_name)));
        produce_block();
        ctrl.prepare_owner(BLOG);
        produce_block();

        if (state <= step_only_create) return;
        for (int i = 0; i < _n_w; i++) {
            BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[i], _test_key, "localhost"));
        }
        produce_block();

        if (state <= step_reg_witnesses) return;
        prepare_balances();
        vector<std::pair<name,name>> votes = {
            {_alice, _w[0]}, {_alice, _w[1]}, {_alice, _w[2]}, {_alice, _w[3]},
            {_bob, _w[0]}, {_w[0], _w[0]}
        };
        for (const auto& v : votes) {
            BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(v.first, v.second));
        }
    }

    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), token.create(BLOG, dasset(100500)));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(BLOG, _token, {_code}));
        BOOST_CHECK_EQUAL(success(), vest.open(cfg::vesting_name, _token, cfg::vesting_name));
        vector<std::pair<uint64_t,double>> amounts = {
            {BLOG, 1000}, {cfg::workers_name, 0}, {_alice, 800}, {_bob, 700}, {_carol, 600},
            {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
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
        CHECK_MATCHING_OBJECT(vest.get_balance(BLOG), vest.make_balance(1000));
        CHECK_MATCHING_OBJECT(vest.get_balance(_alice), vest.make_balance(800));
        CHECK_MATCHING_OBJECT(vest.get_balance(cfg::workers_name), vest.make_balance(0));
    }
};


BOOST_AUTO_TEST_SUITE(golos_emit_tests)

BOOST_FIXTURE_TEST_CASE(start_emission_test, golos_emit_tester) try {
    BOOST_TEST_MESSAGE("Test emission start");

    BOOST_TEST_MESSAGE("--- prepare control");
    prepare_ctrl(step_vote_witnesses);
    produce_blocks(10);

    BOOST_TEST_MESSAGE("--- start succeed");
    auto w = witness_vect(_smajor_witn_count);
    BOOST_CHECK_EQUAL(success(), emit.start(BLOG, w));
    BOOST_TEST_MESSAGE("--- started");
    auto t = emit.get_state();
    auto tx = t["tx_id"];

    BOOST_TEST_MESSAGE("--- go to block just before emission");
    auto emit_interval = cfg::emit_interval * 1000 / cfg::block_interval_ms;
    produce_blocks(emit_interval - 1);
    BOOST_CHECK_EQUAL(tx, emit.get_state()["tx_id"]);
    BOOST_TEST_MESSAGE("--- next block, check emission");
    produce_block();
    BOOST_CHECK_NE(tx, emit.get_state()["tx_id"]);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
