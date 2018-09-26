#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>

#include "contracts.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace fc;

using mvo = fc::mutable_variant_object;
using symbol_type = symbol;


class golos_ctrl_tester : public tester {
public:

    const account_name ME = N(golos.ctrl);

    golos_ctrl_tester() {
        create_accounts({ME, N(witnesstop), N(witn1), N(witn2), N(witn3), N(witn4), N(witn5), N(alice), N(bob), N(carol)});
        produce_block();

        set_code(ME, contracts::ctrl_wasm());
        set_abi(ME, contracts::ctrl_abi().data());
        produce_blocks();

        const auto& accnt = control->db().get<account_object,by_name>(ME);
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);
    }

    action_result push_action(const account_name& signer, const action_name& name, const variant_object& data) {
        string action_type_name = abi_ser.get_action_type(name);
        action act;
        act.account = ME;
        act.name    = name;
        act.data    = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);
        return base_tester::push_action(std::move(act), uint64_t(signer));
    }

    // action_result set_props(account_name signer, symbol_type token, properties props) {
    action_result set_props(account_name signer, symbol_type token, mvo props) {
        return push_action(signer, N(updateprops), mvo()("domain", token)
            ("props",   props)
        );
    }

    action_result attach_acc(symbol_type token, account_name user) {
        return push_action(user, N(attachacc), mvo()("domain", token)
            ("user",    user)
        );
    }
    action_result detach_acc(symbol_type token, account_name user) {
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

    vector<char> get_row(name tbl, uint64_t key) const {
        return get_row_by_account(ME, _token.value(), tbl, key);
    }

    fc::variant check_attached(const account_name user) const {
        vector<char> data = get_row(N(bwuser), user);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("bw_user", data, abi_serializer_max_time);
    }

    const symbol_type _token = symbol(3,"TST");
    const account_name _top = N(witnesstop);          // TODO: contract / witness multisig
    abi_serializer abi_ser;
};


BOOST_AUTO_TEST_SUITE(golos_ctrl_tests)

BOOST_FIXTURE_TEST_CASE(ctrl_attach_account, golos_ctrl_tester) try {
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

    BOOST_CHECK_EQUAL("", attach_acc(_token, al));
    // BOOST_CHECK_EQUAL(wasm_assert_msg(""), attach_acc(_token, N(bob)));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
