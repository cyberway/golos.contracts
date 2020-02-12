#pragma once
#include <eosio/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>
#include <common/config.hpp>

namespace golos {

    using namespace eosio;

    struct st_max_vote_changes_t : parameter {
        uint8_t max_vote_changes;
    };
    using max_vote_changes_prm = param_wrapper<st_max_vote_changes, 1>;

    struct st_cashout_window : parameter {
        uint32_t window;
        uint32_t upvote_lockout;

        void validate() const override {
            eosio::check(window > 0, "Cashout window must be greater than 0.");
            eosio::check(window > upvote_lockout, "Cashout window can't be less than upvote lockout.");
        }
    };
    using cashout_window_prm = param_wrapper<st_cashout_window, 2>;

    struct st_max_beneficiaries_t : parameter {
        uint8_t max_beneficiaries;
    };
    using max_beneficiaries_prm = param_wrapper<st_max_beneficiaries, 1>;

    struct st_max_comment_depth_t : parameter {
        uint16_t max_comment_depth;

        void validate() const override {
            eosio::check(max_comment_depth > 0, "Max comment depth must be greater than 0.");
        }
    };
    using max_comment_depth_prm = param_wrapper<st_max_comment_depth, 1>;

    struct st_social_acc_t : parameter {
        name account;

        void validate() const override {
            if (account != name()) {
                eosio::check(is_account(account), "Social account doesn't exist.");
            }
        }
    };
    using social_acc_prm = param_wrapper<st_social_acc, 1>;

    struct st_referral_acc_t : parameter {
        name account;

        void validate() const override {
            if (account != name()) {
                eosio::check(is_account(account), "Referral account doesn't exist.");
            }
        }
    };
    using referral_acc_prm = param_wrapper<st_referral_acc, 1>;

    struct st_curators_prcnt_t : parameter {
        uint16_t min_curators_prcnt;
        uint16_t max_curators_prcnt;

        void validate() const override {
            eosio::check(min_curators_prcnt <= config::_100percent,
                    "Min curators percent must be between 0% and 100% (0-10000).");
            eosio::check(min_curators_prcnt <= max_curators_prcnt,
                    "Min curators percent must be less than max curators percent or equal.");
            eosio::check(max_curators_prcnt <= config::_100percent, "Max curators percent must be less than 100 or equal.");
        }
        void validate_value(uint16_t x) const {
            eosio::check(x >= min_curators_prcnt, "Curators percent is less than min curators percent.");
            eosio::check(x <= max_curators_prcnt, "Curators percent is greater than max curators percent.");
        }
    };
    using curators_prcnt_prm = param_wrapper<st_curators_prcnt, 2>;

    struct st_bwprovider_t: parameter {
        name actor;
        name permission;

        void validate() const override {
            eosio::check((actor != name()) == (permission != name()), "actor and permission must be set together");
            // check that contract can use cyber:providebw done in setparams
            // (we need know contract account to make this check)
        }
    };
    using st_bwprovider = param_wrapper<st_bwprovider_t,2>;

    struct st_min_abs_rshares : parameter {
        uint64_t value;
    };
    using st_min_abs_rshares = param_wrapper<st_min_abs_rshares_t, 1>;

    #define posting_params std::variant<st_max_vote_changes, st_cashout_window, st_max_beneficiaries, \
          st_max_comment_depth, st_social_acc, st_referral_acc, st_curators_prcnt, st_bwprovider, \
          st_min_abs_rshares>

    struct posting_state {
        st_max_vote_changes max_vote_changes_param;
        st_cashout_window cashout_window_param;
        st_max_beneficiaries max_beneficiaries_param;
        st_max_comment_depth max_comment_depth_param;
        st_social_acc social_acc_param;
        st_referral_acc referral_acc_param;
        st_curators_prcnt curators_prcnt_param;
        st_bwprovider bwprovider_param;
        st_min_abs_rshares min_abs_rshares_param;

        static constexpr int params_count = 9;
    };
    using posting_params_singleton = eosio::singleton<"pstngparams"_n, posting_state>;

} //golos
