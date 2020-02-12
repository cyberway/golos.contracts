#pragma once
#include <eosio/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>
#include <common/config.hpp>

namespace golos {

    using namespace eosio;

    struct st_max_vote_changes_t : parameter {
        uint8_t value;
    };
    using st_max_vote_changes = param_wrapper<st_max_vote_changes_t, 1>;

    struct st_cashout_window_t : parameter {
        uint32_t window;
        uint32_t upvote_lockout;

        void validate() const override {
            eosio::check(window > 0, "Cashout window must be greater than 0.");
            eosio::check(window > upvote_lockout, "Cashout window can't be less than upvote lockout.");
        }
    };
    using st_cashout_window = param_wrapper<st_cashout_window_t, 2>;

    struct st_max_beneficiaries_t : parameter {
        uint8_t value;
    };
    using st_max_beneficiaries = param_wrapper<st_max_beneficiaries_t, 1>;

    struct st_max_comment_depth_t : parameter {
        uint16_t value;

        void validate() const override {
            eosio::check(value > 0, "Max comment depth must be greater than 0.");
        }
    };
    using st_max_comment_depth = param_wrapper<st_max_comment_depth_t, 1>;

    struct st_social_acc_t : parameter {
        name value;

        void validate() const override {
            if (value != name()) {
                eosio::check(is_account(value), "Social account doesn't exist.");
            }
        }
    };
    using st_social_acc = param_wrapper<st_social_acc_t, 1>;

    struct st_referral_acc_t : parameter {
        name value;

        void validate() const override {
            if (value != name()) {
                eosio::check(is_account(value), "Referral account doesn't exist.");
            }
        }
    };
    using st_referral_acc = param_wrapper<st_referral_acc_t, 1>;

    struct st_curators_prcnt_t : parameter {
        percent_t min_curators_prcnt;
        percent_t max_curators_prcnt;

        void validate() const override {
            eosio::check(min_curators_prcnt <= config::_100percent,
                    "Min curators percent must be between 0% and 100% (0-10000).");
            eosio::check(min_curators_prcnt <= max_curators_prcnt,
                    "Min curators percent must be less than max curators percent or equal.");
            eosio::check(max_curators_prcnt <= config::_100percent, "Max curators percent must be less than 100 or equal.");
        }
        void validate_value(percent_t x) const {
            eosio::check(x >= min_curators_prcnt, "Curators percent is less than min curators percent.");
            eosio::check(x <= max_curators_prcnt, "Curators percent is greater than max curators percent.");
        }
    };
    using st_curators_prcnt = param_wrapper<st_curators_prcnt_t, 2>;

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

    struct st_min_abs_rshares_t : parameter {
        uint64_t value;
    };
    using st_min_abs_rshares = param_wrapper<st_min_abs_rshares_t, 1>;

    #define posting_params std::variant<st_max_vote_changes, st_cashout_window, st_max_beneficiaries, \
          st_max_comment_depth, st_social_acc, st_referral_acc, st_curators_prcnt, st_bwprovider, \
          st_min_abs_rshares>

    struct posting_state {
        st_max_vote_changes max_vote_changes;
        st_cashout_window cashout_window;
        st_max_beneficiaries max_beneficiaries;
        st_max_comment_depth max_comment_depth;
        st_social_acc social_acc;
        st_referral_acc referral_acc;
        st_curators_prcnt curators_prcnt;
        st_bwprovider bwprovider;
        st_min_abs_rshares min_abs_rshares;

        static constexpr int params_count = 9;
    };
    using posting_params_singleton [[using eosio: order("id","asc"), contract("golos.publication")]] = eosio::singleton<"pstngparams"_n, posting_state>;

} //golos
