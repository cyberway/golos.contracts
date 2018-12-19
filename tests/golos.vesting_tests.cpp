#include "golos_tester.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.charge_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include "contracts.hpp"
#include "../golos.vesting/config.hpp"

namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

static const auto _token_name = "GOLOS";
static const auto _token_precision = 3;
static const auto _vesting_precision = 6;
static const auto _token_sym = symbol(_token_precision, _token_name);
static const auto _vesting_sym = symbol(_vesting_precision, _token_name);
static const auto default_vesting_amount = 100;

class golos_vesting_tester : public golos_tester {
protected:
    golos_vesting_api vest;
    eosio_token_api token;
    golos_charge_api charge;
public:

    golos_vesting_tester()
        : golos_tester(cfg::vesting_name)
        , vest({this, cfg::vesting_name, _vesting_sym})
        , token({this, cfg::token_name, _token_sym})
        , charge({this, cfg::charge_name, _token_sym})
        
    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), N(notify.acc),
            _code, cfg::token_name, cfg::control_name, cfg::emission_name, cfg::publish_name, cfg::charge_name});
        produce_blocks(2);

        install_contract(_code, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::charge_name, contracts::charge_wasm(), contracts::charge_abi());
    }

    void prepare_balances(int supply = 1e5, int issue1 = 500, int issue2 = 500, int buy1 = default_vesting_amount, int buy2 = default_vesting_amount) {
        BOOST_CHECK_EQUAL(success(), token.create(cfg::emission_name, token.make_asset(supply), {cfg::charge_name, cfg::publish_name}));
        BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(sania), token.make_asset(issue1), "issue tokens sania"));
        BOOST_CHECK_EQUAL(success(), token.issue(cfg::emission_name, N(pasha), token.make_asset(issue2), "issue tokens pasha"));

        BOOST_CHECK_EQUAL(success(), vest.open(N(sania)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(pasha)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(tania)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(vania)));
        produce_block();

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(cfg::emission_name));
        BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, token.make_asset(buy1), "buy vesting"));
        BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, token.make_asset(buy2), "buy vesting"));
        produce_block();
    }

    void init_params() {
        auto vesting_withdraw = vest.vesting_withdraw(withdraw_intervals, withdraw_interval_seconds);
        auto vesting_amount = vest.vesting_amount(vesting_min_amount);
        auto delegation = vest.delegation(delegation_min_amount, delegation_min_remainder, delegation_min_time, delegation_max_interest, delegation_return_time);

        auto params = "[" + vesting_withdraw + "," + vesting_amount + "," + delegation + "]";
        BOOST_CHECK_EQUAL(success(), vest.set_params(cfg::emission_name, _vesting_sym, params));
    }

protected:

    // TODO: make contract error messages more clear
    struct errors: contract_error_messages {
        const string key_not_found    = amsg("unable to find key");
        const string not_issuer       = amsg("Only token issuer can create it");
        const string self_delegate    = amsg("You can not delegate to yourself");
        const string bad_strategy     = amsg("not valid value payout_strategy");
        const string tokens_lt0       = amsg("the number of tokens should not be less than 0");
        const string interest_to_high = amsg("Exceeded the percentage of delegated vesting");
        const string tokens_frozen    = amsg("Tokens are frozen until the end of the period");
        const string delegation_no_funds      = amsg("Insufficient funds for delegation");
        const string delegation_no_funds2     = amsg("insufficient funds for delegation");      // TODO: fix…
        const string undelegation_no_funds    = amsg("Insufficient funds for undelegation");
        const string not_enough_delegation    = amsg("There are not enough delegated tools for output");
        const string delegated_withdraw       = amsg("delegated vesting withdrawn");
        const string unknown_asset    = amsg("unknown asset");
        const string wrong_precision  = amsg("wrong asset precision");
        const string vesting_params   = amsg("not found vesting params");
        const string issuer_not_autority = "missing authority of issuer";

        const string withdraw_intervals        = amsg("intervals <= 0");
        const string withdraw_interval_seconds = amsg("interval_seconds <= 0");
        const string delegation_min_amount     = amsg("delegation min_amount <= 0");
        const string delegation_min_remainder  = amsg("delegation min_remainder <= 0");
        const string delegation_max_interest   = amsg("delegation max_interest > 10000");
        const string delegation_return_time    = amsg("delegation return_time <= 0");

        const string cutoff = amsg("can't delegate, not enough power");
    } err;

    const uint32_t withdraw_intervals = 13;
    const uint32_t withdraw_interval_seconds = 120;
    const uint64_t vesting_min_amount = 10*1e3;
    const uint64_t delegation_min_amount = 5e6;
    const uint64_t delegation_min_remainder = 15e6;
    const uint32_t delegation_min_time = 0;
    const uint16_t delegation_max_interest = 0;
    const uint32_t delegation_return_time = 120;
};

