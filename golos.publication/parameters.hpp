#pragma once
#include <eosiolib/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>

namespace golos {

    using namespace eosio;

    struct st_max_vote_changes : parameter {
        uint8_t max_vote_changes;

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using max_vote_changes_prm = param_wrapper<st_max_vote_changes, 1>;

    struct st_cashout_window : parameter {
        uint32_t window;
        uint32_t upvote_lockout;

        void validate() const override {
            eosio_assert(window > 0, "Cashout window must be greater than 0.");
            eosio_assert(window > upvote_lockout, "Cashout window can't be less than upvote lockout.");
        }

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using cashout_window_prm = param_wrapper<st_cashout_window, 2>;

    struct st_max_beneficiaries : parameter {
        uint8_t max_beneficiaries;

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using max_beneficiaries_prm = param_wrapper<st_max_beneficiaries, 1>;

    struct st_max_comment_depth : parameter {
        uint16_t max_comment_depth;
        
        void validate() const override {
            eosio_assert(max_comment_depth > 0, "Max comment depth must be greater than 0.");
        }

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using max_comment_depth_prm = param_wrapper<st_max_comment_depth, 1>;

    struct st_social_acc : parameter {
        name account;

        void validate() const override {
            if (account != name()) {
                eosio_assert(is_account(account), "Social account doesn't exist.");
            }
        }

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using social_acc_prm = param_wrapper<st_social_acc, 1>;
    
    struct st_referral_acc : parameter {
        name account;

        void validate() const override {
            if (account != name()) {
                eosio_assert(is_account(account), "Referral account doesn't exist.");
            }
        }

        template<typename T>
        bool compare(const T& other) const {
            return *this != other;
        };
    };
    using referral_acc_prm = param_wrapper<st_referral_acc, 1>;


    using posting_params = std::variant<max_vote_changes_prm, cashout_window_prm, max_beneficiaries_prm, 
          max_comment_depth_prm, social_acc_prm, referral_acc_prm>;

    struct [[eosio::table]] posting_state {
        max_vote_changes_prm max_vote_changes_param;
        cashout_window_prm cashout_window_param;
        max_beneficiaries_prm max_beneficiaries_param;
        max_comment_depth_prm max_comment_depth_param;
        social_acc_prm social_acc_param;
        referral_acc_prm referral_acc_param;

        static constexpr int params_count = 6;
    };
    using posting_params_singleton = eosio::singleton<"pstngparams"_n, posting_state>;

} //golos
