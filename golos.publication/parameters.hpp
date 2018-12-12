#pragma once
#include <eosiolib/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>

namespace golos {

    using namespace eosio;

    struct st_max_vote_changes : parameter {
        uint8_t max_vote_changes;
    };
    using max_vote_changes_prm = param_wrapper<st_max_vote_changes, 1>;

    struct st_common : parameter {
        uint32_t cashout_window;
        uint32_t upvote_lockout;

        void validate() const override {
            eosio_assert(cashout_window > 0, "Cashout window must be greater than 0.");
        }
    };
    using common_prm = param_wrapper<st_common, 2>;

    struct st_max_beneficiaries : parameter {
        uint8_t max_beneficiaries;
    };
    using max_beneficiaries_prm = param_wrapper<st_max_beneficiaries, 1>;

    struct st_max_comment_depth : parameter {
        uint16_t max_comment_depth;
        
        void validate() const override {
            eosio_assert(max_comment_depth > 0, "Max comment depth must be greater than 0.");
        }
    };
    using max_comment_depth_prm = param_wrapper<st_max_comment_depth, 1>;

    using posting_params = std::variant<max_vote_changes_prm, common_prm, max_beneficiaries_prm, max_comment_depth_prm>;

    struct [[eosio::table]] posting_state {
        max_vote_changes_prm max_vote_changes_param;
        common_prm common_param;
        max_beneficiaries_prm max_beneficiaries_param;
        max_comment_depth_prm max_comment_depth_param;

        static constexpr int params_count = 4;
    };
    using posting_params_singleton = eosio::singleton<"pstngparams"_n, posting_state>;

} //golos