BOOST_AUTO_TEST_SUITE(golos_vesting_tests)

BOOST_FIXTURE_TEST_CASE(token_tests, golos_vesting_tester) try {
    // TODO: actually tests token, remove?
    BOOST_TEST_MESSAGE("Test creating and issue token");
    BOOST_TEST_MESSAGE("--- create token");
    auto issuer = cfg::emission_name;
    auto stats = mvo()
        ("supply", token.asset_str(0))
        ("max_supply", token.asset_str(100000))
        ("issuer", name(issuer).to_string());
    BOOST_CHECK_EQUAL(success(), token.create(issuer, token.make_asset(100000)));
    CHECK_MATCHING_OBJECT(token.get_stats(), stats);

    BOOST_TEST_MESSAGE("--- issue token");
    BOOST_CHECK_EQUAL(success(), token.issue(issuer, N(sania), token.make_asset(200), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(issuer, N(pasha), token.make_asset(300), "issue tokens pasha"));
    produce_block();

    CHECK_MATCHING_OBJECT(token.get_stats(), mvo(stats)("supply", token.asset_str(500)));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(200)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", token.asset_str(300)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_params, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test vesting parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    produce_block();

    auto issuer = cfg::emission_name;
    BOOST_TEST_MESSAGE("--- create token GOLOS");
    BOOST_CHECK_EQUAL(success(), token.create(issuer, token.make_asset(100000)));

    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(vest.get_params(_token_sym).is_null());

    init_params();

    auto obj_params = vest.get_params(_token_sym);
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(obj_params));

    BOOST_TEST_CHECK(obj_params["vesting_withdraw_params"]["intervals"] == withdraw_intervals);
    BOOST_TEST_CHECK(obj_params["vesting_withdraw_params"]["interval_seconds"] == withdraw_interval_seconds);
    BOOST_TEST_CHECK(obj_params["amount_params"]["min_amount"] == vesting_min_amount);

    BOOST_TEST_CHECK(obj_params["delegation_params"]["min_amount"] == delegation_min_amount);
    BOOST_TEST_CHECK(obj_params["delegation_params"]["min_remainder"] == delegation_min_remainder);
    BOOST_TEST_CHECK(obj_params["delegation_params"]["min_time"] == delegation_min_time);
    BOOST_TEST_CHECK(obj_params["delegation_params"]["max_interest"] == delegation_max_interest);
    BOOST_TEST_CHECK(obj_params["delegation_params"]["return_time"] == delegation_return_time);

    auto params = "[" + vest.vesting_withdraw(0, withdraw_interval_seconds) + "]";
    BOOST_CHECK_EQUAL(err.withdraw_intervals, vest.set_params(issuer, _vesting_sym, params));

    params = "[" + vest.vesting_withdraw(withdraw_intervals, 0) + "]";
    BOOST_CHECK_EQUAL(err.withdraw_interval_seconds, vest.set_params(issuer, _vesting_sym, params));

    params = "[" + vest.delegation(0, delegation_min_remainder, delegation_min_time, delegation_max_interest, delegation_return_time) + "]";
    BOOST_CHECK_EQUAL(err.delegation_min_amount, vest.set_params(issuer, _vesting_sym, params));

    params = "[" + vest.delegation(delegation_min_amount, 0, delegation_min_time, delegation_max_interest, delegation_return_time) + "]";
    BOOST_CHECK_EQUAL(err.delegation_min_remainder, vest.set_params(issuer, _vesting_sym, params));

    params = "[" + vest.delegation(delegation_min_amount, delegation_min_remainder, delegation_min_time, 10001, delegation_return_time) + "]";
    BOOST_CHECK_EQUAL(err.delegation_max_interest, vest.set_params(issuer, _vesting_sym, params));

    params = "[" + vest.delegation(delegation_min_amount, delegation_min_remainder, delegation_min_time, delegation_max_interest, 0) + "]";
    BOOST_CHECK_EQUAL(err.delegation_return_time, vest.set_params(issuer, _vesting_sym, params));


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test creating vesting");
    auto issuer = cfg::emission_name;
    BOOST_TEST_MESSAGE("--- fail on non-existing token");
    BOOST_CHECK_EQUAL(err.key_not_found, vest.create_vesting(issuer));

    BOOST_TEST_MESSAGE("--- fails when not issuer");
    BOOST_CHECK_EQUAL(success(), token.create(issuer, token.make_asset(100000)));
    BOOST_CHECK_EQUAL(err.issuer_not_autority, vest.create_vesting(N(sania)));
    // TODO: test issuers list

    BOOST_TEST_MESSAGE("--- succeed in normal conditions");
    BOOST_CHECK_EQUAL(success(), vest.create_vesting(issuer));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(buy_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test buying vesting / converting token to vesting");
    auto issue = 1000;
    auto buy1 = 250;
    auto buy2 = 100;
    prepare_balances(100500, issue, issue, buy1, buy2);

    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(issue - buy1)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", token.asset_str(issue - buy2)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(buy1));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(buy2));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(convert, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test converting vesting to token");

    prepare_balances();
    init_params();

    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));

    BOOST_TEST_MESSAGE("--- check that no convert object exists before start convering");
    BOOST_TEST_CHECK(vest.get_convert_obj(N(sania)).is_null());

    BOOST_TEST_MESSAGE("--- check that bad asset or precision fails to convert");
    auto start_convert = [&](auto amount, auto precision, auto name) {
        return vest.convert_vesting(N(sania), N(sania), asset(amount, symbol(precision, name)));
    };
    BOOST_CHECK_EQUAL(err.vesting_params, start_convert(1e6, _vesting_precision, "GLS"));
    BOOST_CHECK_EQUAL(err.wrong_precision, start_convert(1e5, _vesting_precision-1, _token_name));
    BOOST_CHECK_EQUAL(err.wrong_precision, start_convert(1e7, _vesting_precision+1, _token_name));

    BOOST_TEST_MESSAGE("--- succeed if start converting with correct asset");

    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(withdraw_intervals)));

    const auto steps = withdraw_intervals;
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], steps);

    const int64_t _blocks_in_interval = golos::seconds_to_blocks(withdraw_interval_seconds);
    BOOST_TEST_MESSAGE("--- skip " + std::to_string(_blocks_in_interval) + " blocks and check same balance");
    produce_blocks(_blocks_in_interval);
    // note: deferred tx will be executed at current block, but after normal txs,
    // so it's result will be available only in next block
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], steps);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(400)));

    BOOST_TEST_MESSAGE("--- go next block and check that withdrawal happened");
    produce_block();
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], steps - 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(99));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(401)));

    BOOST_TEST_MESSAGE("--- skip " + std::to_string(_blocks_in_interval*(steps-1)-1) + " blocks and check balance");
    produce_blocks(_blocks_in_interval * (steps - 1) - 1);  // 1 withdrawal already passed, so -1
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100 - steps + 1));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(400 + steps - 1)));

    BOOST_TEST_MESSAGE("--- go next block and check that fully withdrawn");
    produce_block();
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100 - steps));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(400 + steps)));
    // produce_block();    // TODO: object must destroy inself on same block when if finishing withdraw
    // BOOST_TEST_CHECK(vest.get_convert_obj(N(sania)).is_null());
    // TODO: check fail if not enough funds and other balance issues
    // TODO: check amount that has remainder after dividing to intervals
    // TODO: check change convert, incl. 0
    // TODO: check withdraw to different account
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(cancel_convert, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test canceling convert vesting to token");

    prepare_balances();
    init_params();

    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));

    BOOST_TEST_CHECK(vest.get_convert_obj(N(sania)).is_null());
    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(withdraw_intervals)));
    auto steps = withdraw_intervals;
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], steps);

    BOOST_TEST_MESSAGE("--- check 1st withdrawal");
    const int64_t _blocks_in_interval = golos::seconds_to_blocks(withdraw_interval_seconds);

    produce_blocks(_blocks_in_interval + 1);
    BOOST_CHECK_EQUAL(vest.get_convert_obj(N(sania))["number_of_payments"], steps - 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(99));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(401)));

    BOOST_TEST_MESSAGE("--- cancel convert and check convert object deleted");
    BOOST_CHECK_EQUAL(success(), vest.cancel_convert_vesting(N(sania), vest.make_asset(0)));
    BOOST_TEST_CHECK(vest.get_convert_obj(N(sania)).is_null());

    BOOST_TEST_MESSAGE("--- go to next withdrawal time and check it not happens");
    produce_blocks(_blocks_in_interval);
    BOOST_TEST_CHECK(vest.get_convert_obj(N(sania)).is_null());
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(99));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(401)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delegate_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test delegating vesting shares");

    prepare_balances();
    init_params();
    const int divider = vest.make_asset(1).get_amount();
    const auto min_fract = 1. / divider;
    const int min_remainder = delegation_min_remainder / divider;
    const auto amount = vest.make_asset(min_remainder);
    const auto charge_prop = 0.7;

    BOOST_TEST_MESSAGE("--- fail when delegate to self");
    BOOST_CHECK_EQUAL(err.self_delegate, vest.delegate_vesting(N(sania), N(sania), amount));

    BOOST_TEST_MESSAGE("--- fail on bad payout_strategy or interest rate");
    BOOST_CHECK_EQUAL(err.bad_strategy, vest.delegate_vesting(N(sania), N(pasha), amount, 0, -1));
    BOOST_CHECK_EQUAL(err.bad_strategy, vest.delegate_vesting(N(sania), N(pasha), amount, 0, 2));
    BOOST_CHECK_EQUAL(err.interest_to_high, vest.delegate_vesting(N(sania), N(pasha), amount, 50*cfg::_1percent)); // TODO: test boundaries

    BOOST_TEST_MESSAGE("--- fail when delegate 0 or less than min_remainder");
    BOOST_CHECK_EQUAL(err.tokens_lt0, vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(0)));
    BOOST_CHECK_EQUAL(err.delegation_no_funds, vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(min_fract)));
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(min_remainder - min_fract)));   //TODO: error must be same

    BOOST_TEST_MESSAGE("--- insufficient funds assert");
    BOOST_CHECK_EQUAL(err.delegation_no_funds2, vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(10000)));

    BOOST_TEST_MESSAGE("--- succeed when correct arguments for both payout strategies");
    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(pasha), amount, 0, 0));  // amount = min_remainder
    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(tania), amount, 0, 1));  // TODO: disallow 0% + strategy

    BOOST_TEST_MESSAGE("--- fail when delegate more than scheduled to withdraw");
    BOOST_CHECK_EQUAL(success(), vest.convert_vesting(N(sania), N(sania), vest.make_asset(100-3*min_remainder)));
    BOOST_CHECK_EQUAL(err.delegation_no_funds2, vest.delegate_vesting(N(sania), N(vania), vest.make_asset(min_remainder+1)));
    BOOST_TEST_MESSAGE("--- succeed when withdraval counted");
    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(vania), amount));
    
    BOOST_TEST_MESSAGE("--- charge limit test");
    BOOST_CHECK_EQUAL(success(), charge.use(cfg::emission_name, N(pasha), 0, charge_prop * cfg::_100percent, cfg::_100percent));
    BOOST_CHECK_EQUAL(err.cutoff, vest.delegate_vesting(N(pasha), N(vania), vest.make_asset(default_vesting_amount * (1.01 - charge_prop))));
    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(pasha), N(vania), vest.make_asset(default_vesting_amount * (1.0 - charge_prop))));

