#include "golos_tester.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.charge_test_api.hpp"
#include "cyber.token_test_api.hpp"
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
static const auto _vesting_sym_e = symbol(_vesting_precision + 1, _token_name);
static const auto default_vesting_amount = 100;

class golos_vesting_tester : public golos_tester {
protected:
    golos_vesting_api vest;
    cyber_token_api token;
    golos_charge_api charge;
public:

    golos_vesting_tester()
        : golos_tester(cfg::vesting_name)
        , vest({this, _code, _vesting_sym})
        , token({this, cfg::token_name, _token_sym})
        , charge({this, cfg::charge_name, _token_sym})

    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), N(notify.acc),
            _code, cfg::token_name, cfg::control_name, cfg::emission_name, cfg::publish_name, cfg::charge_name});
        produce_blocks(2);
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(config::msig_account_name, contracts::msig_wasm(), contracts::msig_abi());
        vest.add_changevest_auth_to_issuer(_issuer, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        charge.initialize_contract();
    }

    void prepare_balances(int supply = 1e5, int issue1 = 500, int issue2 = 500, int buy1 = default_vesting_amount, int buy2 = default_vesting_amount) {
        BOOST_CHECK_EQUAL(success(), token.create(_issuer, token.make_asset(supply)));
        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, N(sania), token.make_asset(issue1), "issue tokens sania"));
        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, N(pasha), token.make_asset(issue2), "issue tokens pasha"));
        produce_block();

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer));
        BOOST_CHECK_EQUAL(success(), vest.open(N(sania)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(pasha)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(tania)));
        BOOST_CHECK_EQUAL(success(), vest.open(N(vania)));
        produce_block();

        BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, token.make_asset(buy1), "buy vesting"));
        BOOST_CHECK_EQUAL(success(), token.transfer(N(pasha), cfg::vesting_name, token.make_asset(buy2), "buy vesting"));
        produce_block();
    }

    void init_params(int withdraw_seconds = -1) {
        if (withdraw_seconds < 0)
            withdraw_seconds = withdraw_interval_seconds;
        auto vesting_withdraw = vest.withdraw_param(withdraw_intervals, withdraw_seconds);
        auto vesting_amount = vest.amount_param(vesting_min_amount);
        auto delegation = vest.delegation_param(delegation_min_amount, delegation_min_remainder, delegation_min_time, delegation_return_time, delegation_max_delegators);
        auto bwprovider = vest.bwprovider_param(name(), name());

        auto params = "[" + vesting_withdraw + "," + vesting_amount + "," + delegation  + "," + bwprovider + "]";
        BOOST_CHECK_EQUAL(success(), vest.set_params(_issuer, _vesting_sym, params));
    }

    vector<account_name> add_accounts_with_vests(int count, int issue = 500, int buy = 100) {
        vector<account_name> additional_users;
        for (size_t u = 0; u < count; u++) {
            additional_users.push_back(user_name(u));
        }
        create_accounts(additional_users);
        for (auto& u : additional_users) {
            BOOST_CHECK_EQUAL(success(), token.issue(_issuer, u, token.make_asset(issue), "issue tokens"));
            BOOST_CHECK_EQUAL(success(), vest.open(u));
            BOOST_CHECK_EQUAL(success(), token.transfer(u, cfg::vesting_name, token.make_asset(buy), "buy vesting"));
        }
        return additional_users;
    }

