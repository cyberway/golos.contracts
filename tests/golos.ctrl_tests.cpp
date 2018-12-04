#include "golos_tester.hpp"
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

static const symbol _token = symbol(3,"GLS");
static const int64_t _wmult = _token.precision();   // NOTE: actually, it must be vesting precision, which can be different

class golos_ctrl_tester : public golos_tester {
protected:
    golos_ctrl_api ctrl;
    golos_vesting_api vest;
    eosio_token_api token;

public:
    golos_ctrl_tester()
        : golos_tester(cfg::control_name)
        , ctrl({this, _code})
        , vest({this, cfg::vesting_name, _token})
        , token({this, cfg::token_name, _token})
    {
        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, _carol,
            cfg::vesting_name, cfg::token_name, cfg::workers_name, cfg::emission_name});
        produce_block();

        install_contract(_code, contracts::ctrl_wasm(), contracts::ctrl_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());

        _test_props = ctrl.default_params(BLOG, _token, _max_witnesses, _max_witness_votes);
    }


    asset dasset(double val = 0) const {
        return token.make_asset(val);
    }

    // constants
    const uint16_t _max_witnesses = 2;
    const uint16_t _max_witness_votes = 4;
    const uint16_t _smajor_witn_count = _max_witnesses * 2 / 3 + 1;
    const uint16_t _major_witn_count = _max_witnesses * 1 / 2 + 1;
    const uint16_t _minor_witn_count = _max_witnesses * 1 / 3 + 1;
    const name _minority_name = N(witn.minor);
    const name _majority_name = N(witn.major);

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

    mvo _test_props;
    const string _test_key = string(fc::crypto::config::public_key_legacy_prefix)
        + "6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";

    struct errors: contract_error_messages {
        const string no_symbol          = amsg("not initialized");
        const string already_created    = amsg("already created");
        const string max_witness0       = amsg("max_witnesses cannot be 0");
        const string max_wit_votes0     = amsg("max_witness_votes cannot be 0");
        const string same_params        = amsg("same properties are already set");

        const string bad_url            = amsg("url too long");
        const string bad_pub_key        = amsg("public key should not be the default value");
        const string same_reg_props     = amsg("already updated in the same way");
        const string already_unreg      = amsg("witness already unregistered");
        const string no_witness         = amsg("witness not found");

        const string no_more_votes      = amsg("all allowed votes already casted");
        const string already_voted      = amsg("already voted");
        const string no_votes           = amsg("there are no votes");
        const string no_vote            = amsg("there is no vote for this witness");

        const string already_attached   = amsg("already attached");
        const string already_detached   = amsg("user already detached");
        const string no_account         = amsg("user not found");
    } err;

    // prepare
    enum prep_step {
        step_0,
        step_only_create,
        step_reg_witnesses,
        step_vote_witnesses
    };

    void prepare(prep_step state) {
        if (state == step_0) return;
        BOOST_CHECK_EQUAL(success(), ctrl.create(_test_props));
        produce_block();
        ctrl.prepare_multisig(BLOG);
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
        BOOST_CHECK_EQUAL(success(), token.create(_bob, dasset(100500)));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_bob));
        BOOST_CHECK_EQUAL(success(), vest.open(cfg::vesting_name, _token, cfg::vesting_name));
        vector<std::pair<uint64_t,double>> amounts = {
            {BLOG, 1000}, {_alice, 800}, {_bob, 700}, {_carol, 600},
            {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
        };
        for (const auto& p : amounts) {
            BOOST_CHECK_EQUAL(success(), vest.open(p.first, _token, p.first));
            BOOST_CHECK_EQUAL(success(), token.issue(_bob, p.first, dasset(p.second), "issue"));
            BOOST_CHECK_EQUAL(success(), token.transfer(p.first, cfg::vesting_name, dasset(p.second), "buy vesting"));
        };

        BOOST_CHECK_EQUAL(dasset(123), asset::from_string("123.000 GLS"));
        CHECK_MATCHING_OBJECT(vest.get_balance(BLOG), vest.make_balance(1000));
        produce_block();
        CHECK_MATCHING_OBJECT(vest.get_balance(_alice), vest.make_balance(800));
    }
};


