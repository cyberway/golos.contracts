/*
 * Pure unit-tests, not relying on rewards simulation model
 */

#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.charge_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "../golos.publication/types.h"
#include "../golos.publication/config.hpp"
#include "contracts.hpp"

namespace cfg = golos::config;
using namespace fixed_point_utils;
using namespace eosio::testing;

#define PRECISION 3
#define TOKEN_NAME "GOLOS"
constexpr auto MAX_FIXP = std::numeric_limits<fixp_t>::max();

constexpr uint16_t max_token_percent = 5000;
constexpr auto votes_restore_per_day = 40;
constexpr auto days_to_restore_votes = 5;
constexpr auto hour_seconds = 60*60;
constexpr auto day_seconds = hour_seconds * 24;
constexpr auto week_seconds = day_seconds * 7;

constexpr auto posts_window = 5*60, posts_per_window = 1;
constexpr auto comments_window = 200, comments_per_window = 10;

class posting_tester : public golos_tester {
protected:
    symbol _token_symbol;
    golos_posting_api post;
    golos_vesting_api vest;
    golos_charge_api charge;
    cyber_token_api token;

    account_name _forum_name;
    account_name _issuer;
    vector<account_name> _users;

    struct errors: contract_error_messages {
        const string too_low_power = amsg("too low voting power");
        const string too_low_weight = amsg("too low vote weight");
    } err;

public:
    posting_tester()
        : golos_tester(cfg::publish_name)
        , _token_symbol(PRECISION, TOKEN_NAME)
        , post({this, _code, _token_symbol})
        , vest({this, cfg::vesting_name, _token_symbol})
        , charge({this, cfg::charge_name, _token_symbol})
        , token({this, cfg::token_name, _token_symbol})

        , _issuer(N(issuer))
        , _users{N(alice), N(bob), N(carol), N(user), N(u1), N(u2), N(u3), N(u4), N(u5)} {

        create_accounts({_code, _issuer, cfg::token_name, cfg::vesting_name, cfg::control_name, cfg::charge_name});
        create_accounts(_users);
        create_accounts({N(stranger)});
        produce_block();
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        vest.add_changevest_auth_to_issuer(_issuer, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        charge.initialize_contract();
        post.initialize_contract(cfg::token_name, cfg::charge_name);

        set_authority(_issuer, cfg::invoice_name, create_code_authority({charge._code, post._code}), "active");
        link_authority(_issuer, charge._code, cfg::invoice_name, N(use));
        link_authority(_issuer, charge._code, cfg::invoice_name, N(usenotifygt));
        link_authority(_issuer, vest._code, cfg::invoice_name, N(retire));
    }

    void init(double total_funds) {
        auto funds = token.make_asset(total_funds);
        BOOST_CHECK_EQUAL(success(), post.init_default_params());
        BOOST_CHECK_EQUAL(success(), token.create(_issuer, funds));
        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, _issuer, funds));
        BOOST_CHECK_EQUAL(success(), token.open(_code));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer));
        BOOST_CHECK_EQUAL(success(), vest.open(_code));
    }

    void buy_vesting_for_users(std::vector<account_name> users, int64_t funds) {
        asset amount{funds, _token_symbol};
        for (const auto& u : users) {
            if (vest.get_balance_raw(u).is_null()) {
                BOOST_CHECK_EQUAL(success(), vest.open(u));
            }
            BOOST_CHECK_EQUAL(success(), token.transfer(_issuer, cfg::vesting_name, amount, "send to: " + string(u)));
        }
    }

    void set_rules(
        const funcparams& mainfn,
        const funcparams& curationfn,
        const funcparams& auctionfn,
        limitsarg lims
    ) {
        static const std::vector<std::string> act_names{"post", "comment", "vote", "post bandwidth"};
        static const int64_t max_arg = static_cast<int64_t>(MAX_FIXP);

        BOOST_CHECK_EQUAL(act_names.size(), lims.limitedacts.size());
        for (size_t i = 0; i < act_names.size(); i++) {
            auto act = lims.limitedacts[i];
            BOOST_CHECK_EQUAL(success(), charge.set_restorer(_issuer, act.chargenum,
                lims.restorers[act.restorernum], max_arg, max_arg, max_arg));
            BOOST_CHECK_EQUAL(success(), post.set_limit(
                act_names[i],
                act.chargenum,
                act.chargeprice,
                act.cutoffval,
                lims.vestingprices.size() > i ? lims.vestingprices[i] : 0,
                lims.minvestings.size() > i ? lims.minvestings[i] : 0
            ));
        }
        BOOST_CHECK_EQUAL(success(), post.set_rules(mainfn, curationfn, auctionfn, max_token_percent));
    }

    enum class fn_preset {
        base,
        linear,
        bounded
    };
    enum class lim_preset {
        base,
        real
    };

    void set_rules_with_preset(fn_preset fn, lim_preset lim = lim_preset::base) {
        const limitsarg lim_base = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {}, {}};
        auto cutoff = cfg::_100percent;
        auto muldiv_fn = [](std::string t, auto m, auto d) { return t + "*" + m + "/" + std::to_string(d); }; // "t*m/d"
        const limitsarg lim_real = {
            {
                muldiv_fn("t", std::to_string(cutoff), posts_window),
                muldiv_fn("t", std::to_string(cutoff), comments_window),
                muldiv_fn("t", std::to_string(cutoff), day_seconds * days_to_restore_votes),
                muldiv_fn("t", "p",                    day_seconds)
            }, {
                {1, 0, cutoff, cutoff / posts_per_window},
                {2, 1, cutoff, cutoff / comments_per_window},
                {0, 2, cutoff, cutoff / (votes_restore_per_day * days_to_restore_votes)},
                {3, 3, cutoff*4, cutoff}
            },
            {},
            {}
        };
        const funcparams fn_linear[3] = {
            {"x", MAX_FIXP},
            {"x", MAX_FIXP},
            {"1", FP(1 << fixed_point_fractional_digits)},
        };
        BOOST_CHECK(fn == fn_preset::linear); // Only linear function preset supported for now
        BOOST_CHECK(lim == lim_preset::base || lim == lim_preset::real); // Unsupported limits preset
        const auto& fns = fn_linear;
        set_rules(fns[0], fns[1], fns[2], lim == lim_preset::base ? lim_base : lim_real);
    }
};

