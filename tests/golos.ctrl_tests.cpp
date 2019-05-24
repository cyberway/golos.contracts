#include "golos_tester.hpp"
#include "golos.ctrl_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "contracts.hpp"

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
    cyber_token_api token;

public:
    golos_ctrl_tester()
        : golos_tester(cfg::control_name)
        , ctrl({this, _code})
        , vest({this, cfg::vesting_name, _token})
        , token({this, cfg::token_name, _token})
    {
        create_accounts({_code, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5),
            _alice, _bob, _carol, _issuer,
            cfg::vesting_name, cfg::token_name, cfg::workers_name, cfg::emission_name});
        produce_block();

        install_contract(_code, contracts::ctrl_wasm(), contracts::ctrl_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());

        _test_params = ctrl.default_params(BLOG, _token, _max_witnesses, _max_witness_votes, _update_auth_period);
    }


    asset dasset(double val = 0) const {
        return token.make_asset(val);
    }

    // constants
    const uint32_t _update_auth_period = 3;
    const uint16_t _max_witnesses = 2;
    const uint16_t _max_witness_votes = 4;
    const uint16_t _smajor_witn_count = _max_witnesses * 2 / 3 + 1;
    const uint16_t _major_witn_count = _max_witnesses * 1 / 2 + 1;
    const uint16_t _minor_witn_count = _max_witnesses * 1 / 3 + 1;
    const name _minority_name = N(witn.minor);
    const name _majority_name = N(witn.major);

    const name BLOG = N(blog);
    const name _alice = N(alice);
    const name _bob = N(bob);
    const name _carol = N(carol);
    const name _issuer = N(issuer);
    const uint64_t _w[5] = {N(witn1), N(witn2), N(witn3), N(witn4), N(witn5)};
    const size_t _n_w = sizeof(_w) / sizeof(_w[0]);

    vector<name> witness_vect(size_t n) const {
        vector<name> r;
        auto l = std::min(n, _n_w);
        r.reserve(l);
        while (l--) {
            r.push_back(_w[l]);
        }
        return r;
    }

    string _test_params;

    struct errors: contract_error_messages {
        const string no_symbol          = amsg("not initialized");
        const string immutable          = amsg("can't change immutable parameter");
        const string no_msig_acc        = amsg("multisig account doesn't exists");
        const string max_witness0       = amsg("max witnesses can't be 0");
        const string max_wit_votes0     = amsg("max witness votes can't be 0");

        const string smaj_lt_maj        = amsg("super_majority must not be less than majority");
        const string smaj_lt_min        = amsg("super_majority must not be less than minority");
        const string maj_lt_min         = amsg("majority must not be less than minority");
        const string maj_gt_smaj        = amsg("majority must not be greater than super_majority");
        const string min_gt_smaj        = amsg("minority must not be greater than super_majority");
        const string min_gt_maj         = amsg("minority must not be greater than majority");
        const string smaj_gt_max        = amsg("super_majority must not be greater than max_witnesses");
        const string maj_gt_max         = amsg("majority must not be greater than max_witnesses");
        const string min_gt_max         = amsg("minority must not be greater than max_witnesses");

        const string same_params        = amsg("at least one parameter must change");

        const string bad_url            = amsg("url too long");
        const string same_reg_props     = amsg("already updated in the same way");
        const string not_udpated_flag   = amsg("active flag not updated");
        const string no_witness         = amsg("witness not found");

        const string no_more_votes      = amsg("all allowed votes already casted");
        const string already_voted      = amsg("already voted");
        const string no_votes           = amsg("there are no votes");
        const string no_vote            = amsg("there is no vote for this witness");

        const string already_attached   = amsg("already attached");
        const string already_detached   = amsg("user already detached");
        const string no_account         = amsg("user not found");
        const string auth_period0       = amsg("update auth period can't be 0");
        const string assert_erase_wtnss = amsg("not possible to remove witness as there are votes");
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
        BOOST_CHECK_EQUAL(success(), ctrl.set_params(_test_params));
        produce_block();
        ctrl.prepare_multisig(BLOG);
        produce_block();

        if (state <= step_only_create) return;
        for (int i = 0; i < _n_w; i++) {
            BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[i], "localhost"));
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
        BOOST_CHECK_EQUAL(success(), token.create(_issuer, dasset(100500), {cfg::vesting_name}));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer));
        BOOST_CHECK_EQUAL(success(), vest.open(cfg::vesting_name, _token, cfg::vesting_name));
        vector<std::pair<uint64_t,double>> amounts = {
            {BLOG, 1000}, {_alice, 800}, {_bob, 700}, {_carol, 600},
            {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
        };
        for (const auto& p : amounts) {
            BOOST_CHECK_EQUAL(success(), vest.open(p.first, _token, p.first));
            BOOST_CHECK_EQUAL(success(), token.issue(_issuer, p.first, dasset(p.second), "issue"));
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
    BOOST_TEST_CHECK(ctrl.get_params().is_null());
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.reg_witness(_w[0], "localhost"));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unreg_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.vote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.unvote_witness(_alice, _w[0]));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.attach_acc(_carol));
    BOOST_CHECK_EQUAL(err.no_symbol, ctrl.detach_acc(_carol));

    BOOST_TEST_MESSAGE("--- test fail when create community with bad parameters");
    BOOST_CHECK_EQUAL(err.no_msig_acc, ctrl.set_params(ctrl.default_params(N(nobody), _token)));
    // BOOST_CHECK_EQUAL(err.max_witness0, ctrl.set_params(ctrl.default_params(BLOG, symbol(-1,"bad"))));   // is it handled by serializer?
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.set_params(ctrl.default_params(BLOG, _token, 0)));
    BOOST_CHECK_EQUAL(err.max_wit_votes0, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 0)));

    BOOST_TEST_MESSAGE("------- permission thresholds");
    BOOST_CHECK_EQUAL(err.smaj_lt_maj, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 3,5,0)));
    BOOST_CHECK_EQUAL(err.smaj_lt_min, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 3,0,5)));
    BOOST_CHECK_EQUAL(err.maj_lt_min,  ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 0,2,5)));

    BOOST_CHECK_EQUAL(err.smaj_gt_max, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 22,2,1)));
    BOOST_CHECK_EQUAL(err.maj_gt_max,  ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 0,22,1)));
    BOOST_CHECK_EQUAL(err.min_gt_max,  ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 0,0,22)));
    BOOST_CHECK_EQUAL(err.maj_gt_smaj, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 2,0,0)));
    BOOST_CHECK_EQUAL(err.min_gt_smaj, ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 2,1,0)));
    BOOST_CHECK_EQUAL(err.min_gt_maj,  ctrl.set_params(ctrl.default_params(BLOG, _token, 21, 30, _update_auth_period, 15,1,0)));

    BOOST_TEST_MESSAGE("--- create community with valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_params(_test_params));
    BOOST_TEST_MESSAGE("--- created, check state");
    const auto t = ctrl.get_params();
    CHECK_EQUAL_OBJECTS(t["token"], json_str_to_obj(ctrl.token_param(_token))[1]);
    CHECK_EQUAL_OBJECTS(t["multisig"], json_str_to_obj(ctrl.multisig_param(BLOG))[1]);
    CHECK_EQUAL_OBJECTS(t["witnesses"], json_str_to_obj(ctrl.max_witnesses_param(_max_witnesses))[1]);
    CHECK_EQUAL_OBJECTS(t["msig_perms"], json_str_to_obj(ctrl.msig_perms_param())[1]);
    CHECK_EQUAL_OBJECTS(t["witness_votes"], json_str_to_obj(ctrl.max_witness_votes_param(_max_witness_votes))[1]);
    CHECK_EQUAL_OBJECTS(t["update_auth_period"], json_str_to_obj(ctrl.update_auth_param(_update_auth_period))[1]);
    produce_block();

    BOOST_TEST_MESSAGE("--- test fail when trying to create again");
    BOOST_CHECK_EQUAL(err.immutable, ctrl.set_params(_test_params));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(register_witness, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness registration");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_only_create);

    BOOST_TEST_MESSAGE("--- check registration success");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], "localhost"));
    auto w = mvo()("name","witn1")("url","localhost")("active",true)("total_weight",0);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), w);

    BOOST_TEST_MESSAGE("--- check properties update");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], "-"));
    produce_block();

    BOOST_CHECK_EQUAL(err.same_reg_props, ctrl.reg_witness(_w[0], "-"));
    string maxurl =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], maxurl));
    BOOST_CHECK_EQUAL(err.bad_url, ctrl.reg_witness(_w[0], maxurl+"!"));

    BOOST_TEST_MESSAGE("--- check stop witness");
    BOOST_CHECK_EQUAL(success(), ctrl.stop_witness(_w[0]));
    produce_block();

    BOOST_CHECK_EQUAL(err.not_udpated_flag, ctrl.stop_witness(_w[0]));
    BOOST_CHECK_EQUAL(err.no_witness, ctrl.stop_witness(_w[1]));

    BOOST_TEST_MESSAGE("--- check unreg");
    BOOST_CHECK_EQUAL(success(), ctrl.unreg_witness(_w[0]));
} FC_LOG_AND_RETHROW()

