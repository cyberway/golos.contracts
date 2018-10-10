#include "golos_tester.hpp"
#include <eosio/chain/permission_object.hpp>

namespace eosio { namespace testing {

using std::vector;
using std::string;

void golos_tester::install_contract(
    account_name acc, const std::vector<uint8_t>& wasm, const std::vector<char>& abi,
    abi_serializer& abi_ser, bool produce
) {
    set_code(acc, wasm);
    set_abi (acc, abi.data());
    if (produce)
        produce_block();
    const auto& accnt = control->db().get<account_object,by_name>(acc);
    abi_def abi_d;
    BOOST_CHECK_EQUAL(abi_serializer::to_abi(accnt.abi, abi_d), true);
    abi_ser.set_abi(abi_d, abi_serializer_max_time);
};

vector<permission> golos_tester::get_account_permissions(account_name a) {
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

base_tester::action_result golos_tester::push_action(
    const abi_serializer* abi,
    account_name code,
    action_name name,
    const variant_object& data,
    account_name signer
) {
    action act;
    act.account = code;
    act.name    = name;
    act.data    = abi->variant_to_binary(abi->get_action_type(name), data, abi_serializer_max_time);
    return base_tester::push_action(std::move(act), uint64_t(signer));
}

base_tester::action_result golos_tester::push_action_msig_tx(
    const abi_serializer* abi,
    account_name code,
    action_name name,
    const variant_object& data,
    vector<permission_level> perms,
    vector<account_name> signers
) {
    action act;
    act.account = code;
    act.name    = name;
    act.data    = abi->variant_to_binary(abi->get_action_type(name), data, abi_serializer_max_time);
    for (const auto& perm : perms) {
        act.authorization.emplace_back(perm);
    }

    signed_transaction tx;
    tx.actions.emplace_back(std::move(act));
    set_transaction_headers(tx);
    for (const auto& a : signers) {
        tx.sign(get_private_key(a, "active"), control->get_chain_id());
    }
    return push_tx(std::move(tx));
}

base_tester::action_result golos_tester::push_tx(signed_transaction&& tx) {
    try {
        push_transaction(tx);
    } catch (const fc::exception& ex) {
        edump((ex.to_detail_string()));
        return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
        //return error(ex.to_detail_string());
    }
    produce_block();
    BOOST_REQUIRE_EQUAL(true, chain_has_transaction(tx.id()));
    return success();
}

// table helpers
const table_id_object* golos_tester::find_table(name code, name scope, name tbl) const {
    auto tid = control->db().find<table_id_object, by_code_scope_table>(std::make_tuple(code, scope, tbl));
    return tid;
}

vector<char> golos_tester::get_tbl_row(name code, name scope, name tbl, uint64_t id) const {
    vector<char> data;
    const auto& tid = find_table(code, scope, tbl);
    if (tid) {
        const auto& db = control->db();
        const auto& idx = db.get_index<chain::key_value_index, chain::by_scope_primary>();
        auto itr = idx.lower_bound(std::make_tuple(tid->id, id));
        if (itr != idx.end() && itr->t_id == tid->id) {
            data.resize(itr->value.size());
            memcpy(data.data(), itr->value.data(), data.size());
        }
    }
    return data;
}

fc::variant golos_tester::get_tbl_struct(name code, name scope, name tbl, uint64_t id,
    const string& n, const abi_serializer& abi
) const {
    vector<char> data = get_tbl_row(code, scope, tbl, id);
    return data.empty() ? fc::variant() : abi.binary_to_variant(n, data, abi_serializer_max_time);
}

fc::variant golos_tester::get_tbl_struct_singleton(name code, name scope, name tbl, const abi_serializer& abi) const {
    vector<char> data = get_row_by_account( code, scope, tbl, tbl );
    if (data.empty())
        std::cout << "\nData is empty\n" << std::endl;
    return data.empty() ? fc::variant() : abi.binary_to_variant( "account_battery", data, abi_serializer_max_time );
}


}} // eosio::tesing
