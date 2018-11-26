#pragma once
#include "golos_tester.hpp"
#include <boost/algorithm/string/replace.hpp>

namespace eosio { namespace testing {

using mvo = fc::mutable_variant_object;
using action_result = base_tester::action_result;

// allows to use ' quotes instead of ", but will break if ' used not as quote
inline fc::variant json_str_to_obj(std::string s) {
    return fc::json::from_string(boost::replace_all_copy(s, "'", "\""));
}

struct base_contract_api {
    golos_tester* _tester;
    name _code;

    base_contract_api(golos_tester* tester, name code): _tester(tester), _code(code) {}

    action_result push(action_name name, account_name signer, const variant_object& data) {
        return _tester->push_action(_code, name, signer, data);
    }

    base_tester::action_result push_msig(action_name name, std::vector<permission_level> perms, std::vector<account_name> signers,
        const variant_object& data
    ) {
        return _tester->push_action_msig_tx(_code, name, perms, signers, data);
    }

    variant get_struct(uint64_t scope, name tbl, uint64_t id, const string& name) const {
        return _tester->get_chaindb_struct(_code, scope, tbl, id, name);
    }

    virtual mvo args() {
        return mvo();
    }

};

template<typename DomainType>
struct domain_contract_api: base_contract_api {
    DomainType _domain;
    domain_contract_api(golos_tester* tester, name code, DomainType domain)
    : base_contract_api(tester, code)
    , _domain(domain) {}

    uint64_t domain_to_scope() const;

    variant get_struct(name tbl, uint64_t id, const string& name) const {
        return base_contract_api::get_struct(domain_to_scope(), tbl, id, name);
    }

    //base args
    mvo args() override {
        return mvo()("domain", _domain);
    }
};

template<>
inline uint64_t domain_contract_api<symbol>::domain_to_scope() const {
    return _domain.value();
}

} }