#if 0
BOOST_FIXTURE_TEST_CASE(register_update_witness, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Witness registration");

    BOOST_CHECK_EQUAL(success(), ctrl.set_params(_test_params));
    produce_block();

    ctrl.prepare_multisig(BLOG);
    produce_block();

    for (int i = 0; i < _n_w; i++) {
        BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[i], "localhost"));
    }
    produce_block();
    BOOST_TEST_MESSAGE("--- check registration success");

    prepare_balances();
    vector<std::tuple<name,name,bool>> votes = {
        {_alice, _w[1], true}, {_alice, _w[2], true}, {_alice, _w[3], false},
        {_alice, _w[0], true}, {_bob, _w[0], false}, {_w[0], _w[0], false}
    };

    for (const auto& v : votes) {
        BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(std::get<0>(v), std::get<1>(v)));
        BOOST_TEST_MESSAGE("--- check top witnesses");

        produce_blocks(golos::seconds_to_blocks(_update_auth_period));
        auto current_time = control->head_block_time().time_since_epoch();

        auto top_witnesses = ctrl.get_msig_auths();
        auto last_update_top_witnesses = top_witnesses["last_update"].as<fc::time_point>().time_since_epoch();

        BOOST_CHECK_EQUAL(last_update_top_witnesses == current_time, std::get<2>(v));

        auto save_top_witnesses = top_witnesses["witnesses"].as<vector<name>>();
        auto list_top_witnesses = ctrl.get_all_witnesses();

        std::sort(list_top_witnesses.begin(), list_top_witnesses.end(), [](const auto &it1, const auto &it2) {
            return it1["total_weight"].as_uint64() > it2["total_weight"].as_uint64();
        });

        list_top_witnesses.erase(std::remove_if(list_top_witnesses.begin(), list_top_witnesses.end(), [](const auto& item){
            return !bool(item["total_weight"].as_uint64());
        }), list_top_witnesses.end());


        vector<name> top(list_top_witnesses.size());
        std::transform(list_top_witnesses.begin(), list_top_witnesses.end(), top.begin(), [](auto& w) {
            return name(w["name"].as_string());
        });

        top.resize(_max_witnesses);
        if (save_top_witnesses.size() < _max_witnesses) // TODO when adding the first element the list of witnesses is less than the specified number
            top.resize(save_top_witnesses.size());

        std::sort(top.begin(), top.end(), [](const auto &it1, const auto &it2) {
            return it1.value < it2.value;
        });

        auto result = std::equal(save_top_witnesses.begin(), save_top_witnesses.end(), top.begin(), [&] (const auto &old_element, const auto &new_element) {
            return old_element.value == new_element.value;
        });

        BOOST_CHECK_EQUAL(true, result);
    }
} FC_LOG_AND_RETHROW()
#endif

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
    BOOST_CHECK_EQUAL(err.assert_erase_wtnss, ctrl.unreg_witness(_w[0]));
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
    auto wp = mvo()("name","witn1")("url","localhost")("active",true);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",(800+700+100)*_wmult));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check witness weight after unvote");
    BOOST_CHECK_EQUAL(success(), ctrl.unvote_witness(_bob, _w[0]));
    produce_block();

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
    BOOST_CHECK_EQUAL(err.no_votes, ctrl.unvote_witness(_carol, _w[0]));
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