BOOST_AUTO_TEST_SUITE(golos_ctrl_tests)

BOOST_FIXTURE_TEST_CASE(create_community, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Test community creation");
    BOOST_TEST_MESSAGE("--- check that actions disabled before community created");
    BOOST_TEST_CHECK(ctrl.get_props().is_null());
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.reg_witness(_w[0], _test_key, "localhost"));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unreg_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.vote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unvote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.set_props(_test_props));
    // attach/detach require permissions which do not exist at this moment
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.attach_acc(_carol));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.detach_acc(_carol));
    // add fake one to test
    auto minority = authority(1, {}, {
        {.permission = {_alice, config::active_name}, .weight = 1}
    });
    set_authority(_bob, _minority_name, minority, "active");
    link_authority(_bob, _code, _minority_name, N(attachacc));
    link_authority(_bob, _code, _minority_name, N(detachacc));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.attach_acc_ex(_bob, {_alice}, _carol));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.detach_acc_ex(_bob, {_alice}, _carol));

    BOOST_TEST_MESSAGE("--- test fail when create community with bad parameters");
    auto w0 = mvo(_test_props)("max_witnesses",0);
    auto v0 = mvo(_test_props)("max_witness_votes",0);
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.create(w0));
    BOOST_CHECK_EQUAL(err.max_wit_votes0, ctrl.create(v0));

    BOOST_TEST_MESSAGE("--- create community with valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.create(_test_props));
    BOOST_TEST_MESSAGE("--- created");
    const auto t = ctrl.get_props();
    CHECK_EQUAL_OBJECTS(t["props"]["token"], _test_props["token"]);
    auto props = mvo(_test_props);
    props.erase("token");
    CHECK_MATCHING_OBJECT(t["props"], props);
    produce_block();

    BOOST_TEST_MESSAGE("--- test fail when trying to create again");
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(_test_props));
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(_test_props));
    auto w30 = mvo(_test_props)("max_witnesses",100);
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(w30));
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(register_witness, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness registration");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_only_create);

    BOOST_TEST_MESSAGE("--- check registration success");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], _test_key, "localhost"));
    auto w = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true)("total_weight",0);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), w);

    BOOST_TEST_MESSAGE("--- check properties update");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], _test_key, "-"));
    BOOST_CHECK_EQUAL(err.same_reg_props, ctrl.reg_witness(_w[0], _test_key, "-"));
    // BOOST_CHECK_EQUAL(err.bad_pub_key, ctrl.reg_witness(_w[0], "", "-"));        // not needed, key will be removed
    string maxurl =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], _test_key, maxurl));
    BOOST_CHECK_EQUAL(err.bad_url, ctrl.reg_witness(_w[0], _test_key, maxurl+"!"));

    BOOST_TEST_MESSAGE("--- check unreg");
    BOOST_CHECK_EQUAL(success(), ctrl.unreg_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.already_unreg, ctrl.unreg_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.no_witness, ctrl.unreg_witness(_w[1]));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(vote_witness, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness vote");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_reg_witnesses);
    prepare_balances();

    BOOST_TEST_MESSAGE("--- Cast some votes");
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[1]));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[2]));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[3]));
    BOOST_CHECK_EQUAL(err.no_more_votes, ctrl.vote_witness(_alice, _w[4]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_bob, _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_w[0], _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(err.already_voted, ctrl.vote_witness(_alice, _w[0]));
    produce_block();
    BOOST_TEST_MESSAGE("P: " + fc::json::to_string(get_account_permissions(BLOG)));
    // TODO: check permissions

    BOOST_TEST_MESSAGE("--- Check witness weight");
    auto wp = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+700+100)*_wmult));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check witness weight after unvote");
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_bob, _w[0]));
    BOOST_CHECK_EQUAL(err.no_vote, ctrl.unvote_witness(_bob, _w[0]));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+100)*_wmult));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check unvote and vote again");
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_alice, _w[3]));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[3]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_alice, _w[3]));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[4]));
    produce_block();
    BOOST_CHECK_EQUAL(err.no_votes, ctrl.unvote_witness(_carol, _bob));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(attach_detach_account, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Attach/detach accounts");
    return; // disabled for now. TODO: fix after start using attach/detach
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that account not attached");
    auto attached = ctrl.get_attached(_alice);
    BOOST_CHECK_EQUAL(true, attached.is_null());

    BOOST_TEST_MESSAGE("--- check success on attach action");
    auto w = witness_vect(_minor_witn_count);
    BOOST_CHECK_EQUAL(success(), ctrl.attach_acc(_code, w, _alice));
    produce_block();
    auto expect = mvo()("name", "alice")("attached", true);
    CHECK_MATCHING_OBJECT(ctrl.get_attached(_alice), expect);
    BOOST_CHECK_EQUAL(err.already_attached, ctrl.attach_acc(_code, w, _alice));

    BOOST_TEST_MESSAGE("--- check detaching");
    BOOST_CHECK_EQUAL(success(), ctrl.detach_acc(_code, w, _alice));
    CHECK_MATCHING_OBJECT(ctrl.get_attached(_alice), expect("attached", false));
    BOOST_CHECK_EQUAL(err.already_detached, ctrl.detach_acc(_code, w, _alice));
    BOOST_CHECK_EQUAL(err.no_account, ctrl.detach_acc(_code, w, _bob));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_params, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Update parameters");
    return; // disabled for now. TODO: fix after move to new parameters mechanism
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that setting same parameters fail");
    auto w = witness_vect(_smajor_witn_count);
    BOOST_CHECK_EQUAL(err.same_params, ctrl.set_props(_test_props));

    BOOST_TEST_MESSAGE("--- check that setting invalid parameters fail");
    // TODO: maybe move to separate parameters validation test
    auto p = mvo(_test_props);
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.set_props(mvo(p)("max_witnesses",0)));
    BOOST_CHECK_EQUAL(err.max_wit_votes0, ctrl.set_props(mvo(p)("max_witness_votes",0)));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(mvo(p)("witness_majority",2)));

    // TODO: maybe separate test for signatures
    BOOST_TEST_MESSAGE("--- check that too many or too less signatures fail");
    p = mvo(_test_props)("witness_supermajority",1);
    BOOST_CHECK_NE(success(), ctrl.set_props(p));
    BOOST_CHECK_NE(success(), ctrl.set_props(p));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(p));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_bob, _w[4]));    // it requires re-vote now

    BOOST_TEST_MESSAGE("--- check that changed witness_supermajority = 1 in effect");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(mvo(p)("witness_supermajority",0)));
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_bob, _w[4]));

    BOOST_TEST_MESSAGE("--- check that restored witness_supermajority in effect");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(mvo(p)("witness_supermajority",2)));

    BOOST_TEST_MESSAGE("--- check that unregistered witness removed from miltisig");
    BOOST_CHECK_EQUAL(success(), ctrl.unreg_witness(_w[0]));
    auto top = witness_vect(3); // returns w3,w2,w1
    BOOST_CHECK_NE(success(), ctrl.set_props(mvo(p)("witness_supermajority",0)));
    top.erase(top.end() - 1);   // remove w1
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(mvo(p)("witness_supermajority",0)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(change_vesting, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Change-vesting notification");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- fail on direct action call");
    BOOST_CHECK_NE(success(), ctrl.change_vests(BLOG, vest.make_asset(5)));
    BOOST_CHECK_NE(success(), ctrl.change_vests(_bob, vest.make_asset(-5)));
    produce_block();

    BOOST_TEST_MESSAGE("--- witness weight change when adding vesting");
    auto wp = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",_wmult*(800+700+100)));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",_wmult*(800))("name","witn2"));
    BOOST_CHECK_EQUAL(success(), token.issue(_bob, _alice, dasset(100), "issue"));
    BOOST_CHECK_EQUAL(success(), token.transfer(_alice, cfg::vesting_name, dasset(100), "buy vesting"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",_wmult*(800+700+100+100))("name","witn1"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",_wmult*(800+100))("name","witn2"));
    produce_block();

    // TODO: check decreasing vesting and paths, other than `issue`+`transfer`
} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
