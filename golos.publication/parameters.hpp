#pragma once
#include <eosiolib/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>

namespace golos {

    using namespace eosio;

    struct st_max_vote_changes : parameter {
        uint8_t max_vote_changes;

        void validate() const override {
            eosio_assert(max_vote_changes > 0, "Max vote changes can't be negative.");
        }
    };
    using max_vote_changes_prm = param_wrapper<st_max_vote_changes, 1>;

    struct st_cashout_window : parameter {
        uint32_t cashout_window;

        void validate() const override {
            eosio_assert(cashout_window > 0, "Cashout window can't be negative.");
        }
    };
    using cashout_window_prm = param_wrapper<st_cashout_window, 1>;

    struct st_upvote_lockout : parameter {
        uint8_t upvote_lockout;
        
        void validate() const override {
            eosio_assert(upvote_lockout > 0, "Upvote lockout can't be negative.");
        }
    };
    using upvote_lockout_prm = param_wrapper<st_upvote_lockout, 1>;

    struct st_max_beneficiaries : parameter {
        uint8_t max_beneficiaries;
        
        void validate() const override {
            eosio_assert(max_beneficiaries > 0, "Max beneficiaries can't be negative.");
        }
    };
    using max_beneficiaries_prm = param_wrapper<st_max_beneficiaries, 1>;

    struct st_max_comment_depth : parameter {
        uint16_t max_comment_depth;
        
        void validate() const override {
            eosio_assert(max_comment_depth > 0, "Max comment depth can't be negative.");
        }
    };
    using max_comment_depth_prm = param_wrapper<st_max_comment_depth, 1>;

    using posting_params = std::variant<max_vote_changes_prm, cashout_window_prm, 
          upvote_lockout_prm, max_beneficiaries_prm, max_comment_depth_prm>;

    struct [[eosio::table]] posting_state {
        max_vote_changes_prm max_vote_changes_param;
        cashout_window_prm cashout_window_param;
        upvote_lockout_prm upvote_lockout_param;
        max_beneficiaries_prm max_beneficiaries_param;
        max_comment_depth_prm max_comment_depth_param;

        static constexpr int params_count = 5;
    };
    using posting_params_singleton = eosio::singleton<"pstngparams"_n, posting_state>;

} //golos