BOOST_AUTO_TEST_SUITE(posting_unit_tests)

BOOST_FIXTURE_TEST_CASE(golos_rshares_test, posting_tester) try {
    BOOST_TEST_MESSAGE("Test raw rshares and vote weights when use Golos-setup");
    BOOST_TEST_MESSAGE("--- create test accounts and set predefined vesting values");
    _users.resize(3);
    const int64_t total_vests = 31061610388087ll;         // raw GESTS values obtained from sample Golos acc
    const int64_t delegated = 30713649230251ll;
    const int64_t effective = total_vests - delegated;
    auto vests = effective;
    init((vests / 1000 + 1) * _users.size());
    produce_block();

    auto poster = _users[0];
    auto voter = _users[1];
    auto hater = _users[2];
    buy_vesting_for_users(_users, vests);
    produce_block();

    BOOST_CHECK_EQUAL(vest.get_balance_raw(voter)["vesting"]["_amount"], effective);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(hater)["vesting"]["_amount"], effective);

    BOOST_TEST_MESSAGE("--- setrules");
    set_rules_with_preset(fn_preset::linear, lim_preset::real);
    BOOST_CHECK_EQUAL(success(), post.set_params("["+post.get_str_cashout_window(12*3600, post.upvote_lockout)+"]"));
    produce_block();

    auto votes_capacity = days_to_restore_votes * votes_restore_per_day;    // 200 by default
    auto factor = cfg::_100percent / votes_capacity;                        // 50 by default
    int n_comments = votes_capacity / factor;   // number of votes required to reduce factor
    n_comments++;                       // add 1: votes have delays for 1 block, so some fraction of battery restored
    BOOST_TEST_MESSAGE("--- create_message and " + std::to_string(n_comments+1) + " comments");
    mssgid post_id{poster, "post"};
    BOOST_CHECK_EQUAL(success(), post.create_msg(post_id));
    for (int i = 1; i <= n_comments + 1; i++) {
        BOOST_CHECK_EQUAL(success(), post.create_msg({poster, "comment" + std::to_string(i)}, post_id));
    }
    produce_block();

    BOOST_TEST_MESSAGE("--- vote by stranger with no balance");
    buy_vesting_for_users({N(stranger)}, 10);
    BOOST_CHECK_EQUAL(err.too_low_weight, post.upvote(N(stranger), post_id));
    produce_block();

    BOOST_TEST_MESSAGE("--- vote with 100%");
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, post_id));
    produce_block();
    BOOST_TEST_MESSAGE("--- flag with 100%");
    BOOST_CHECK_EQUAL(success(), post.downvote(hater, post_id));
    produce_block();

    auto make_rshares = [](auto rshares, auto weight) {
        return mvo("rshares", rshares)("curatorsw", weight);
    };

    int64_t expect = effective * factor / cfg::_100percent; // full weight vote
    BOOST_CHECK_EQUAL(1739'805'789, expect);            // ensure it's same as tested manually (remove if change params)
    CHECK_MATCHING_OBJECT(post.get_vote(poster, 0), make_rshares(expect, expect));
    CHECK_MATCHING_OBJECT(post.get_vote(poster, 1), make_rshares(-expect, 0));

    BOOST_TEST_MESSAGE("--- vote for comments and check reduced charge at the last vote");
    for (int i = 1; i < n_comments; i++) {
        BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "comment" + std::to_string(i)}));
        produce_block();
        CHECK_MATCHING_OBJECT(post.get_vote(poster, i+1), make_rshares(expect, expect));
    }
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "comment" + std::to_string(n_comments)}));
    int64_t expect2 = effective * (factor-1) / cfg::_100percent;     // four votes reduce weight factor to 49
    BOOST_CHECK_EQUAL(1705'009'673, expect2);
    CHECK_MATCHING_OBJECT(post.get_vote(poster, n_comments+1), make_rshares(expect2, expect2));

    BOOST_TEST_MESSAGE("--- wait to restore battery to get full-weight vote again");
    // last vote got reduced weight factor 49, one vote before got full weight 50
    auto units = factor + factor - 1;
    auto seconds = (day_seconds * days_to_restore_votes) * units / cfg::_100percent;
    auto blocks = golos::seconds_to_blocks(seconds);
    blocks -= 2 + (n_comments - 1); // votes casted with delay (1st 2 blocks, other 1 block), so reduce blocks count
    blocks++;                       // add one more block to compensate possible rounding
    produce_blocks(blocks);
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "comment" + std::to_string(n_comments+1)}));
    produce_block();
    CHECK_MATCHING_OBJECT(post.get_vote(poster, n_comments+2), make_rshares(expect, expect));

    BOOST_TEST_MESSAGE("--- creating comments to full discharge battery");
    int n_full_comments = (votes_capacity / factor) * (votes_capacity-1);
    n_full_comments += 20; // regeneration by blocks
    n_full_comments++; // and 1 comment for vote after 1% charge
    for (int i = 1; i <= n_full_comments; i++) {
        BOOST_CHECK_EQUAL(success(), post.create_msg({poster, "fullcomment" + std::to_string(i)}, post_id));
        produce_blocks(golos::seconds_to_blocks(comments_window / comments_per_window) + 1);
    }
    produce_block();

    BOOST_TEST_MESSAGE("--- vote for comments and testing discharge check");
    int i = 1;
    for (; i <= n_full_comments - 1; i++) {
        BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "fullcomment" + std::to_string(i)}));
        produce_block();
    }

    BOOST_CHECK_EQUAL(err.too_low_power, post.upvote(voter, {poster, "fullcomment" + std::to_string(i)}));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(overcharge_test, posting_tester) try {
    BOOST_TEST_MESSAGE("Test battery overcharge");
    BOOST_TEST_MESSAGE("--- create test accounts, add vesting and set rules");
    _users.resize(2);
    auto vests = 100000;
    init((vests / 1000 + 1) * _users.size());
    produce_block();

    auto poster = _users[0];
    auto voter = _users[1];
    buy_vesting_for_users(_users, vests);
    produce_block();

    set_rules_with_preset(fn_preset::linear, lim_preset::real);
    BOOST_CHECK_EQUAL(success(), post.set_params("["+post.get_str_cashout_window(12*3600, post.upvote_lockout)+"]"));
    produce_block();

    BOOST_CHECK(charge.get_balance(voter, 0).is_null());

    int n_comments = 2;
    BOOST_TEST_MESSAGE("--- create message and " + std::to_string(n_comments) + " comments");
    mssgid post_id{poster, "post"};
    BOOST_CHECK_EQUAL(success(), post.create_msg(post_id));
    for (int i = 1; i <= n_comments; i++) {
        BOOST_CHECK_EQUAL(success(), post.create_msg({poster, "comment" + std::to_string(i)}, post_id));
    }
    produce_block();

    BOOST_TEST_MESSAGE("--- vote with 100%");
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, post_id));
    produce_block();
    BOOST_CHECK(!charge.get_balance(voter, 0).is_null());
    BOOST_TEST_MESSAGE("ch: " + fc::json::to_string(charge.get_balance(voter, 0)));

    BOOST_TEST_MESSAGE("--- wait for overcharge");
    produce_blocks(golos::seconds_to_blocks(day_seconds * days_to_restore_votes / cfg::_100percent * (50+50+10)));

    BOOST_TEST_MESSAGE("--- vote overcharged, it will delete battery due 100% charge after vote");
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "comment1"}));
    BOOST_CHECK(charge.get_balance(voter, 0).is_null());
    BOOST_TEST_MESSAGE("ch: " + fc::json::to_string(charge.get_balance(voter, 0)));
    BOOST_TEST_MESSAGE("--- vote again, it will re-create battery");
    BOOST_CHECK_EQUAL(success(), post.upvote(voter, {poster, "comment2"}));
    BOOST_CHECK(!charge.get_balance(voter, 0).is_null());
    BOOST_TEST_MESSAGE("ch: " + fc::json::to_string(charge.get_balance(voter, 0)));

    produce_blocks(50); // go over lib

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
