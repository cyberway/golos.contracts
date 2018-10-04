#include "golos_tester.hpp"
#include "contracts.hpp"
#include <fc/variant_object.hpp>


using namespace eosio;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

using mvo = fc::mutable_variant_object;
using symbol_type = symbol;


class golos_ctrl_tester : public golos_tester {
public:
    golos_ctrl_tester(): golos_tester(N(golos.ctrl), symbol(6,"TST").value()) {
        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), _alice, _bob, _carol,
            N(golos.vest), N(eosio.token)});
        produce_block();

        install_contract(_code, contracts::ctrl_wasm(), contracts::ctrl_abi(), abi_ser);
        install_contract(N(eosio.token), contracts::token_wasm(), contracts::token_abi(), abi_ser_token);
        install_contract(N(golos.vest), contracts::vesting_wasm(), contracts::vesting_abi(), abi_ser_vesting);

    }

    abi_serializer* acc2abi(account_name a) {
        abi_serializer* r = &abi_ser;
        switch (uint64_t(a)) {
            case N(golos.vest):  r = &abi_ser_vesting; break;
            case N(eosio.token): r = &abi_ser_token; break;
        }
        return r;
    }

    action_result push_action(account_name code, account_name signer, action_name name, const variant_object& data) {
        const auto abi = acc2abi(code);
        return golos_tester::push_action(abi, code, name, data, signer);
    }
    action_result push_action(account_name signer, action_name name, const variant_object& data) {
        return push_action(_code, signer, name, data);
    }

    // control actions
    action_result create(symbol_type token, account_name owner, mvo props) {
        return push_action(owner, N(create), mvo()("domain", token)
            ("owner", owner)
            ("props", props)
        );
    }

    action_result set_props(vector<account_name> signers, symbol_type token, mvo props) {
        return push_action_msig_tx(&abi_ser, _code, N(updateprops),
            mvo()("domain", token)("props", props),
            {{BLOG, config::active_name}},
            signers
        );
    }

    action_result attach_acc_ex(account_name owner, vector<account_name> signers, symbol_type token, account_name user) {
        return push_action_msig_tx(&abi_ser, _code, N(attachacc),
            mvo()("domain", token)("user", user),
            {{owner, _minority_name}},
            signers
        );
    }
    action_result detach_acc_ex(account_name owner, vector<account_name> signers, symbol_type token, account_name user) {
        return push_action_msig_tx(&abi_ser, _code, N(detachacc),
            mvo()("domain", token)("user", user),
            {{owner, _minority_name}},
            signers
        );
    }
    action_result attach_acc(vector<account_name> signers, symbol_type token, account_name user) {
        return attach_acc_ex(BLOG, signers, token, user);
    }
    action_result detach_acc(vector<account_name> signers, symbol_type token, account_name user) {
        return detach_acc_ex(BLOG, signers, token, user);
    }

    action_result reg_witness(symbol_type token, account_name witness, string key, string url) {
        return push_action(witness, N(regwitness), mvo()("domain", token)
            ("witness", witness)
            ("key", key == "" ? fc::crypto::public_key() : fc::crypto::public_key(key))
            ("url", url)
        );
    }
    action_result unreg_witness(symbol_type token, account_name witness) {
        return push_action(witness, N(unregwitness), mvo()("domain", token)
            ("witness", witness)
        );
    }

    action_result vote_witness(symbol_type token, account_name voter, account_name witness, uint16_t weight=10000) {
        return push_action(voter, N(votewitness), mvo()("domain", token)
            ("voter", voter)
            ("witness", witness)
            ("weight", weight)
        );
    }
    action_result unvote_witness(symbol_type token, account_name voter, account_name witness) {
        return push_action(voter, N(unvotewitn), mvo()("domain", token)
            ("voter", voter)
            ("witness", witness)
        );
    }

    // void update_top(symbol_type token) {
    //     push_action(_code, N(updatetop), _sys, mvo()("domain", token));
    // }

    // vesting actions
    action_result create_pair(asset token, asset vesting) {
        return push_action(N(golos.vest), N(golos.vest), N(createpair), mvo()("token", token)("vesting", vesting));
    }
    action_result open_vesting(account_name owner, symbol_type symbol, account_name payer) {
        return push_action(N(golos.vest), payer, N(open), mvo()
            ("owner", owner)
            ("symbol", symbol)
            ("ram_payer", payer)
        );
    }
    action_result accrue_vesting(account_name sender, account_name user, asset quantity) {
        return push_action(N(golos.vest), sender, N(accruevg), mvo()
            ("sender", sender)
            ("user", user)
            ("quantity", quantity)
        );
    }


    // tables helpers
    fc::variant get_struct(name tbl, uint64_t id, const string& name) const {
        return golos_tester::get_struct(tbl, id, name, abi_ser);
    }

    fc::variant get_attached(account_name user) const {
        return get_struct(N(bwuser), user, "bw_user");
    }

    fc::variant get_props() const {
        return get_struct(N(props), 0, "ctrl_props");
    }

    fc::variant get_witness(account_name witness) const {
        return get_struct(N(witness), witness, "witness_info");
    }

    fc::variant get_account_vesting(account_name acc, symbol_type sym) const {
        return get_tbl_struct(N(golos.vest), acc, N(balances), sym.to_symbol_code(), "user_balance", abi_ser_vesting);
    }


    void check_vesting_balance(const fc::variant& obj, asset vesting,
        asset dlg_vest = asset::from_string("0.000000 TST"),
        asset rcv_vest = asset::from_string("0.000000 TST")
    ) {
        BOOST_CHECK_EQUAL(true, obj.is_object());
        BOOST_CHECK_EQUAL(obj["vesting"].as<asset>(), vesting);
        BOOST_CHECK_EQUAL(obj["delegate_vesting"].as<asset>(), dlg_vest);
        BOOST_CHECK_EQUAL(obj["received_vesting"].as<asset>(), rcv_vest);
    }


    asset dasset(double val = 0) const {
        return asset(val*1e6, _token); // 6 is precision. todo: derive from symbol and less hardcode
    }
    const symbol_type _token = symbol(_scope);

    abi_serializer abi_ser;
    abi_serializer abi_ser_vesting;
    abi_serializer abi_ser_token;

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

    const mvo _test_props = mvo()
        ("max_witnesses",_max_witnesses)
        ("max_witness_votes",_max_witness_votes)
        ("witness_supermajority",0)
        ("witness_majority",0)
        ("witness_minority",0);
    const string _test_key = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";

    const string err_no_symbol      = wasm_assert_msg("symbol not found");
    const string err_already_created= wasm_assert_msg("this token already created");
    const string err_max_witness0   = wasm_assert_msg("max_witnesses cannot be 0");
    const string err_max_wit_votes0 = wasm_assert_msg("max_witness_votes cannot be 0");
    const string err_same_params    = wasm_assert_msg("same properties are already set");

    const string err_bad_url        = wasm_assert_msg("url too long");
    const string err_bad_pub_key    = wasm_assert_msg("public key should not be the default value");
    const string err_same_reg_props = wasm_assert_msg("already updated in the same way");
    const string err_already_unreg  = wasm_assert_msg("witness already unregistered");
    const string err_no_witness     = wasm_assert_msg("witness not found");

    const string err_no_more_votes  = wasm_assert_msg("all allowed votes already casted");
    const string err_already_voted  = wasm_assert_msg("already voted");
    const string err_no_votes       = wasm_assert_msg("there are no votes");
    const string err_no_vote        = wasm_assert_msg("there is no vote for this witness");

    const string err_already_attached   = wasm_assert_msg("already attached");
    const string err_already_detached   = wasm_assert_msg("user already detached");
    const string err_no_account         = wasm_assert_msg("user not found");

    // prepare
    enum prep_step {
        step_0,
        step_only_create,
        step_reg_witnesses,
        step_vote_witnesses
    };

    void prepare(prep_step state) {
        if (state == step_0) return;
        BOOST_CHECK_EQUAL(success(), create(_token, BLOG, _test_props));
        produce_block();
        prepare_account();
        produce_block();

        if (state <= step_only_create) return;
        for (int i = 0; i < _n_w; i++) {
            BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[i], _test_key, "localhost"));
        }
        produce_block();

        if (state <= step_reg_witnesses) return;
        prepare_balances();
        //vote
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[0]));
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[1]));
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[2]));
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[3]));
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _bob, _w[0]));
        BOOST_CHECK_EQUAL(success(), vote_witness(_token, _w[0], _w[0]));
    }

    void prepare_account() {
        // witn.major/minor
        auto auth = authority(1, {}, {
            {.permission = {BLOG, config::active_name}, .weight = 1}
        });
        set_authority(BLOG, _majority_name, auth, "active");
        set_authority(BLOG, _minority_name, auth, "active");
        // link witn.minor to allow attachacc/detachacc
        link_authority(BLOG, _code, _minority_name, N(attachacc));
        link_authority(BLOG, _code, _minority_name, N(detachacc));

        //eosio.code
        auto code_auth = authority(1, {}, {
            {.permission = {_code, config::eosio_code_name}, .weight = 1}
        });
        set_authority(BLOG, config::owner_name, code_auth, 0);
        code_auth.keys = {{get_public_key(BLOG, "active"), 1}};
        set_authority(BLOG, config::active_name, code_auth, "owner",
            {permission_level{BLOG, config::active_name}}, {get_private_key("blog", "active")});
    }

    void prepare_balances() {
        BOOST_CHECK_EQUAL(success(), create_pair(asset::from_string("0.0000 GLS"), dasset(0)));
        vector<std::pair<uint64_t,double>> amounts = {
            {BLOG, 1000}, {_alice, 800}, {_bob, 700}, {_carol, 600},
            {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
        };
        for (const auto& p : amounts) {
            BOOST_CHECK_EQUAL(success(), open_vesting(p.first, _token, p.first));
            BOOST_CHECK_EQUAL(success(), accrue_vesting(N(golos.vest), p.first, dasset(p.second)));
        };

        BOOST_CHECK_EQUAL(dasset(123), asset::from_string("123.000000 TST"));
        check_vesting_balance(get_account_vesting(_alice, _token), dasset(800));
        produce_block();
        check_vesting_balance(get_account_vesting(_carol, _token), dasset(600));
    }
};


