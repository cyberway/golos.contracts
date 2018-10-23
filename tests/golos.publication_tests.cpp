#define UNIT_TEST_ENV
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <eosio/chain/abi_serializer.hpp>

#include <Runtime/Runtime.h>
#include <fc/variant_object.hpp>
#include <eosio/testing/tester.hpp>

#include "golos_tester.hpp"
#include "contracts.hpp"

#include "../golos.publication/types.h"
#include "../golos.publication/config.hpp"
using namespace fixed_point_utils;

#define UNLOCKED_FOR_CREATE_MESSAGE 21

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace golos::config;

using mvo = fc::mutable_variant_object;

namespace structures {
    struct beneficiaries {
        account_name account;
        int64_t deductprcnt;
    };

    struct tags {
        std::string tag;
    };
}
FC_REFLECT(structures::tags, (tag))
FC_REFLECT(structures::beneficiaries, (account)(deductprcnt))


class golos_publication_tester : public golos_tester {
    std::vector<account_name> _users;
public:

    golos_publication_tester(): _users{N(jackiechan), N(brucelee), N(chucknorris), N(golos.pub)}{
        produce_blocks(2);
        create_accounts(_users);
        create_accounts({N(eosio.token)});
        create_accounts({vesting_name});
        create_accounts({N(dan.larimer)});
        
        produce_blocks(2);

        install_contract(N(golos.pub), contracts::posting_wasm(), contracts::posting_abi(), _serializers[N(golos.pub)]);
        install_contract(N(eosio.token), contracts::token_wasm(), contracts::token_abi(), _serializers[N(eosio.token)]);
        install_contract(vesting_name, contracts::vesting_wasm(), contracts::vesting_abi(), _serializers[vesting_name]);
    }

    action_result push_action(const account_name& signer,
                              const action_name &name,
                              const variant_object &data, const account_name& code = N(golos.pub)) {
       auto& abi_ser = _serializers[code]; 
       string action_type_name = abi_ser.get_action_type(name);       
       action act;
       act.account = code;
       act.name = name;
       act.data = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);

