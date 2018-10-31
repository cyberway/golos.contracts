#include "golos_tester.hpp"
#include "golos.ctrl_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include "contracts.hpp"


using namespace eosio;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

using symbol_type = symbol;

static const symbol _domain = symbol(6,"TST");

class golos_ctrl_tester : public golos_tester {
protected:
    golos_ctrl_api ctrl;
    golos_vesting_api vest;
    eosio_token_api etoken;

public:
    golos_ctrl_tester()
        : golos_tester(N(golos.ctrl), _domain.value())
        , ctrl({this, _code, _domain})
        , vest({this, N(golos.vest)})
        , etoken({this, N(eosio.token)})
    {
        vest._symbol =
        etoken._symbol = _domain;//.to_symbol_code().value;

        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, _carol,
            _vesting_name, N(eosio.token), N(worker)});
        produce_block();

        install_contract(_code, contracts::ctrl_wasm(), contracts::ctrl_abi());
        install_contract(N(eosio.token), contracts::token_wasm(), contracts::token_abi());
        install_contract(_vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
    }


    asset dasset(double val = 0) const {
        // return asset(val * _token.precision(), _token);
        return vest.make_asset(val);
    }
    const symbol_type _token = symbol(_scope);

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
    const account_name _vesting_name = N(golos.vest);

    vector<account_name> witness_vect(size_t n) const {
        vector<account_name> r;
        auto l = std::min(n, _n_w);
        r.reserve(l);
        while (l--) {
            r.push_back(_w[l]);
        }
        return r;
    }

    const mvo _test_props = mvo()
        ("max_witnesses",_max_witnesses)
        ("max_witness_votes",_max_witness_votes)
        ("witness_supermajority",0)
        ("witness_majority",0)
        ("witness_minority",0)
        ("infrate_start", 1500)
        ("infrate_stop", 95)
        ("infrate_narrowing", 250000*6)
        ("content_reward", 6667-667)
        ("vesting_reward", 2667-267)
        ("workers_reward", 1000)
        ("workers_pool", "worker");

    const string _test_key = string(fc::crypto::config::public_key_legacy_prefix)
        + "6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";

    struct errors: contract_error_messages {
        const string no_symbol          = amsg("symbol not found");
        const string already_created    = amsg("this token already created");
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
        BOOST_CHECK_EQUAL(success(), ctrl.create(BLOG, _test_props));
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
        BOOST_CHECK_EQUAL(success(), etoken.create(_bob, dasset(100500)));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_bob, _token));
        BOOST_CHECK_EQUAL(success(), vest.open(_vesting_name, _token, _vesting_name));
        vector<std::pair<uint64_t,double>> amounts = {
            {BLOG, 1000}, {_alice, 800}, {_bob, 700}, {_carol, 600},
            {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
        };
        for (const auto& p : amounts) {
            BOOST_CHECK_EQUAL(success(), vest.open(p.first, _token, p.first));
            BOOST_CHECK_EQUAL(success(), etoken.issue(_bob, p.first, dasset(p.second), "issue"));
            BOOST_CHECK_EQUAL(success(), etoken.transfer(p.first, _vesting_name, dasset(p.second), "buy vesting"));
        };

        BOOST_CHECK_EQUAL(dasset(123), asset::from_string("123.000000 TST"));
        CHECK_MATCHING_OBJECT(vest.get_balance(BLOG), vest.make_balance(1000));
        produce_block();
        CHECK_MATCHING_OBJECT(vest.get_balance(_alice), vest.make_balance(800));
    }
};


BOOST_AUTO_TEST_SUITE(golos_ctrl_tests)

BOOST_FIXTURE_TEST_CASE(create_community_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Test community creation");
    BOOST_TEST_MESSAGE("--- check that actions disabled before community created");
    BOOST_TEST_CHECK(ctrl.get_props(BLOG).is_null());
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.reg_witness(_w[0], _test_key, "localhost"));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unreg_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.vote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unvote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.set_props(BLOG, {BLOG}, _test_props));
    // attach/detach require permissions which do not exist at this moment
    BOOST_CHECK_NE(success(), ctrl.attach_acc(BLOG, {BLOG}, _carol));
    BOOST_CHECK_NE(success(), ctrl.detach_acc(BLOG, {BLOG}, _carol));
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
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.create(BLOG, w0));
    BOOST_CHECK_EQUAL(err.max_wit_votes0, ctrl.create(BLOG, v0));

    BOOST_TEST_MESSAGE("--- create community with valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.create(BLOG, _test_props));
    const auto t = ctrl.get_props(BLOG);
    BOOST_CHECK_EQUAL(t["owner"], fc::variant(BLOG));
    CHECK_MATCHING_OBJECT(t["props"], _test_props);
    produce_block();

    BOOST_TEST_MESSAGE("--- test fail when trying to create again");
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(BLOG, _test_props));
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(_alice, _test_props));
    auto w30 = mvo(_test_props)("max_witnesses",100);
    BOOST_CHECK_EQUAL(err.already_created, ctrl.create(BLOG, w30));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(register_witness_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness registration");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_only_create);

    BOOST_TEST_MESSAGE("--- check registration success");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], _test_key, "localhost"));
    auto w = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true)("total_weight",0);
    CHECK_EQUAL_OBJECTS(ctrl.get_witness(_w[0]), w);

    BOOST_TEST_MESSAGE("--- check properties update");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], _test_key, "-"));
    BOOST_CHECK_EQUAL(err.same_reg_props, ctrl.reg_witness(_w[0], _test_key, "-"));
    BOOST_CHECK_EQUAL(err.bad_pub_key, ctrl.reg_witness(_w[0], "", "-"));
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