protected:
    // TODO: make contract error messages more clear
    struct errors: contract_error_messages {
        const string key_not_found    = amsg("unable to find key");
        const string not_issuer       = amsg("Only token issuer can create it");    // TODO: test #744
        const string tokens_lt0       = amsg("the number of tokens should not be less than 0");
        const string tokens_frozen    = amsg("Tokens are frozen until the end of the period");
        const string undelegation_no_funds    = amsg("Insufficient funds for undelegation");
        const string not_enough_delegation    = amsg("There are not enough delegated tools for output");
        const string delegated_withdraw       = amsg("delegated vesting withdrawn");
        const string unknown_asset      = amsg("unknown asset");    // TODO: test #744
        const string withdraw_le0       = amsg("quantity must be positive");
        const string withdraw_low_rate  = amsg("withdraw rate is too low");
        const string withdraw_no_to_acc = amsg("to account does not exist");

        const string no_token_balance   = amsg("to account have not opened balance");
        const string wrong_precision    = amsg("wrong asset precision");
        const string vesting_params     = amsg("not found vesting params");
        const string issuer_not_autority = "missing authority of " + cfg::emission_name.to_string();

        const string withdraw_intervals        = amsg("intervals <= 0");
        const string withdraw_interval_seconds = amsg("interval_seconds <= 0");

        const string delegation_min_amount     = amsg("delegation min_amount <= 0");
        const string delegation_min_remainder  = amsg("delegation min_remainder <= 0");
        const string delegation_return_time    = amsg("delegation return_time <= 0");

        const string retire_le0         = amsg("must retire positive quantity");
        const string retire_no_vesting  = amsg("Vesting not found");
        const string retire_symbol_miss = amsg("symbol precision mismatch");
        const string retire_gt_supply   = amsg("invalid amount");
        const string retire_gt_balance  = amsg("overdrawn unlocked balance");
        const string retire_no_account  = amsg("no balance object found");

        const string not_found_token_vesting = amsg("not found token vesting");
        const string mismatch_of_accuracy = amsg("mismatch of accuracy of vesting");
        const string owner_not_exists = amsg("owner account does not exist");
        
        const string unsuccessful_delegation = amsg("unsuccessful delegation");
        const string self_delegate           = amsg("You can not delegate to yourself");
        const string interest_to_high        = amsg("interest_rate cannot be greater than 100% (10000)"); 
        const string delegation_no_funds     = amsg("Insufficient funds for delegation");
        const string cutoff                  = amsg("can't delegate, not enough power");
        const string max_delegators          = amsg("Recipient reached maximum of delegators");
        
        string missing_auth(name arg) { return "missing authority of " + arg.to_string(); };
    } err;

    const name _issuer = cfg::emission_name;
    const uint8_t withdraw_intervals = 13;
    const uint32_t withdraw_interval_seconds = 120;
    const uint64_t vesting_min_amount = 10*1e3;
    const uint64_t delegation_min_amount = 5e6;
    const uint64_t delegation_min_remainder = 15e6;
    const uint32_t delegation_min_time = 0;
    const uint32_t delegation_return_time = 120;
    const uint32_t delegation_max_delegators = 32;
};

BOOST_AUTO_TEST_SUITE(golos_vesting_tests)

