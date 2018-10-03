#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/permission_object.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>

#include "contracts.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace fc;

using mvo = fc::mutable_variant_object;
using symbol_type = symbol;


// TODO: maybe use native db struct
struct permission {
    name        perm_name;
    name        parent;
    authority   required_auth;
};
FC_REFLECT(permission, (perm_name)(parent)(required_auth))


class golos_ctrl_tester : public tester {
public:

    const account_name ME = N(golos.ctrl);
    const account_name BLOG = N(blog);

    golos_ctrl_tester() {
        create_accounts({ME, BLOG, N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), N(alice), N(bob), N(carol),
            N(golos.vest), N(eosio.token)});
        produce_block();

        set_code(ME, contracts::ctrl_wasm());
        set_abi(ME, contracts::ctrl_abi().data());
        produce_blocks();

        const auto& accnt = control->db().get<account_object,by_name>(ME);
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);

        BOOST_TEST_MESSAGE("--- Create and initialize vesting contract");
        set_code(N(eosio.token), contracts::token_wasm());
        set_abi (N(eosio.token), contracts::token_abi().data());
        set_code(N(golos.vest), contracts::vesting_wasm());
        set_abi (N(golos.vest), contracts::vesting_abi().data());

        const auto& acc_v = control->db().get<account_object,by_name>(N(golos.vest));
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(acc_v.abi, abi), true);
        abi_ser_vesting.set_abi(abi, abi_serializer_max_time);
        const auto& acc_t = control->db().get<account_object,by_name>(N(eosio.token));
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(acc_t.abi, abi), true);
        abi_ser_token.set_abi(abi, abi_serializer_max_time);
    }

    vector<permission> get_account_permissions(account_name a) {
        vector<permission> r;
        const auto& d = control->db();
        const auto& perms = d.get_index<permission_index,by_owner>();
        auto perm = perms.lower_bound(boost::make_tuple(a));
        while (perm != perms.end() && perm->owner == a) {
            name parent;
            if (perm->parent._id) {
                const auto* p = d.find<permission_object,by_id>(perm->parent);
                if (p) {
                    parent = p->name;
                }
            }
            r.push_back(permission{perm->name, parent, perm->auth.to_authority()});
            ++perm;
        }
        return r;
    }

    abi_serializer* acc2abi(account_name a) {
        abi_serializer* r = &abi_ser;
        switch (uint64_t(a)) {
            case N(golos.vest):  r = &abi_ser_vesting; break;
            case N(eosio.token): r = &abi_ser_token; break;
        }
        return r;
    }

    action_result push_action(const account_name& who, const account_name& signer, const action_name& name, const variant_object& data) {
        const auto abi = acc2abi(who);
        action act;
        act.account = who;
        act.name    = name;
        act.data    = abi->variant_to_binary(abi->get_action_type(name), data, abi_serializer_max_time);
        return base_tester::push_action(std::move(act), uint64_t(signer));
    }
    action_result push_action(const account_name& signer, const action_name& name, const variant_object& data) {
        return push_action(ME, signer, name, data);
    }

    action_result create(symbol_type token, account_name owner, mvo props) {
        return push_action(owner, N(create), mvo()("domain", token)
            ("owner", owner)
            ("props", props)
        );
    }

    action_result set_props(account_name signer, symbol_type token, mvo props) {
        // TODO: supermajority signer
        return push_action(signer, N(updateprops), mvo()("domain", token)
            ("props",   props)
        );
    }

    action_result attach_acc(symbol_type token, account_name user) {
        // TODO: minority signer
        return push_action(_top, N(attachacc), mvo()("domain", token)
            ("user",    user)
        );
    }
    action_result detach_acc(symbol_type token, account_name user) {
        // TODO: minority signer
        return push_action(_top, N(detachacc), mvo()("domain", token)
            ("user",    user)
        );
    }

    action_result reg_witness(symbol_type token, account_name witness, string key, string url) {
        return push_action(witness, N(regwitness), mvo()("domain", token)
            ("witness", witness)
            ("key", fc::crypto::public_key(key))
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
    //     push_action(ME, N(updatetop), _sys, mvo()("domain", token));
    // }

    // vesting stuff
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
    const table_id_object* find_table(name code, name scope, name table) const {
        auto tid = control->db().find<table_id_object, by_code_scope_table>(boost::make_tuple(code, scope, table));
        return tid;
    }
    // caller must check if id equal to requested (because of lower_bound)
    vector<char> get_row_by_id(name tbl, uint64_t id) const {
        vector<char> data;
        const auto& tid = find_table(ME, _token.value(), tbl);
        if (tid) {
            const auto& db = control->db();
            const auto& idx = db.get_index<chain::key_value_index, chain::by_scope_primary>();
            auto itr = idx.lower_bound(boost::make_tuple(tid->id, id));
            if (itr != idx.end() && itr->t_id == tid->id) {
                data.resize(itr->value.size());
                memcpy(data.data(), itr->value.data(), data.size());
            }
        }
        return data;
    }

    vector<char> get_row(name tbl, uint64_t key) const {
        return get_row_by_account(ME, _token.value(), tbl, key);
    }

    fc::variant check_attached(const account_name user) const {
        vector<char> data = get_row(N(bwuser), user);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("bw_user", data, abi_serializer_max_time);
    }

    fc::variant get_props() const {
        vector<char> data = get_row_by_id(N(props), 0);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("ctrl_props", data, abi_serializer_max_time);
    }

    fc::variant get_witness(const account_name witness) const {
        vector<char> data = get_row_by_id(N(witness), witness);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("witness_info", data, abi_serializer_max_time);
    }

    fc::variant get_account_vesting(const account_name acc, const symbol_type sym) const {
        vector<char> data = get_row_by_account(N(golos.vest), acc, N(balances), sym.to_symbol_code());
        return data.empty() ? fc::variant() : abi_ser_vesting.binary_to_variant("user_balance", data, abi_serializer_max_time);
    }


    void check_vesting_balance(const fc::variant& obj, const asset vesting, const asset dlg_vest = asset::from_string("0.000000 TST"), const asset rcv_vest = asset::from_string("0.000000 TST")) {
        BOOST_CHECK_EQUAL(true, obj.is_object());
        BOOST_CHECK_EQUAL(obj["vesting"].as<asset>(), vesting);
        BOOST_CHECK_EQUAL(obj["delegate_vesting"].as<asset>(), dlg_vest);
        BOOST_CHECK_EQUAL(obj["received_vesting"].as<asset>(), rcv_vest);
        // BOOST_CHECK_EQUAL(obj["battery"].get_object()["charge"].as<uint16_t>(), obj2.get_object()["battery"].get_object()["charge"].as<uint16_t>());
    }

///
    asset dasset(double val = 0) const {
        return asset(val*1e6, _token); // 6 is precision. todo: derive from symbol and less hardcode
    }
    const symbol_type _token = symbol(6,"TST");
    const account_name _top = BLOG;
    const uint64_t _w[5] = {N(witn1), N(witn2), N(witn3), N(witn4), N(witn5)};

    abi_serializer abi_ser;
    abi_serializer abi_ser_vesting;
    abi_serializer abi_ser_token;
};


BOOST_AUTO_TEST_SUITE(golos_ctrl_tests)

BOOST_FIXTURE_TEST_CASE(create_community, golos_ctrl_tester) try {
    BOOST_TEST_MESSAGE("Test create_community");
    auto key = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";
    auto key2 = "EOS5jnmSKrzdBHE9n8hw58y7yxFWBC8SNiG7m8S1crJH3KvAnf9o6";

    BOOST_TEST_MESSAGE("--- Check that actions disabled before community created");
    BOOST_CHECK_EQUAL(true, get_props().is_null());
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), reg_witness(_token, _w[0], key, "http://ya.ru"));
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), unreg_witness(_token, _w[0]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), vote_witness(_token, N(alice), _w[0]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), unvote_witness(_token, N(alice), _w[0]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), attach_acc(_token, N(carol)));
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), detach_acc(_token, N(carol)));

    auto p = mvo()("max_witnesses",2)("max_witness_votes",4)
        ("witness_supermajority",0)("witness_majority",0)("witness_minority",0);
    BOOST_CHECK_EQUAL(wasm_assert_msg("symbol not found"), set_props(_top, _token, p));

    BOOST_TEST_MESSAGE("--- Create community");
    BOOST_CHECK_EQUAL(success(), create(_token, _top, p));
    const auto t = get_props();
    BOOST_CHECK_EQUAL(t["owner"], fc::variant(_top));
    REQUIRE_MATCHING_OBJECT(p, t["props"]); // this macro doesn't support nested objects
    produce_blocks(1);

    BOOST_TEST_MESSAGE("--- Reg witnesses");
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[0], key, "https://ya.ru"));
    auto z = mvo()("name","witn1")("key",key)("url","https://ya.ru")("active",true)("total_weight",0);
    REQUIRE_MATCHING_OBJECT(get_witness(0), z);
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[0], key, "-"));
    BOOST_CHECK_EQUAL(wasm_assert_msg("already updated in the same way"), reg_witness(_token, _w[0], key, "-"));

    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[1], key2, "https://1.ru"));
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[2], key, "https://2.ru"));
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[3], key, "https://3.ru"));
    BOOST_CHECK_EQUAL(success(), reg_witness(_token, _w[4], key, "https://4.ru"));
    produce_block();

    BOOST_CHECK_EQUAL(success(), unreg_witness(_token, _w[4]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("witness already unregistered"), unreg_witness(_token, _w[4]));

    BOOST_TEST_MESSAGE("--- Create some vesting for voters");
    produce_block();
    BOOST_CHECK_EQUAL(success(), create_pair(asset::from_string("0.0000 GLS"), dasset(0)));
    vector<std::pair<uint64_t,double>> amounts = {
        {BLOG, 1000}, {N(alice), 800}, {N(bob), 700}, {N(carol), 600},
        {_w[0], 100}, {_w[1], 200}, {_w[2], 300}, {_w[3], 400}, {_w[4], 500}
    };
    std::for_each(amounts.begin(), amounts.end(), [&](auto& p) {
        BOOST_CHECK_EQUAL(success(), open_vesting(p.first, _token, p.first));
        BOOST_CHECK_EQUAL(success(), accrue_vesting(N(golos.vest), p.first, dasset(p.second)));
    });

    BOOST_TEST_MESSAGE("--- Check vesting balance: " + _token.name());
    BOOST_CHECK_EQUAL(dasset(123), asset::from_string("123.000000 TST"));
    check_vesting_balance(get_account_vesting(N(alice), _token), dasset(800));
    produce_block();
    check_vesting_balance(get_account_vesting(N(carol), _token), dasset(600));

    BOOST_TEST_MESSAGE("--- Add eosio.code auth to Blog");
    BOOST_TEST_MESSAGE("Perms: " + fc::json::to_string(get_account_permissions(BLOG)));
    auto code_auth = authority(1, {}, {
        {.permission = {ME, config::eosio_code_name}, .weight = 1}
    });
    set_authority(BLOG, config::owner_name, code_auth, 0);
    set_authority(BLOG, config::active_name, code_auth, "owner",
        {permission_level{BLOG, config::active_name}}, {get_private_key("blog", "active")});
    BOOST_TEST_MESSAGE("Perms: " + fc::json::to_string(get_account_permissions(BLOG)));
    produce_block();

    BOOST_TEST_MESSAGE("--- Cast some votes");
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[0]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[1]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[2]));
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[3]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("all allowed votes already casted"), vote_witness(_token, N(alice), _w[4]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(bob), _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, _w[0], _w[0]));
    produce_block();
    BOOST_CHECK_EQUAL(wasm_assert_msg("already voted"), vote_witness(_token, N(alice), _w[0]));
    produce_block();
    BOOST_TEST_MESSAGE("P: " + fc::json::to_string(get_account_permissions(BLOG)));

    BOOST_TEST_MESSAGE("--- Check witness weight");
    z = z("url","-");
    REQUIRE_MATCHING_OBJECT(get_witness(_w[0]), z("total_weight",(800+700+100)*1e6));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check witness weight after unvote");
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, N(bob), _w[0]));
    BOOST_CHECK_EQUAL(wasm_assert_msg("there is no vote for this witness"), unvote_witness(_token, N(bob), _w[0]));
    REQUIRE_MATCHING_OBJECT(get_witness(_w[0]), z("total_weight",(800+100)*1e6));
    produce_block();

    BOOST_TEST_MESSAGE("--- Check unvote and vote again");
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, N(alice), _w[3]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[3]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), unvote_witness(_token, N(alice), _w[3]));
    produce_block();
    BOOST_CHECK_EQUAL(success(), vote_witness(_token, N(alice), _w[4]));
    produce_block();

} FC_LOG_AND_RETHROW()

/*BOOST_FIXTURE_TEST_CASE(attach_account, golos_ctrl_tester) try {
    //broken
    auto al = N(alice);
    auto attached = check_attached(al);
    BOOST_CHECK_EQUAL(true, attached.is_null());
    auto r = attach_acc(_token, al);
    produce_blocks(1);

    attached = check_attached(al);
    // BOOST_CHECK_EQUAL(fc::variant::null_type, attached.get_type());
    REQUIRE_MATCHING_OBJECT(attached, mvo()
        ("name", "alice")
        ("attached", true)
    );
    // r = detach_acc(_token, al);
    // produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), attach_acc(_token, al));
    // BOOST_CHECK_EQUAL(wasm_assert_msg(""), attach_acc(_token, N(bob)));
} FC_LOG_AND_RETHROW()*/

BOOST_AUTO_TEST_SUITE_END()