BOOST_AUTO_TEST_SUITE(golos_ctrl_tests)

BOOST_FIXTURE_TEST_CASE(create_community_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Test community creation");
    BOOST_TEST_MESSAGE("--- check that actions disabled before community created");
    BOOST_CHECK_EQUAL(true, get_props().is_null());
    BOOST_CHECK_EQUAL(err_no_symbol, reg_witness(_token, _w[0], _test_key, "localhost"));
    BOOST_CHECK_EQUAL(err_no_symbol, unreg_witness(_token, _w[0]));
    BOOST_CHECK_EQUAL(err_no_symbol, vote_witness(_token, _alice, _w[0]));
    BOOST_CHECK_EQUAL(err_no_symbol, unvote_witness(_token, _alice, _w[0]));
    BOOST_CHECK_EQUAL(err_no_symbol, set_props({BLOG}, _token, _test_props));
    // attach/detach require permissions which do not exist at this moment
    BOOST_CHECK_NE(success(), attach_acc({BLOG}, _token, _carol));
    BOOST_CHECK_NE(success(), detach_acc({BLOG}, _token, _carol));
    // add fake one to test
    auto minority = authority(1, {}, {
        {.permission = {_alice, config::active_name}, .weight = 1}
    });
    set_authority(_bob, _minority_name, minority, "active");
    link_authority(_bob, _code, _minority_name, N(attachacc));
    link_authority(_bob, _code, _minority_name, N(detachacc));
    BOOST_CHECK_EQUAL(err_no_symbol, attach_acc_ex(_bob, {_alice}, _token, _carol));
    BOOST_CHECK_EQUAL(err_no_symbol, detach_acc_ex(_bob, {_alice}, _token, _carol));

    BOOST_TEST_MESSAGE("--- test fail when create community with bad parameters");
    auto w0 = mvo(_test_props)("max_witnesses",0);
    auto v0 = mvo(_test_props)("max_witness_votes",0);
    BOOST_CHECK_EQUAL(err_max_witness0, create(_token, BLOG, w0));
    BOOST_CHECK_EQUAL(err_max_wit_votes0, create(_token, BLOG, v0));

    BOOST_TEST_MESSAGE("--- create community with valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), create(_token, BLOG, _test_props));
    const auto t = get_props();
    BOOST_CHECK_EQUAL(t["owner"], fc::variant(BLOG));
    CHECK_MATCHING_OBJECT(_test_props, t["props"]);
    produce_block();

    BOOST_TEST_MESSAGE("--- test fail when trying to create again");
    BOOST_CHECK_EQUAL(err_already_created, create(_token, BLOG, _test_props));
    BOOST_CHECK_EQUAL(err_already_created, create(_token, _alice, _test_props));
    auto w30 = mvo(_test_props)("max_witnesses",100);
    BOOST_CHECK_EQUAL(err_already_created, create(_token, BLOG, w30));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(register_witness_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness registration");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_only_create);

    BOOST_TEST_MESSAGE("--- check registration success");
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[0], _test_key, "localhost"));
    auto w = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true)("total_weight",0);
    CHECK_EQUAL_OBJECTS(get_witness(0), w);

    BOOST_TEST_MESSAGE("--- check properties update");
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[0], _test_key, "-"));
    BOOST_CHECK_EQUAL(err_same_reg_props, reg_witness(_token, _w[0], _test_key, "-"));
    BOOST_CHECK_EQUAL(err_bad_pub_key, reg_witness(_token, _w[0], "", "-"));
    string maxurl =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[0], _test_key, maxurl));
    BOOST_CHECK_EQUAL(err_bad_url, reg_witness(_token, _w[0], _test_key, maxurl+"!"));

    BOOST_TEST_MESSAGE("--- check unreg");
    BOOST_CHECK_EQUAL(success(), unreg_witness(_token, _w[0]));
    BOOST_CHECK_EQUAL(err_already_unreg, unreg_witness(_token, _w[0]));
    BOOST_CHECK_EQUAL(err_no_witness, unreg_witness(_token, _w[1]));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(vote_witness_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness vote");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_reg_witnesses);
    prepare_balances();

    BOOST_TEST_MESSAGE("--- Cast some votes");
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[0]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[1]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[2]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[3]));
    BOOST_CHECK_EQUAL(err_no_more_votes, vote_witness(_token, _alice, _w[4]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _bob, _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _w[0], _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(err_already_voted, vote_witness(_token, _alice, _w[0]));
    produce_block();
    BOOST_TEST_MESSAGE("P: " + fc::json::to_string(get_account_permissions(BLOG)));
    // TODO: check permissions

    BOOST_TEST_MESSAGE("--- Check witness weight");
    auto wp = mvo()("name","witn1")("key",_test_key)("url","localhost")("active",true);
    CHECK_EQUAL_OBJECTS(get_witness(_w[0]), wp("total_weight",(800+700+100)*1e6));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check witness weight after unvote");
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, _bob, _w[0]));
    BOOST_CHECK_EQUAL(err_no_vote, unvote_witness(_token, _bob, _w[0]));
    CHECK_EQUAL_OBJECTS(get_witness(_w[0]), wp("total_weight",(800+100)*1e6));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check unvote and vote again");
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, _alice, _w[3]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[3]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, _alice, _w[3]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _alice, _w[4]));
    produce_block();
    BOOST_CHECK_EQUAL(err_no_votes, unvote_witness(_token, _carol, _bob));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(attach_detach_account_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Attach/detach accounts");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that account not attached");
    auto attached = get_attached(_alice);
    BOOST_CHECK_EQUAL(true, attached.is_null());

    BOOST_TEST_MESSAGE("--- check success on attach action");
    auto w = witness_vect(_minor_witn_count);
    BOOST_CHECK_EQUAL(success(), attach_acc(w, _token, _alice));
    produce_block();
    attached = get_attached(_alice);
    CHECK_EQUAL_OBJECTS(attached, mvo()
        ("name", "alice")
        ("attached", true)
    );
    BOOST_CHECK_EQUAL(err_already_attached, attach_acc(w, _token, _alice));

    BOOST_TEST_MESSAGE("--- check detaching");
    BOOST_CHECK_EQUAL(success(), detach_acc(w, _token, _alice));
    attached = get_attached(_alice);
    CHECK_EQUAL_OBJECTS(attached, mvo()
        ("name", "alice")
        ("attached", false)
    );
    BOOST_CHECK_EQUAL(err_already_detached, detach_acc(w, _token, _alice));
    BOOST_CHECK_EQUAL(err_no_account, detach_acc(w, _token, _bob));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_params_test, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Update parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_vote_witnesses);

    BOOST_TEST_MESSAGE("--- check that setting same parameters fail");
    auto w = witness_vect(_smajor_witn_count);
    BOOST_CHECK_EQUAL(err_same_params, set_props(w, _token, _test_props));

    BOOST_TEST_MESSAGE("--- check that setting invalid parameters fail");
    // TODO: maybe move to separate parameters validation test
    auto p = mvo(_test_props);
    BOOST_CHECK_EQUAL(err_max_witness0, set_props(w, _token, mvo(p)("max_witnesses",0)));
    BOOST_CHECK_EQUAL(err_max_wit_votes0, set_props(w, _token, mvo(p)("max_witness_votes",0)));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), set_props(vector<account_name>{_w[0], _w[1]}, _token, mvo(p)("witness_majority",2)));

    // TODO: maybe separate test for signatures
    BOOST_TEST_MESSAGE("--- check that too many or too less signatures fail");
    p = mvo(_test_props)("witness_supermajority",1);
    BOOST_CHECK_NE(success(), set_props(witness_vect(_smajor_witn_count+1), _token, p));
    BOOST_CHECK_NE(success(), set_props(witness_vect(_smajor_witn_count-1), _token, p));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), set_props(w, _token, p));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _bob, _w[4]));    // it requires re-vote now

    BOOST_TEST_MESSAGE("--- check that changed witness_supermajority = 1 in effect");
    BOOST_CHECK_EQUAL(success(), set_props(witness_vect(1), _token, mvo(p)("witness_supermajority",0)));
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, _bob, _w[4]));

    BOOST_TEST_MESSAGE("--- check that restored witness_supermajority in effect");
    BOOST_CHECK_EQUAL(success(), set_props(w, _token, mvo(p)("witness_supermajority",2)));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(vesting_notify_test, golos_ctrl_tester) try {

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