BOOST_FIXTURE_TEST_CASE(set_params, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Set parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    prepare(step_only_create);

    BOOST_TEST_MESSAGE("--- check that setting immutable parameters fail");
    BOOST_CHECK_EQUAL(err.immutable, ctrl.set_param(ctrl.token_param(_token)));
    BOOST_CHECK_EQUAL(err.immutable, ctrl.set_param(ctrl.max_witness_votes_param()));

    BOOST_TEST_MESSAGE("--- check that setting same parameters fail");
    BOOST_CHECK_EQUAL(err.same_params, ctrl.set_param(ctrl.multisig_param(BLOG)));
    BOOST_CHECK_EQUAL(err.same_params, ctrl.set_param(ctrl.max_witnesses_param(_max_witnesses)));
    BOOST_CHECK_EQUAL(err.same_params, ctrl.set_param(ctrl.msig_perms_param()));

    BOOST_TEST_MESSAGE("--- check that setting invalid parameters fail");
    // TODO: maybe move to separate parameters validation test
    BOOST_CHECK_EQUAL(err.no_msig_acc, ctrl.set_param(ctrl.multisig_param(N(nobody))));
    BOOST_CHECK_EQUAL(err.max_witness0, ctrl.set_param(ctrl.max_witnesses_param(0)));
    BOOST_CHECK_EQUAL(err.auth_period0, ctrl.set_param(ctrl.update_auth_param(0)));

    BOOST_TEST_MESSAGE("--- check that setting valid parameters succeed");
    BOOST_CHECK_EQUAL(success(), ctrl.set_param(ctrl.msig_perms_param(_max_witnesses)));

    BOOST_TEST_MESSAGE("--- check that setting max_witnesses fails if conflicts with current msig_perms");
    BOOST_CHECK_EQUAL(err.smaj_gt_max, ctrl.set_param(ctrl.max_witnesses_param(_max_witnesses-1)));

    // TODO: test that multisig changes depending on max_witnesses
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
    auto wp = mvo()("name","witn1")("url","localhost")("active",true);
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",_wmult*(800+700+100)));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",_wmult*(800))("name","witn2"));
    BOOST_CHECK_EQUAL(success(), token.issue(_issuer, _alice, dasset(100), "issue"));
    BOOST_CHECK_EQUAL(success(), token.transfer(_alice, cfg::vesting_name, dasset(100), "buy vesting"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[0]), wp("total_weight",_wmult*(800+700+100+100))("name","witn1"));
    CHECK_MATCHING_OBJECT(ctrl.get_witness(_w[1]), wp("total_weight",_wmult*(800+100))("name","witn2"));
    produce_block();

    // TODO: check decreasing vesting and paths, other than `issue`+`transfer`
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_auths, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Checking update auths period");
    prepare(step_only_create);
    prepare_balances();

    BOOST_TEST_MESSAGE("--- checking that update auths possible only once");
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[0], "localhost"));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_bob, _w[0]));
    produce_blocks(golos::seconds_to_blocks(_update_auth_period)-1);
    BOOST_CHECK_EQUAL(success(), ctrl.reg_witness(_w[1], "localhost"));
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_bob, _w[1]));
    auto top_witnesses = ctrl.get_msig_auths();
    std::vector<name> wtns = {_w[0]};
    BOOST_CHECK(top_witnesses["witnesses"].as<std::vector<name>>().size() ==  wtns.size());

    BOOST_TEST_MESSAGE("--- checking that update auths possible again");
    produce_blocks(2);
    BOOST_CHECK_EQUAL(success(), ctrl.vote_witness(_alice, _w[1]));
    wtns.push_back(_w[1]);
    top_witnesses = ctrl.get_msig_auths();
    BOOST_CHECK(top_witnesses["witnesses"].as<std::vector<name>>().size() == wtns.size());
} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