//    // BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(issuer), amount));    // TODO: fix: sending to issuer instead of vania fails
//    // TODO: check delegation to account without balance
//    // TODO: check change delegation
//    // TODO: check min step
//    // TODO: check min remainder
//    // TODO: check delegation object
//    // TODO: check battery limitation
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(undelegate_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test un-delegating vesting shares");

    prepare_balances();
    init_params();

    const int divider = vest.make_asset(1).get_amount();
    const int min_step = delegation_min_amount / divider;
    const int min_remainder = delegation_min_remainder / divider;
    const int amount = min_step + min_remainder;

    BOOST_CHECK_EQUAL(success(), vest.delegate_vesting(N(sania), N(pasha), vest.make_asset(amount)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount));
    produce_block();

    if (delegation_min_time > 0) {
        // TODO: test it when config allowed to be changed with `ctrl`
        BOOST_TEST_MESSAGE("--- fail when undelegate earlier than min_time");
        BOOST_CHECK_EQUAL(err.tokens_frozen, vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(min_step)));
        produce_blocks(golos::seconds_to_blocks(delegation_min_time));
    }

    BOOST_TEST_MESSAGE("--- fail when undelegate more than delegated");
    BOOST_CHECK_EQUAL(err.not_enough_delegation, vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(amount + 1))); // TODO: boundary

    BOOST_TEST_MESSAGE("--- fail when step is less than min_amount");
    BOOST_CHECK_EQUAL(err.undelegation_no_funds, vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(min_step - 1))); // TODO: boundary

    BOOST_TEST_MESSAGE("--- fail when remainder less than min_remainder");
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(min_step + 1))); // TODO: boundaries
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(amount - 1)));

    BOOST_TEST_MESSAGE("--- succed in normal conditions");
    BOOST_CHECK_EQUAL(success(), vest.undelegate_vesting(N(sania), N(pasha), vest.make_asset(min_step)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount - min_step));

    BOOST_TEST_MESSAGE("--- wait delegation return block and check that balance is the same");
    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));
    produce_blocks(golos::seconds_to_blocks(delegation_return_time - delegation_min_time));      // Note: it's return + 1 block here, check, why delayed
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount - min_step));

    BOOST_TEST_MESSAGE("--- go next block and check return");
    produce_block();
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount - min_step));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount - min_step));
    // TODO: check delegation and return objects
    // TODO: check step, remainder
    // TODO: check min time, return time
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