       return base_tester::push_action(std::move(act), uint64_t(signer));
    }
    
    void init() {
        symbol sym(0, "DUMMY");
        BOOST_REQUIRE_EQUAL(success(), push_action(N(golos.pub), N(open), 
            mvo()( "owner", account_name(N(golos.pub)))
            ( "symbol", sym)
            ( "ram_payer", account_name(N(golos.pub))), 
            N(eosio.token)));  
        
        limitsarg lims = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {0, 0}, {0, 0, 0}};
        BOOST_REQUIRE_EQUAL(success(), push_action(N(golos.pub), N(setrules), mvo()                
                ("mainfunc", mvo()("str", "0")("maxarg", 1))
                ("curationfunc", mvo()("str", "0")("maxarg", 1))
                ("timepenalty", mvo()("str", "0")("maxarg", 1))
                ("curatorsprop", 0)
                ("maxtokenprop", 0)
                ("tokensymbol", sym)
                ("lims", lims)));
        for(auto& u : _users) {
            BOOST_REQUIRE_EQUAL(success(), push_action( u, N(open), 
                mvo()( "owner", u)
                ( "symbol", sym)
                ( "ram_payer", u), 
                vesting_name));   
        }
    }

    action_result create_message(account_name account, std::string permlink,
                              account_name parentacc = N(), std::string parentprmlnk = "parentprmlnk",
                              std::vector<structures::beneficiaries> beneficiaries = {{N(golos.pub), 777}},
                              int64_t tokenprop = 5000, bool vestpayment = false,
                              std::string headermssg = "headermssg",
                              std::string bodymssg = "bodymssg", std::string languagemssg = "languagemssg",
                              std::vector<structures::tags> tags = {{"tag"}},
                              std::string jsonmetadata = "jsonmetadata") {
        return push_action(account, N(createmssg), mvo()
                           ("account", account)
                           ("permlink", permlink)
                           ("parentacc", parentacc)
                           ("parentprmlnk", parentprmlnk)
                           ("beneficiaries", beneficiaries)
                           ("tokenprop", tokenprop)
                           ("vestpayment", vestpayment)
                           ("headermssg", headermssg)
                           ("bodymssg", bodymssg)
                           ("languagemssg", languagemssg)
                           ("tags", tags)
                           ("jsonmetadata", jsonmetadata)
        );
    }

    fc::variant get_messages( account_name acc, uint64_t id ) {
        return get_tbl_struct(N(golos.pub), acc, N(messagetable), id, "message", _serializers[N(golos.pub)]);
    }

    fc::variant get_content( account_name acc, uint64_t id ) {
        return get_tbl_struct(N(golos.pub), acc, N(contenttable), id, "content", _serializers[N(golos.pub)]);
    }

    fc::variant get_vote( account_name acc, uint64_t id ) {
        return get_tbl_struct(N(golos.pub), acc, N(votetable), id, "voteinfo", _serializers[N(golos.pub)]);
    }

    action_result update_message(account_name account, std::string permlink,
                              std::string headermssg, std::string bodymssg,
                              std::string languagemssg, std::vector<structures::tags> tags,
                              std::string jsonmetadata) {
        return push_action(account, N(updatemssg), mvo()
                           ("account", account)
                           ("permlink", permlink)
                           ("headermssg", headermssg)
                           ("bodymssg", bodymssg)
                           ("languagemssg", languagemssg)
                           ("tags",tags)
                           ("jsonmetadata", jsonmetadata)
        );
    }

    action_result delete_message(account_name account, std::string permlink) {
        return push_action(account, N(deletemssg), mvo()
                           ("account", account)
                           ("permlink", permlink)
        );
    }

    action_result upvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
        return push_action(voter, N(upvote), mvo()
                           ("voter", voter)
                           ("author", author)
                           ("permlink", permlink)
                           ("weight", weight)
        );
    }

    action_result downvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
        return push_action(voter, N(downvote), mvo()
                           ("voter", voter)
                           ("author", author)
                           ("permlink", permlink)
                           ("weight", weight)
        );
    }

    action_result unvote(account_name voter, account_name author, std::string permlink) {
        return push_action(voter, N(unvote), mvo()
                           ("voter", voter)
                           ("author", author)
                           ("permlink", permlink)
        );
    }

    void check_equal(const fc::variant &obj1, const fc::variant &obj2) {
        BOOST_CHECK_EQUAL(true, obj1.is_object() && obj2.is_object());
        BOOST_CHECK_EQUAL(obj1.get_object()["id"].as<uint64_t>(), obj2.get_object()["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["parentacc"].as<account_name>(), obj2.get_object()["parentacc"].as<account_name>());
        BOOST_CHECK_EQUAL(obj1.get_object()["parent_id"].as<uint64_t>(), obj2.get_object()["parent_id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["beneficiaries"].get_array().at(0).get_object()["account"].as<account_name>(), obj2.get_object()["beneficiaries"].get_array().at(0).get_object()["account"].as<account_name>());
        BOOST_CHECK_EQUAL(obj1.get_object()["beneficiaries"].get_array().at(0).get_object()["deductprcnt"].as<uint64_t>(), obj2.get_object()["beneficiaries"].get_array().at(0).get_object()["deductprcnt"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["childcount"].as<uint64_t>(), obj2.get_object()["childcount"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["closed"].as<bool>(), obj2.get_object()["closed"].as<bool>());
    }

    void check_equal_content(const fc::variant &obj1, const fc::variant &obj2) {
        BOOST_CHECK_EQUAL(true, obj1.is_object() && obj2.is_object());
        BOOST_CHECK_EQUAL(obj1.get_object()["id"].as<uint64_t>(), obj2.get_object()["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["headermssg"].as<std::string>(), obj2.get_object()["headermssg"].as<std::string>());
        BOOST_CHECK_EQUAL(obj1.get_object()["bodymssg"].as<std::string>(), obj2.get_object()["bodymssg"].as<std::string>());
        BOOST_CHECK_EQUAL(obj1.get_object()["languagemssg"].as<std::string>(), obj2.get_object()["languagemssg"].as<std::string>());
        BOOST_CHECK_EQUAL(obj1.get_object()["tags"].get_array().at(0).get_object()["tag"].as_string(), obj2.get_object()["tags"].get_array().at(0).get_object()["tag"].as_string());
        BOOST_CHECK_EQUAL(obj1.get_object()["jsonmetadata"].as<std::string>(), obj2.get_object()["jsonmetadata"].as<std::string>());
    }

    std::map<account_name, abi_serializer> _serializers;
};

BOOST_AUTO_TEST_SUITE(golos_publication_tests)

BOOST_FIXTURE_TEST_CASE(create_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Create message testing.");
    init();
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));
                            
    BOOST_TEST_MESSAGE("Checking that another user can create a message with the same permlink.");
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(chucknorris),
                            "permlink"));

    auto mssg_stats = get_messages(N(brucelee), 0);
    check_equal( mssg_stats, mvo()
                   ("id", hash64("permlink"))
                   ("date", 1577836804)
                   ("parentacc", "")
                   ("parent_id", 0)
                   ("beneficiaries", variants({
                                                  mvo()
                                                  ("account", "golos.pub")
                                                  ("deductprcnt", static_cast<base_t>(elaf_t(elai_t(777) / elai_t(10000)).data()))
                                              }))
                   ("childcount", 0)
                   ("closed", false)
                   );

    auto content_stats = get_content(N(brucelee), hash64("permlink"));
    check_equal_content(content_stats, mvo()
                            ("id", hash64("permlink"))
                            ("headermssg", "headermssg")
                            ("bodymssg", "bodymssg")
                            ("languagemssg", "languagemssg")
                            ("tags", variants({
                                                  mvo()
                                                  ("tag", "tag")
                                              }))
                            ("jsonmetadata", "jsonmetadata"));

    produce_blocks(CLOSE_MESSAGE_PERIOD*2-2);

    {
        BOOST_TEST_MESSAGE("Checking that message wasn't closed.");
        auto mssg_stats = get_messages(N(brucelee), hash64("permlink"));
        BOOST_CHECK_EQUAL(fc::variant(mssg_stats).get_object()["closed"].as<bool>(), false);
    }

    {
        BOOST_TEST_MESSAGE("Checking that message was closed.");
        produce_blocks(1);
        auto mssg_stats = get_messages(N(brucelee), hash64("permlink"));
        BOOST_CHECK_EQUAL(fc::variant(mssg_stats).get_object()["closed"].as<bool>(), true);
    }

    {
        BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                                N(jackiechan),
                                "permlink1",
                                N(brucelee),
                                "permlink"));

        auto mssg_stats = get_messages(N(brucelee), 0);
        BOOST_CHECK_EQUAL(fc::variant(mssg_stats).get_object()["childcount"].as<uint64_t>(), 1);
    }

    BOOST_CHECK_EQUAL(error("assertion failure with message: This message already exists."), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));
    BOOST_CHECK_EQUAL(error("assertion failure with message: unregistered user: dan.larimer"), golos_publication_tester::create_message(
                            N(dan.larimer),
                            "Hi"));
    
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(update_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Update message testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::update_message(
                            N(brucelee),
                            "permlink",
                            "headermssgnew",
                            "bodymssgnew",
                            "languagemssgnew",
                            {{"tagnew"}},
                            "jsonmetadatanew"));

    auto content_stats = get_content(N(brucelee), hash64("permlink"));
    check_equal_content(content_stats, mvo()
                            ("id", hash64("permlink"))
                            ("headermssg", "headermssgnew")
                            ("bodymssg", "bodymssgnew")
                            ("languagemssg", "languagemssgnew")
                            ("tags", variants({
                                                  mvo()
                                                  ("tag", "tagnew")
                                              }))
                            ("jsonmetadata", "jsonmetadatanew"));

    BOOST_CHECK_EQUAL(error("assertion failure with message: Content doesn't exist."), golos_publication_tester::update_message(
                            N(brucelee),
                            "permlinknew",
                            "headermssgnew",
                            "bodymssgnew",
                            "languagemssgnew",
                            {{"tagnew"}},
                            "jsonmetadatanew"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(delete_message, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Delete message testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(jackiechan),
                            "permlink1",
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(error("assertion failure with message: You can't delete comment with child comments."), golos_publication_tester::delete_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::delete_message(
                            N(jackiechan),
                            "permlink1"));

    BOOST_CHECK_EQUAL(error("assertion failure with message: Message doesn't exist."), golos_publication_tester::delete_message(
                            N(jackiechan),
                            "permlink1"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::delete_message(
                            N(brucelee),
                            "permlink"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(upvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Upvote testing.");
    init();
    
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Vote with the same weight has already existed."), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Message doesn't exist."), golos_publication_tester::upvote(
                            N(brucelee),
                            N(jackiechan),
                            "permlink",
                            111));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight must be positive."), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            -333));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight must be positive."), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            0));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            10000));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight can't be more than 100%."), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            10001));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            111));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            222));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            333));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            444));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            555));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    auto vote_stats = get_vote(N(brucelee), 1);
    BOOST_CHECK_EQUAL(fc::variant(vote_stats).get_object()["count"].as<int64_t>(), 5);

    BOOST_CHECK_EQUAL(error("assertion failure with message: You can't revote anymore."), golos_publication_tester::upvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            777));
    produce_blocks(CLOSE_MESSAGE_PERIOD*2 - VOTE_OPERATION_INTERVAL*2*12-6);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                          N(jackiechan),
                          N(brucelee),
                          "permlink",
                          777));

    {
        auto vote_stats = get_vote(N(brucelee), 1);
        BOOST_CHECK_EQUAL(fc::variant(vote_stats).get_object()["count"].as<int64_t>(), -1);
    }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(disable_upvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Disable upvote testing.");
    init();
    
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));
    produce_blocks((CLOSE_MESSAGE_PERIOD - UPVOTE_DISABLE_PERIOD)*2-1);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            888));
        produce_blocks(2);
        BOOST_CHECK_EQUAL(error("assertion failure with message: You can't upvote, because publication will be closed soon."), golos_publication_tester::upvote(
                                N(jackiechan),
                                N(brucelee),
                                "permlink",
                                777));
        produce_blocks(UPVOTE_DISABLE_PERIOD*2-3);

        BOOST_CHECK_EQUAL(error("assertion failure with message: You can't upvote, because publication will be closed soon."), golos_publication_tester::upvote(
                                N(brucelee),
                                N(brucelee),
                                "permlink",
                                777));
        produce_blocks(1);

        BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                                N(jackiechan),
                                N(brucelee),
                                "permlink",
                                888));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(downvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Downvote testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Vote with the same weight has already existed."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Message doesn't exist."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(jackiechan),
                            "permlink",
                            111));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight sign can't be negative."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            -333));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight sign can't be negative."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            0));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            10000));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(error("assertion failure with message: The weight can't be more than 100%."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            10001));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            111));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            222));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            333));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            444));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            555));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    auto vote_stats = get_vote(N(brucelee), 1);
    BOOST_CHECK_EQUAL(fc::variant(vote_stats).get_object()["count"].as<int64_t>(), 5);

    BOOST_CHECK_EQUAL(error("assertion failure with message: You can't revote anymore."), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            777));
    produce_blocks(CLOSE_MESSAGE_PERIOD*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(jackiechan),
                            N(brucelee),
                            "permlink",
                            777));

    {
        auto vote_stats = get_vote(N(brucelee), 1);
        BOOST_CHECK_EQUAL(fc::variant(vote_stats).get_object()["count"].as<int64_t>(), -1);
    }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(unvote, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Unvote testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::unvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink"));

    {
        BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                                N(brucelee),
                                N(brucelee),
                                "permlink",
                                333));
        produce_blocks(VOTE_OPERATION_INTERVAL*2);

        BOOST_CHECK_EQUAL(success(), golos_publication_tester::unvote(
                                N(brucelee),
                                N(brucelee),
                                "permlink"));
    }

    produce_blocks(VOTE_OPERATION_INTERVAL*2);
    BOOST_CHECK_EQUAL(error("assertion failure with message: Vote with the same weight has already existed."), golos_publication_tester::unvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(error("assertion failure with message: Message doesn't exist."), golos_publication_tester::unvote(
                            N(brucelee),
                            N(jackiechan),
                            "permlink1"));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(mixed_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Mixed vote testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            321));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            333));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    auto vote_stats = get_vote(N(brucelee), 0);
    BOOST_CHECK_EQUAL(fc::variant(vote_stats).get_object()["count"].as<uint64_t>(), 3);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(erase_vote_test, golos_publication_tester) try {
    BOOST_TEST_MESSAGE("Erase vote testing.");
    init();

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink"));
    
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_message(
                            N(brucelee),
                            "permlink1"));

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::downvote(
                            N(chucknorris),
                            N(brucelee),
                            "permlink1",
                            321));
    produce_blocks(VOTE_OPERATION_INTERVAL*2);
    
    BOOST_CHECK_EQUAL(success(), golos_publication_tester::delete_message(
                            N(brucelee),
                            "permlink1"));
    produce_blocks(1);
    
    BOOST_CHECK_EQUAL(error("assertion failure with message: Vote with the same weight has already existed."), golos_publication_tester::downvote(
                            N(brucelee),
                            N(brucelee),
                            "permlink",
                            123));

    
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
