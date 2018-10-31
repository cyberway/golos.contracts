#pragma once
#include "golos_tester.hpp"

namespace eosio { namespace testing {

using mvo = fc::mutable_variant_object;
using action_result = base_tester::action_result;


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
    variant get_struct(name tbl, uint64_t id, const string& name) const {
        return _tester->get_chaindb_struct(tbl, id, name);
    }

    virtual mvo args() {
        return mvo();
    }

};

template<typename D>
struct domain_contract_api: base_contract_api {
    D _domain;
    domain_contract_api(golos_tester* tester, name code, D domain): base_contract_api(tester, code), _domain(domain) {}

    //base args
    mvo args() override {
        return mvo()("domain", _domain);
    }
};

} }