BOOST_FIXTURE_TEST_CASE(vote_witness_test, golos_ctrl_tester) try {
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
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+700+100)*1e6));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check witness weight after unvote");
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_bob, _w[0]));
    BOOST_CHECK_EQUAL(err.no_vote, ctrl.unvote_witness(_bob, _w[0]));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+100)*1e6));
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

BOOST_FIXTURE_TEST_CASE(attach_detach_account_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Attach/detach accounts");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that account not attached");
    auto attached = ctrl.get_attached(_alice);
    BOOST_CHECK_EQUAL(true, attached.is_null());

    BOOST_TEST_MESSAGE("--- check success on attach action");
    auto w = witness_vect(_minor_witn_count);
    BOOST_CHECK_EQUAL(success(), ctrl.attach_acc(BLOG, w, _alice));
    produce_block();
    auto expect = mvo()("name", "alice")("attached", true);
    CHECK_MATCHING_OBJECT(ctrl.get_attached(_alice), expect);
    std::cout << "REF: " << variant(expect) << std::endl;
    auto qq = expect["attached"];
    BOOST_TEST_CHECK(qq.is_object());
    BOOST_TEST_CHECK(qq.is_null());
    BOOST_TEST_CHECK(qq.is_bool());
    BOOST_TEST_CHECK(qq.is_string());
    BOOST_TEST_CHECK(qq.is_array());
    BOOST_TEST_CHECK(qq.is_blob());
    attached = ctrl.get_attached(_alice);
    BOOST_CHECK_EQUAL(qq.get_type(), attached["attached"].get_type());
    BOOST_CHECK_EQUAL(qq, attached["attached"]);
    BOOST_CHECK_EQUAL(qq.as_uint64(), attached["attached"].as_uint64());
    BOOST_CHECK_EQUAL(err.already_attached, ctrl.attach_acc(BLOG, w, _alice));

    BOOST_TEST_MESSAGE("--- check detaching");
    BOOST_CHECK_EQUAL(success(), ctrl.detach_acc(BLOG, w, _alice));
    CHECK_MATCHING_OBJECT(ctrl.get_attached(_alice), expect("attached", false));
    BOOST_CHECK_EQUAL(err.already_detached, ctrl.detach_acc(BLOG, w, _alice));
    BOOST_CHECK_EQUAL(err.no_account, ctrl.detach_acc(BLOG, w, _bob));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_params_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Update parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that setting same parameters fail");
    auto w = witness_vect(_smajor_witn_count);
    BOOST_CHECK_EQUAL(err.same_params, ctrl.set_props(BLOG, w, _test_props));

    BOOST_TEST_MESSAGE("--- check that setting invalid parameters fail");
    // TODO: maybe move to separate parameters validation test
    auto p = mvo(_test_props);
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.set_props(BLOG, w, mvo(p)("max_witnesses",0)));
    BOOST_CHECK_EQUAL(err.max_wit_votes0, ctrl.set_props(BLOG, w, mvo(p)("max_witness_votes",0)));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(BLOG, vector<account_name>{_w[0],_w[1]}, mvo(p)("witness_majority",2)));

    // TODO: maybe separate test for signatures
    BOOST_TEST_MESSAGE("--- check that too many or too less signatures fail");
    p = mvo(_test_props)("witness_supermajority",1);
    BOOST_CHECK_NE(success(), ctrl.set_props(BLOG, witness_vect(_smajor_witn_count+1), p));
    BOOST_CHECK_NE(success(), ctrl.set_props(BLOG, witness_vect(_smajor_witn_count-1), p));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(BLOG, w, p));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_bob, _w[4]));    // it requires re-vote now

    BOOST_TEST_MESSAGE("--- check that changed witness_supermajority = 1 in effect");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(BLOG, witness_vect(1), mvo(p)("witness_supermajority",0)));
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_bob, _w[4]));

    BOOST_TEST_MESSAGE("--- check that restored witness_supermajority in effect");
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(BLOG, w, mvo(p)("witness_supermajority",2)));

    BOOST_TEST_MESSAGE("--- check that unregistered witness removed from miltisig");
    BOOST_CHECK_EQUAL(success(), ctrl.unreg_witness(_w[0]));
    auto top = witness_vect(3); // returns w3,w2,w1
    BOOST_CHECK_NE(success(), ctrl.set_props(BLOG, top, mvo(p)("witness_supermajority",0)));
    top.erase(top.end() - 1);   // remove w1
    BOOST_CHECK_EQUAL(success(), ctrl.set_props(BLOG, top, mvo(p)("witness_supermajority",0)));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(change_vesting_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Change-vesting notification");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- fail on direct action call");
    BOOST_CHECK_NE(success(), ctrl.change_vests(BLOG, dasset(5)));
    BOOST_CHECK_NE(success(), ctrl.change_vests(_bob, dasset(-5)));
    produce_block();

    BOOST_TEST_MESSAGE("--- witness weight change when adding vesting");
    auto wp = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+700+100)*1e6));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",(800)*1e6)("name","witn2"));
    BOOST_CHECK_EQUAL(success(), etoken.issue(_bob, _alice, dasset(100), "issue"));
    BOOST_CHECK_EQUAL(success(), etoken.transfer(_alice, _vesting_name, dasset(100), "buy vesting"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+700+100+100)*1e6)("name","witn1"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",(800+100)*1e6)("name","witn2"));
    produce_block();

    // TODO: check decreasing vesting and paths, other than `issue`+`transfer`

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