BOOST_FIXTURE_TEST_CASE(token_tests, golos_vesting_tester) try {
    // TODO: actually tests token, remove? #744
    BOOST_TEST_MESSAGE("Test creating and issue token");
    BOOST_TEST_MESSAGE("--- create token");
    auto stats = mvo()
        ("supply", token.asset_str(0))
        ("max_supply", token.asset_str(100000))
        ("issuer", name(_issuer).to_string());
    BOOST_CHECK_EQUAL(success(), token.create(_issuer, token.make_asset(100000)));
    CHECK_MATCHING_OBJECT(token.get_stats(), stats);

    BOOST_TEST_MESSAGE("--- issue token");
    BOOST_CHECK_EQUAL(success(), token.issue(_issuer, N(sania), token.make_asset(200), "issue tokens sania"));
    BOOST_CHECK_EQUAL(success(), token.issue(_issuer, N(pasha), token.make_asset(300), "issue tokens pasha"));
    produce_block();

    CHECK_MATCHING_OBJECT(token.get_stats(), mvo(stats)("supply", token.asset_str(500)));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(200)));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), mvo()("balance", token.asset_str(300)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(set_params, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test vesting parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    produce_block();

    BOOST_TEST_MESSAGE("--- create token GOLOS");
    BOOST_CHECK_EQUAL(success(), token.create(_issuer, token.make_asset(100000)));

    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(vest.get_params(_token_sym).is_null());

    init_params();

    auto obj_params = vest.get_params(_token_sym);
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(obj_params));

    BOOST_TEST_CHECK(obj_params["withdraw"]["intervals"] == withdraw_intervals);
    BOOST_TEST_CHECK(obj_params["withdraw"]["interval_seconds"] == withdraw_interval_seconds);
    BOOST_TEST_CHECK(obj_params["min_amount"]["min_amount"] == vesting_min_amount);

    BOOST_TEST_CHECK(obj_params["delegation"]["min_amount"] == delegation_min_amount);
    BOOST_TEST_CHECK(obj_params["delegation"]["min_remainder"] == delegation_min_remainder);
    BOOST_TEST_CHECK(obj_params["delegation"]["min_time"] == delegation_min_time);
    BOOST_TEST_CHECK(obj_params["delegation"]["return_time"] == delegation_return_time);
    BOOST_TEST_CHECK(obj_params["delegation"]["max_delegators"] == delegation_max_delegators);

    auto params = "[" + vest.withdraw_param(0, withdraw_interval_seconds) + "]";
    BOOST_CHECK_EQUAL(err.withdraw_intervals, vest.set_params(_issuer, _vesting_sym, params));

    params = "[" + vest.withdraw_param(withdraw_intervals, 0) + "]";
    BOOST_CHECK_EQUAL(err.withdraw_interval_seconds, vest.set_params(_issuer, _vesting_sym, params));

    params = "[" + vest.delegation_param(0, delegation_min_remainder, delegation_min_time, delegation_return_time, delegation_max_delegators) + "]";
    BOOST_CHECK_EQUAL(err.delegation_min_amount, vest.set_params(_issuer, _vesting_sym, params));

    params = "[" + vest.delegation_param(delegation_min_amount, 0, delegation_min_time, delegation_return_time, delegation_max_delegators) + "]";
    BOOST_CHECK_EQUAL(err.delegation_min_remainder, vest.set_params(_issuer, _vesting_sym, params));

    params = "[" + vest.delegation_param(delegation_min_amount, delegation_min_remainder, delegation_min_time, 0, delegation_max_delegators) + "]";
    BOOST_CHECK_EQUAL(err.delegation_return_time, vest.set_params(_issuer, _vesting_sym, params));


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test creating vesting");
    const bool skip_authority_check = true;

    BOOST_CHECK_EQUAL(success(), token.create(_issuer, token.make_asset(100000)));

    BOOST_TEST_MESSAGE("--- fail on non-existing token");
    BOOST_CHECK_EQUAL(err.key_not_found, vest.create_vesting(_issuer, symbol(_token_precision, "GOLOSA"), N(notify.acc), skip_authority_check));

    BOOST_TEST_MESSAGE("--- fails when not issuer");
    BOOST_CHECK_EQUAL(err.issuer_not_autority, vest.create_vesting(N(sania), _vesting_sym, N(notify.acc), skip_authority_check));
    // TODO: test issuers list #744

    BOOST_CHECK_EQUAL(err.not_found_token_vesting, vest.open(N(sania)));

    BOOST_TEST_MESSAGE("--- succeed in normal conditions");
    BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer));

    BOOST_CHECK_EQUAL(err.mismatch_of_accuracy, vest.open(N(sania), _vesting_sym_e, N(sania)));
    BOOST_CHECK_EQUAL(err.owner_not_exists, vest.open(N(denlarimer), _vesting_sym_e, N(sania)));

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

    CHECK_MATCHING_OBJECT(token.get_account(cfg::vesting_name), mvo()("balance", token.asset_str(buy1+buy2)));
    CHECK_MATCHING_OBJECT(vest.get_stats(), mvo()("supply", vest.asset_str(buy1+buy2)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(bulk_buy_vesting, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test bulk buying vesting / converting token to vesting");
    BOOST_CHECK_EQUAL(success(), token.create(_issuer, token.make_asset(100000)));
    BOOST_CHECK_EQUAL(success(), token.issue(_issuer, N(sania), token.make_asset(100000), "issue tokens sania"));
    produce_block();

    BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer));
    BOOST_CHECK_EQUAL(success(), vest.open(N(sania)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(pasha)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(tania)));
    BOOST_CHECK_EQUAL(success(), vest.open(N(vania)));
    produce_block();

    BOOST_CHECK_EQUAL(success(), token.bulk_transfer(N(sania), {
        {cfg::vesting_name, token.make_asset(200), golos::config::send_prefix + name{"pasha"}.to_string()},
        {cfg::vesting_name, token.make_asset(100), golos::config::send_prefix + name{"vania"}.to_string()}
    }));

    const auto magic1 = 0.05;   // TODO: proper test #744
    const auto magic2 = 3.568;
    const auto magic3 = 584.076;
    BOOST_CHECK_EQUAL(success(), token.bulk_transfer(N(sania), {
        {cfg::vesting_name, token.make_asset(magic1), "buy vesting"},
        {cfg::vesting_name, token.make_asset(magic2), "buy vesting"},
        {cfg::vesting_name, token.make_asset(magic3), "buy vesting"}
    }));

    BOOST_CHECK_EQUAL(success(), token.bulk_transfer(N(sania), {
        {cfg::vesting_name, token.make_asset(50), "buy vesting"},
        {N(tania), token.make_asset(50), "transfer token"}
    }));

    const auto magic4 = 99012.306;   // TODO: proper test #744
    const auto magic5 = 637.694;
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), mvo()("balance", token.asset_str(magic4)));
    CHECK_MATCHING_OBJECT(token.get_account(N(tania)), mvo()("balance", token.asset_str(50)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(magic5));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(200));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(vania)), vest.make_balance(100));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(tania)), vest.make_balance(0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test converting vesting to token (withdraw)");

    auto issue = 500, buy = 100;
    prepare_balances(100500, issue, issue, buy, buy);
    auto tokens = issue - buy;  // price is 1:1
    auto vests = buy;
    auto payments = 0, payments2 = 0;
    auto vests2 = vests;
    init_params();

    BOOST_CHECK_EQUAL(success(), vest.timeout(_code));

    BOOST_TEST_MESSAGE("--- check that no convert object exists before start converting");
    BOOST_TEST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());

    BOOST_TEST_MESSAGE("--- check that bad asset or precision fails to convert");
    auto do_withdraw = [&](auto amount, auto to) {
        return vest.withdraw(N(sania), to, amount);
    };
    auto custom_withdraw = [&](auto amount, auto precision, auto sname) {
        return do_withdraw(asset(amount, symbol(precision, sname)), N(sania));
    };
    BOOST_CHECK_EQUAL(err.withdraw_le0, do_withdraw(vest.make_asset(0), N(sania)));
    BOOST_CHECK_EQUAL(err.withdraw_le0, do_withdraw(vest.make_asset(-1), N(sania)));
    BOOST_CHECK_EQUAL(err.withdraw_low_rate, custom_withdraw(withdraw_intervals-1, _vesting_precision, _token_name));
    BOOST_CHECK_EQUAL(err.withdraw_no_to_acc, do_withdraw(vest.make_asset(1), N(not.exists)));
    BOOST_CHECK_EQUAL(err.vesting_params, custom_withdraw(1e6, _vesting_precision, "GLS"));
    BOOST_CHECK_EQUAL(err.wrong_precision, custom_withdraw(1e5, _vesting_precision-1, _token_name));
    BOOST_CHECK_EQUAL(err.wrong_precision, custom_withdraw(1e7, _vesting_precision+1, _token_name));

    BOOST_TEST_MESSAGE("--- succeed if start converting with correct asset");

    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(withdraw_intervals)));

    const auto steps = withdraw_intervals;
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps);

    const int64_t _blocks_in_interval = golos::seconds_to_blocks(withdraw_interval_seconds);
    BOOST_TEST_MESSAGE("--- skip " + std::to_string(_blocks_in_interval) + " blocks and check same balance");
    produce_blocks(_blocks_in_interval);
    // note: deferred tx will be executed at current block, but after normal txs,
    // so it's result will be available only in next block
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens));

    BOOST_TEST_MESSAGE("--- go next block and check that withdrawal happened");
    produce_block();
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps - 1);
    vests--;
    payments++;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens, payments));

    BOOST_TEST_MESSAGE("--- skip " + std::to_string(_blocks_in_interval*(steps-1)-1) + " blocks and check balance");
    produce_blocks(_blocks_in_interval * (steps - 1) - 1);  // 1 withdrawal already passed, so -1
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], 1);
    vests -= steps - 2;
    payments += steps - 2;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens, payments));

    BOOST_TEST_MESSAGE("--- go next block and check that fully withdrawn");
    produce_block();
    BOOST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    vests--;
    payments++;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens, payments));

    BOOST_TEST_MESSAGE("--- check withdraw to different account");
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(pasha), vest.make_asset(withdraw_intervals)));
    BOOST_CHECK(vest.get_withdraw_obj(N(pasha)).is_null());

    BOOST_TEST_MESSAGE("--- skip " + std::to_string(_blocks_in_interval*steps-1) + " blocks and check balance");
    produce_blocks(_blocks_in_interval * steps);
    vests -= steps - 1;
    payments2 = steps - 1;
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(vests2));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens, payments));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), token.make_balance(tokens, payments2));

    BOOST_TEST_MESSAGE("--- go next block and check that fully withdrawn");
    produce_block();
    vests--;
    payments2++;
    BOOST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(vests2));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(tokens, payments));
    CHECK_MATCHING_OBJECT(token.get_account(N(pasha)), token.make_balance(tokens, payments2));

    BOOST_TEST_MESSAGE("--- check that cannot withdraw with closed token balance");
    produce_block();
    auto balance = token.get_account(N(sania)).get_object();
    auto tok = balance["balance"].as<asset>();
    auto pay = balance["payments"].as<asset>();
    BOOST_CHECK_EQUAL(success(), token.claim(N(sania), pay));
    BOOST_CHECK_EQUAL(success(), token.transfer(N(sania), cfg::vesting_name, tok + pay, "buy vesting for all remain"));
    BOOST_CHECK_EQUAL(success(), token.close(N(sania), _token_sym));
    BOOST_CHECK_EQUAL(err.no_token_balance, vest.withdraw(N(sania), N(sania), vest.make_asset(withdraw_intervals)));

    // TODO: check fail if not enough funds and other balance issues #744
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw_fail, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test auto-stop withdraw if wrong conditions met");
    auto man = N(sania);
    auto issue = 100, buy = 100;
    prepare_balances(100500, issue, issue, buy, buy);
    auto tokens = issue - buy;  // price is 1:1
    auto vests = buy;
    init_params();
    BOOST_CHECK_EQUAL(success(), vest.timeout(_code));

    BOOST_TEST_MESSAGE("-- check stop if token balance closed");
    BOOST_TEST_CHECK(vest.get_withdraw_obj(man).is_null());
    BOOST_TEST_MESSAGE("---- start withdraw");
    BOOST_CHECK_EQUAL(success(), vest.withdraw(man, man, vest.make_asset(vests)));
    const auto steps = withdraw_intervals;

    BOOST_TEST_MESSAGE("---- check withdraw object existance");
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(man)["remaining_payments"], steps);
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(man), token.make_balance(0, tokens));

    BOOST_TEST_MESSAGE("---- close token balance");
    BOOST_CHECK_EQUAL(success(), token.close(man, _token_sym));
    BOOST_TEST_CHECK(token.get_account(man).is_null());

    const int64_t _blocks_in_interval = golos::seconds_to_blocks(withdraw_interval_seconds);
    BOOST_TEST_MESSAGE("---- wait " + std::to_string(_blocks_in_interval + 1) +
        " blocks to payout and check that withdraw object deleted (auto-stop)");
    produce_blocks(_blocks_in_interval + 1);
    BOOST_TEST_CHECK(vest.get_withdraw_obj(man).is_null());


    BOOST_TEST_MESSAGE("-- check stop if converted amount too low");
    BOOST_CHECK_EQUAL(success(), token.open(man, _token_sym));
    int factor = 100;
    BOOST_CHECK_EQUAL(success(), vest.withdraw(man, man, asset(factor - 1, _vesting_sym)));
    BOOST_TEST_MESSAGE("---- check withdraw object existance");
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(man)["remaining_payments"], steps);
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(man), token.make_balance(0, tokens));

    BOOST_TEST_MESSAGE("---- reduce token supply by 99% so price changes to 1:100");
    auto vsupply = vests + vests;
    auto remainder = vsupply / factor;
    CHECK_MATCHING_OBJECT(token.get_account(_code), mvo()("balance", token.make_asset(vsupply)));
    BOOST_CHECK_EQUAL(success(), token.transfer(_code, N(vania), token.make_asset(vsupply - remainder), "funds out"));
    CHECK_MATCHING_OBJECT(token.get_account(_code), mvo()("balance", token.make_asset(remainder)));

    BOOST_TEST_MESSAGE("---- wait " + std::to_string(_blocks_in_interval + 1) +
        " blocks to payout and check that withdraw object deleted (auto-stop)");
    produce_blocks(_blocks_in_interval + 1);
    BOOST_TEST_CHECK(vest.get_withdraw_obj(man).is_null());

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(withdraw_convert_rate, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test that convert rate is correct when withdrawing");
    int base = 100;             // withdraw_rate will be equal to this value
    auto buy1 = base * withdraw_intervals;
    auto buy2 = base * 10;      // this makes fractional withdraw_rate
    prepare_balances(100500, buy1, buy2, buy1, buy2);   // puts buy1+buy2 to vesting
    init_params();

    BOOST_CHECK_EQUAL(success(), vest.timeout(_code));
    BOOST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(buy1)));
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(pasha), N(pasha), vest.make_asset(buy2)));

    const int64_t blocks_to_wait = golos::seconds_to_blocks(withdraw_intervals * withdraw_interval_seconds);
    BOOST_TEST_MESSAGE("--- skip " + std::to_string(blocks_to_wait) + " blocks and check balances");
    produce_blocks(blocks_to_wait);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(pasha))["remaining_payments"], 1);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(base));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(0, buy1 - base));

    BOOST_TEST_MESSAGE("--- go next block and check that all vesting withdrawn");
    produce_block();
    BOOST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    BOOST_CHECK(vest.get_withdraw_obj(N(pasha)).is_null());
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(0));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(0));
    // Balances can be not precise at this point, but their sum should not change
    auto balance1 = token.get_account(N(sania), true)["payments"].as<asset>();
    auto balance2 = token.get_account(N(pasha), true)["payments"].as<asset>();
    BOOST_CHECK_EQUAL(token.make_asset(buy1 + buy2), balance1 + balance2);
    CHECK_MATCHING_OBJECT(vest.get_stats(), mvo()("supply", vest.asset_str(0)));
    CHECK_MATCHING_OBJECT(token.get_account(cfg::vesting_name), token.make_balance(0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(stop_withdraw, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test canceling convert vesting to token");

    int tokens = 100, vests = 100;
    prepare_balances(100500, tokens, tokens, vests, vests);
    tokens -= vests;
    init_params();
    BOOST_CHECK_EQUAL(success(), vest.timeout(cfg::vesting_name));

    BOOST_TEST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(withdraw_intervals)));
    auto steps = withdraw_intervals;
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps);

    BOOST_TEST_MESSAGE("--- check 1st withdrawal");
    const int64_t _blocks_in_interval = golos::seconds_to_blocks(withdraw_interval_seconds);
    produce_blocks(_blocks_in_interval + 1);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps - 1);
    vests -= 1;
    tokens += 1;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(0, tokens));

    BOOST_TEST_MESSAGE("--- change withdraw paramters and check it applied after next payment");
    int mult = 2;
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(withdraw_intervals * mult)));
    produce_blocks(_blocks_in_interval + 1);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps - 1);
    vests -= mult;
    tokens += mult;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(0, tokens));

    BOOST_TEST_MESSAGE("--- wait some time, change withdraw paramters and check that next payout moved");
    int blocks_to_payout = 5;
    mult = 7;
    produce_blocks(_blocks_in_interval - blocks_to_payout);
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(withdraw_intervals * mult)));
    produce_blocks(blocks_to_payout + 1);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps);

    BOOST_TEST_MESSAGE("--- need to wait whole interval to reach payout time. go to block just before withdraw");
    produce_blocks(_blocks_in_interval - blocks_to_payout - 1);
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps);

    BOOST_TEST_MESSAGE("--- wait 1 more block for withdraw and check payout");
    produce_block();
    BOOST_CHECK_EQUAL(vest.get_withdraw_obj(N(sania))["remaining_payments"], steps-1);
    vests -= mult;
    tokens += mult;
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(0, tokens));

    BOOST_TEST_MESSAGE("--- cancel convert and check convert object deleted");
    BOOST_CHECK_EQUAL(success(), vest.stop_withdraw(N(sania), vest.make_asset(0).get_symbol()));
    BOOST_TEST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());

    BOOST_TEST_MESSAGE("--- go to next withdrawal time and check it not happens");
    produce_blocks(_blocks_in_interval);
    BOOST_TEST_CHECK(vest.get_withdraw_obj(N(sania)).is_null());
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(vests));
    CHECK_MATCHING_OBJECT(token.get_account(N(sania)), token.make_balance(0, tokens));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delegate, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test delegating vesting shares");

    prepare_balances();
    init_params();
    BOOST_CHECK_EQUAL(success(), charge.set_restorer(_issuer, 0, "t", 42, 42, 42));
    const int divider = vest.make_asset(1).get_amount();
    const auto min_fract = 1. / divider;
    const int min_remainder = delegation_min_remainder / divider;
    const auto amount = vest.make_asset(min_remainder);
    const auto charge_prop = 0.7;

    BOOST_TEST_MESSAGE("--- fail when delegate to self");
    BOOST_CHECK_EQUAL(err.unsuccessful_delegation, vest.msig_delegate(N(sania), N(sania), amount));

    BOOST_TEST_MESSAGE("--- fail on bad interest rate");
    BOOST_CHECK_EQUAL(err.interest_to_high, vest.delegate(N(sania), N(pasha), amount, 10001)); // TODO: test boundaries #744

    BOOST_TEST_MESSAGE("--- fail when delegate 0 or less than min_remainder");
    BOOST_CHECK_EQUAL(err.tokens_lt0, vest.delegate(N(sania), N(pasha), vest.make_asset(0)));
    BOOST_CHECK_EQUAL(err.delegation_no_funds, vest.delegate(N(sania), N(pasha), vest.make_asset(min_fract)));
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.delegate(N(sania), N(pasha), vest.make_asset(min_remainder - min_fract)));

    BOOST_TEST_MESSAGE("--- insufficient funds assert");
    BOOST_CHECK_EQUAL(err.delegation_no_funds, vest.delegate(N(sania), N(pasha), vest.make_asset(10000)));
    
    BOOST_TEST_MESSAGE("--- fail when not enough signatures");
    BOOST_CHECK_EQUAL(err.missing_auth(N(sania)), vest.delegate_unauthorized(N(sania), N(pasha), amount, false));
    BOOST_CHECK_EQUAL(err.missing_auth(N(pasha)), vest.delegate_unauthorized(N(sania), N(pasha), amount, true));

    BOOST_TEST_MESSAGE("--- fail when delegate more than scheduled to withdraw");
    BOOST_CHECK_EQUAL(success(), vest.msig_delegate(N(sania), N(pasha), amount, 0));
    BOOST_CHECK_EQUAL(success(), vest.msig_delegate(N(sania), N(tania), amount, 0));
    BOOST_CHECK_EQUAL(success(), vest.withdraw(N(sania), N(sania), vest.make_asset(100-3*min_remainder)));
    BOOST_CHECK_EQUAL(err.delegation_no_funds, vest.delegate(N(sania), N(vania), vest.make_asset(min_remainder+1)));
    BOOST_TEST_MESSAGE("--- succeed when withdraval counted");
    BOOST_CHECK_EQUAL(success(), vest.msig_delegate(N(sania), N(vania), amount));

    BOOST_TEST_MESSAGE("--- charge limit test");
    BOOST_CHECK_EQUAL(success(), charge.use(_issuer, N(pasha), 0, charge_prop * cfg::_100percent, cfg::_100percent));
    BOOST_CHECK_EQUAL(err.cutoff, vest.delegate(N(pasha), N(vania), vest.make_asset(default_vesting_amount * (1.01 - charge_prop))));
    BOOST_CHECK_EQUAL(success(), vest.msig_delegate(N(pasha), N(vania), vest.make_asset(default_vesting_amount * (1.0 - charge_prop))));

    BOOST_TEST_MESSAGE("--- check max delegators limit");
    auto additional_users = add_accounts_with_vests(delegation_max_delegators + 1 - 2); // 2 are already delegated
    for (size_t u = 0; u < additional_users.size() - 1; u++) {
        BOOST_CHECK_EQUAL(success(), vest.delegate(additional_users[u], N(vania), vest.make_asset(20)));
    }
    BOOST_CHECK_EQUAL(err.max_delegators, vest.delegate(additional_users.back(), N(vania), vest.make_asset(20)));
    BOOST_CHECK_EQUAL(success(), vest.undelegate(additional_users[0], N(vania), vest.make_asset(11)));
    BOOST_CHECK_EQUAL(err.max_delegators, vest.delegate(additional_users.back(), N(vania), vest.make_asset(20)));
    BOOST_CHECK_EQUAL(success(), vest.undelegate(additional_users[0], N(vania), vest.make_asset(9)));
    BOOST_CHECK_EQUAL(success(), vest.delegate(additional_users.back(), N(vania), vest.make_asset(20)));

