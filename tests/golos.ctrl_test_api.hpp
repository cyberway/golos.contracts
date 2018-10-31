#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {


struct golos_ctrl_api: domain_contract_api<symbol> {
    using domain_contract_api::domain_contract_api;

    // name _owner; // TODO: domain linked to owner, some actions can have shortcuts with no need to pass owner

    const name _minority_name = N(witn.minor);
    const name _majority_name = N(witn.major);

    //// control actions
    action_result create(account_name owner, mvo props) {
        return push(N(create), owner, args()
            ("owner", owner)
            ("props", props)
        );
    }

    action_result set_props(account_name owner, vector<account_name> signers, mvo props) {
        return push_msig(N(updateprops), {{owner, config::active_name}}, signers, args()
            ("props", props)
        );
    }

    action_result attach_acc_ex(account_name owner, vector<account_name> signers, account_name user) {
        return push_msig(N(attachacc), {{owner, _minority_name}}, signers, args()
            ("user", user)
        );
    }

    action_result detach_acc_ex(account_name owner, vector<account_name> signers, account_name user) {
        return push_msig(N(detachacc), {{owner, _minority_name}}, signers, args()
            ("user", user)
        );
    }

    action_result attach_acc(account_name owner, vector<account_name> signers, account_name user) {
        return attach_acc_ex(owner, signers, user);
    }

    action_result detach_acc(account_name owner, vector<account_name> signers, account_name user) {
        return detach_acc_ex(owner, signers, user);
    }

    action_result reg_witness(account_name witness, string key, string url) {
        return push(N(regwitness), witness, args()
            ("witness", witness)
            ("key", key)    // TODO: remove
            ("url", url)
        );
    }

    action_result unreg_witness(account_name witness) {
        return push(N(unregwitness), witness, args()
            ("witness", witness)
        );
    }

    action_result vote_witness(account_name voter, account_name witness, uint16_t weight=10000) {
        return push(N(votewitness), voter, args()
            ("voter", voter)
            ("witness", witness)
            ("weight", weight));
    }
    action_result unvote_witness(account_name voter, account_name witness) {
        return push(N(unvotewitn), voter, args()
            ("voter", voter)
            ("witness", witness));
    }

    action_result change_vests(account_name owner, asset diff) {
        return push(N(changevest), owner, args()
            ("owner", owner)
            ("diff", diff));
    }

    //// control tables
    variant get_attached(account_name user) const {
        return get_struct(N(bwuser), user, "bw_user");
    }

    variant get_props(account_name owner) const {
        return get_struct(N(props), owner, "ctrl_props");
    }

    variant get_witness(account_name witness) const {
        return get_struct(N(witness), witness, "witness_info");
    }

    //// helpers
    void prepare_owner(account_name owner) {
        // witn.major/minor
        auto auth = authority(1, {}, {
            {.permission = {owner, config::active_name}, .weight = 1}
        });
        _tester->set_authority(owner, _majority_name, auth, "active");
        _tester->set_authority(owner, _minority_name, auth, "active");
        // link witn.minor to allow attachacc/detachacc
        _tester->link_authority(owner, _code, _minority_name, N(attachacc));
        _tester->link_authority(owner, _code, _minority_name, N(detachacc));

        //eosio.code
        auto code_auth = authority(1, {}, {
            {.permission = {_code, config::eosio_code_name}, .weight = 1}
        });
        _tester->set_authority(owner, config::owner_name, code_auth, 0);
        code_auth.keys = {{_tester->get_public_key(owner, "active"), 1}};
        _tester->set_authority(owner, config::active_name, code_auth, "owner",
            {permission_level{owner, config::active_name}}, {_tester->get_private_key("blog", "active")});
    }

};


}} // eosio::testing
