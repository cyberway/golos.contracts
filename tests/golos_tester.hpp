#pragma once
#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <fc/variant_object.hpp>

namespace fc {
uint64_t hash64(const std::string& arg);
}

// Note: cannot check nested mvo
#define CHECK_EQUAL_OBJECTS(left, right) { \
    auto a = fc::variant(left); \
    auto b = fc::variant(right); \
    BOOST_CHECK_EQUAL(true, a.is_object()); \
    BOOST_CHECK_EQUAL(true, b.is_object()); \
    if (a.is_object() && b.is_object()) { \
        BOOST_CHECK_EQUAL_COLLECTIONS(a.get_object().begin(), a.get_object().end(), b.get_object().begin(), b.get_object().end()); }}

#define CHECK_MATCHING_OBJECT(left, right) { \
    auto a = fc::variant(left); \
    auto b = fc::variant(right); \
    BOOST_CHECK_EQUAL(true, a.is_object()); \
    BOOST_CHECK_EQUAL(true, b.is_object()); \
    if (a.is_object() && b.is_object()) { \
        auto filtered = ::eosio::testing::filter_fields(a.get_object(), b.get_object()); \
        BOOST_CHECK_EQUAL_COLLECTIONS(a.get_object().begin(), a.get_object().end(), filtered.begin(), filtered.end()); }}

namespace eosio { namespace testing {


// using namespace eosio::testing;

// TODO: maybe use native db struct
struct permission {
    name        perm_name;
    name        parent;
    authority   required_auth;
};


class golos_tester : public tester {
protected:
    const name _code;     // base values to make things simpler
    const name _scope;

public:
    golos_tester() {}
    golos_tester(name code, name scope): tester(), _code(code), _scope(scope) {}

    void install_contract(account_name acc, const std::vector<uint8_t>& wasm, const std::vector<char>& abi,
        abi_serializer& abi_ser, bool produce = true);

    std::vector<permission> get_account_permissions(account_name a);

    action_result push_action(const abi_serializer* abi, account_name code, action_name name,
        const variant_object& data, account_name signer);
    action_result push_action_msig_tx(const abi_serializer* abi, account_name code, action_name name,
        const variant_object& data, std::vector<permission_level> perms, std::vector<account_name>signers);
    action_result push_tx(signed_transaction&& tx);

    // table helpers
    const table_id_object* find_table(name code, name scope, name tbl) const;
    // Note: uses `lower_bound`, so caller must check id of returned value
    std::vector<char> get_tbl_row(name code, name scope, name tbl, uint64_t id) const;
    fc::variant get_tbl_struct(name code, name scope, name tbl, uint64_t id, const std::string& n, const abi_serializer& abi) const;
    fc::variant get_tbl_struct_singleton(name code, name scope, name tbl, const string &n, const abi_serializer &abi) const;

    // simplified versions
    const table_id_object* find_table(name tbl) const {
        return find_table(_code, _scope, tbl);
    }
    std::vector<char> get_row(name tbl, uint64_t id) const {
        return get_tbl_row(_code, _scope, tbl, id);
    }
    
    vector<vector<char> > get_all_rows(uint64_t code, uint64_t scope, uint64_t table, bool strict = true) const;
    
    fc::variant get_struct(name tbl, uint64_t id, const std::string name, const abi_serializer& abi) const {
        return get_tbl_struct(_code, _scope, tbl, id, name, abi);
    }
};


}} // eosio::tesing

FC_REFLECT(eosio::testing::permission, (perm_name)(parent)(required_auth))