//    // BOOST_CHECK_EQUAL(success(), vest.delegate(N(sania), N(issuer), amount));    // TODO: fix: sending to issuer instead of vania fails #744
//    // TODO: check delegation to account without balance #744
//    // TODO: check change delegation #744
//    // TODO: check min step #744
//    // TODO: check min remainder #744
//    // TODO: check delegation object #744
//    // TODO: check battery limitation #744
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(undelegate, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test un-delegating vesting shares");

    prepare_balances();
    init_params();

    const int divider = vest.make_asset(1).get_amount();
    const int min_step = delegation_min_amount / divider;
    const int min_remainder = delegation_min_remainder / divider;
    const int amount = min_step + min_remainder;

    BOOST_CHECK_EQUAL(success(), vest.delegate(N(sania), N(pasha), vest.make_asset(amount)));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, amount));
    produce_block();

    if (delegation_min_time > 0) {
        // TODO: test it when config allowed to be changed with `ctrl` #744
        BOOST_TEST_MESSAGE("--- fail when undelegate earlier than min_time");
        BOOST_CHECK_EQUAL(err.tokens_frozen, vest.undelegate(N(sania), N(pasha), vest.make_asset(min_step)));
        produce_blocks(golos::seconds_to_blocks(delegation_min_time));
    }

    BOOST_TEST_MESSAGE("--- fail when undelegate more than delegated");
    BOOST_CHECK_EQUAL(err.not_enough_delegation, vest.undelegate(N(sania), N(pasha), vest.make_asset(amount + 1))); // TODO: boundary #744

    BOOST_TEST_MESSAGE("--- fail when step is less than min_amount");
    BOOST_CHECK_EQUAL(err.undelegation_no_funds, vest.undelegate(N(sania), N(pasha), vest.make_asset(min_step - 1))); // TODO: boundary #744

    BOOST_TEST_MESSAGE("--- fail when remainder less than min_remainder");
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.undelegate(N(sania), N(pasha), vest.make_asset(min_step + 1))); // TODO: boundaries #744
    BOOST_CHECK_EQUAL(err.delegated_withdraw, vest.undelegate(N(sania), N(pasha), vest.make_asset(amount - 1)));

    BOOST_TEST_MESSAGE("--- succed in normal conditions");
    BOOST_CHECK_EQUAL(success(), vest.undelegate(N(sania), N(pasha), vest.make_asset(min_step)));
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

    BOOST_TEST_MESSAGE("--- check full undelegation");
    BOOST_CHECK_EQUAL(success(), vest.undelegate(N(sania), N(pasha), vest.make_asset(amount - min_step), true));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, amount - min_step));
    CHECK_MATCHING_OBJECT(vest.get_balance(N(pasha)), vest.make_balance(100, 0, 0));
    produce_blocks(golos::seconds_to_blocks(delegation_return_time) + 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(N(sania)), vest.make_balance(100, 0));

    // TODO: check delegation and return objects #744
    // TODO: check step, remainder #744
    // TODO: check min time, return time #744
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(retire, golos_vesting_tester) try {
    BOOST_TEST_MESSAGE("Test retire vesting shares");

    int supply = 100500, tokens = 500, vests = 100, burn = 4;
    int vests_supply = 2 * vests;
    prepare_balances(supply, tokens, tokens, vests, vests);
    int withdraw_seconds = withdraw_interval_seconds * 2;
    init_params(withdraw_seconds); // need to separate withdraw and undelegation to test properly
    BOOST_CHECK_EQUAL(success(), vest.timeout(_code));

    const int64_t blocks_in_interval = golos::seconds_to_blocks(withdraw_seconds);
    int steps = withdraw_intervals;
    int withdraw_per_step = 5;
    int withdraw = withdraw_intervals * withdraw_per_step;
    int delegate = 20;
    constexpr auto man = N(sania), man2 = N(pasha);

    auto retire_asset = [&](asset amount, name who = N(sania)) {
        return vest.retire(amount, who, _issuer);
    };
    auto retire = [&](int amount, name who = N(sania)) {
        return retire_asset(vest.make_asset(amount), who);
    };

    BOOST_TEST_MESSAGE("--- check fail on wrong parameters");
    BOOST_CHECK_EQUAL(err.retire_le0, retire(0));
    BOOST_CHECK_EQUAL(err.retire_le0, retire(-1));

    BOOST_CHECK_EQUAL(success(), token.create(_issuer, asset(supply, symbol(_vesting_precision, "BAD"))));
    BOOST_CHECK_EQUAL(err.retire_no_vesting, retire_asset(asset(1, symbol(_vesting_precision, "BAD"))));
    BOOST_CHECK_EQUAL(err.retire_symbol_miss, retire_asset(asset(1, _vesting_sym_e)));
    BOOST_CHECK_EQUAL(err.retire_gt_supply, retire(vests_supply + 1));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(vests_supply));
    BOOST_CHECK_EQUAL(err.retire_no_account, retire(1, N(bania)));

    BOOST_TEST_MESSAGE("--- check fail if not unlocked");
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(burn));

    BOOST_TEST_MESSAGE("--- check success after unlock limit");
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests));
    BOOST_CHECK_EQUAL(success(), vest.unlock_limit(man, vest.make_asset(burn)));
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, burn));
    BOOST_CHECK_EQUAL(success(), retire(burn));
    vests -= burn;
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, 0));

    BOOST_TEST_MESSAGE("--- check limiting with withdraw/delegation");
    int unlocked = supply;
    BOOST_CHECK_EQUAL(success(), vest.unlock_limit(man, vest.make_asset(unlocked)));
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, unlocked));
    BOOST_CHECK_EQUAL(success(), vest.withdraw(man, man, vest.make_asset(withdraw)));
    BOOST_CHECK_EQUAL(success(), vest.delegate(man, man2, vest.make_asset(delegate)));
    int allowed = vests - withdraw - delegate;
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(allowed + 1));
    BOOST_CHECK_EQUAL(success(), retire(allowed));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(1));
    vests -= allowed;
    unlocked -= allowed;
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, delegate, 0, unlocked));

    BOOST_TEST_MESSAGE("--- check that limit reduces after undelegate");
    BOOST_CHECK_EQUAL(success(), vest.undelegate(man, man2, vest.make_asset(delegate)));
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, delegate, 0, unlocked));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(1));
    auto blocks_to_return = golos::seconds_to_blocks(delegation_return_time);
    produce_blocks(blocks_to_return + 1);
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, unlocked));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(delegate + 1));
    BOOST_CHECK_EQUAL(success(), retire(1));
    BOOST_CHECK_EQUAL(success(), retire(delegate/2 - 1));
    vests -= delegate/2;
    unlocked -= delegate/2;
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, unlocked));

    BOOST_TEST_MESSAGE("--- check that limit reduces after withdraw payout (vesting balance reduces too)");
    produce_blocks(blocks_in_interval - blocks_to_return);
    vests -= withdraw_per_step;
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, unlocked));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(delegate/2 + 1));
    BOOST_CHECK_EQUAL(success(), retire(delegate/2));
    BOOST_CHECK_EQUAL(err.retire_gt_balance, retire(1));
    vests -= delegate/2;
    unlocked -= delegate/2;
    CHECK_MATCHING_OBJECT(vest.get_balance(man), vest.make_balance(vests, 0, 0, unlocked));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
