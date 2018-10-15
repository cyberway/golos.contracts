#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/permission_object.hpp>
#include <eosio/chain/authorization_manager.hpp>

#include <Runtime/Runtime.h>
#include <fc/variant_object.hpp>

#include "golos_tester.hpp"
#include <golos.publication/golos.publication.wast.hpp>
#include <golos.publication/golos.publication.abi.hpp>

#define UNLOCKED_FOR_CREATE_POST 21

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

namespace eosio::structures {
    struct beneficiary {
        account_name account;
        uint64_t deductprcnt;
    };

    struct tags {
        std::string tag;
    };
}
FC_REFLECT(eosio::structures::tags, (tag))
FC_REFLECT(eosio::structures::beneficiary, (account)(deductprcnt))


class golos_publication_tester : public golos_tester {
public:

    golos_publication_tester() {
        produce_blocks(2);

        create_accounts({N(jackiechan), N(brucelee), N(chucknorris), N(golos.pub)});
        produce_blocks(2);

        set_code(N(golos.pub), golos_publication_wast);
        set_abi(N(golos.pub), golos_publication_abi);

        produce_blocks();

        const auto& accnt = control->db().get<account_object, by_name>(N(golos.pub));
        abi_def abi;
        BOOST_CHECK_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);
    }

    action_result push_action(const account_name& signer,
                              const action_name &name,
                              const variant_object &data) {
       string action_type_name = abi_ser.get_action_type(name);

       action act;
       act.account = N(golos.pub);
       act.name = name;
       act.data = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);

       return base_tester::push_action(std::move(act), uint64_t(signer));
    }

    action_result create_post(account_name account, std::string permlink,
                              account_name parentacc, std::string parentprmlnk,
                              uint64_t curatorprcnt, std::string payouttype,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string paytype, std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tags> tags,
                              std::string jsonmetadata) {
        return push_action(account, N(createpost), mvo()
                           ("account", account)
                           ("permlink", permlink)
                           ("parentacc", parentacc)
                           ("parentprmlnk", parentprmlnk)
                           ("curatorprcnt", curatorprcnt)
                           ("payouttype", payouttype)
                           ("beneficiaries", beneficiaries)
                           ("paytype", paytype)
                           ("headerpost", headerpost)
                           ("bodypost", bodypost)
                           ("languagepost", languagepost)
                           ("tags", tags)
                           ("jsonmetadata", jsonmetadata)
        );
    }

    fc::variant get_posts( account_name acc, uint64_t id ) {
        return get_tbl_struct(N(golos.pub), acc, N(posttable), id, "post", abi_ser);
    }

    fc::variant get_battery( account_name acc) {
        return get_tbl_struct_singleton(N(golos.pub), acc, N(batterytable), "account_battery", abi_ser);
    }

    action_result update_post(account_name account, std::string permlink,
                              std::string headerpost, std::string bodypost,
                              std::string languagepost, std::vector<structures::tags> tags,
                              std::string jsonmetadata) {
        return push_action(N(golos.pub), N(updatepost), mvo()
                           ("account", account)
                           ("permlink", permlink)
                           ("headerpost", headerpost)
                           ("bodypost", bodypost)
                           ("languagepost", languagepost)
                           ("tags",tags)
                           ("jsonmetadata", jsonmetadata)
        );
    }

    action_result delete_post(account_name account, std::string permlink) {
        return push_action(N(golos.pub), N(deletepost), mvo()
                           ("account", account)
                           ("permlink", permlink)
        );
    }

    action_result upvote(account_name voter, account_name author, std::string permlink, asset weight) {
        return push_action(voter, N(upvote), mvo()
                           ("voter", voter)
                           ("author", author)
                           ("permlink", permlink)
                           ("weight", weight)
        );
    }

    action_result downvote(account_name voter, account_name author, std::string permlink, asset weight) {
        return push_action(N(golos.pub), N(downvote), mvo()
                           ("voter", voter)
                           ("author", author)
                           ("permlink", permlink)
                           ("weight", weight)
        );
    }

    action_result create_battery(account_name account) {
        return push_action(account, N(createacc), mvo()
                           ("name", account)
                          );
    }

    void require_equal(const fc::variant &obj1, const fc::variant &obj2) {
        BOOST_CHECK_EQUAL(true, obj1.is_object() && obj2.is_object());
        BOOST_CHECK_EQUAL(obj1.get_object()["id"].as<uint64_t>(), obj2.get_object()["id"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["account"].as<account_name>(), obj2.get_object()["account"].as<account_name>());
//        BOOST_CHECK_EQUAL(obj1.get_object()["permlink"].as<checksum_type>(), obj2.get_object()["permlink"].as<checksum_type>());
        BOOST_CHECK_EQUAL(obj1.get_object()["parentacc"].as<account_name>(), obj2.get_object()["parentacc"].as<account_name>());
        BOOST_CHECK_EQUAL(obj1.get_object()["parentprmlnk"].as<std::string>(), obj2.get_object()["parentprmlnk"].as<std::string>());
        BOOST_CHECK_EQUAL(obj1.get_object()["curatorprcnt"].as<uint64_t>(), obj2.get_object()["curatorprcnt"].as<uint64_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["payouttype"].as<std::string>(), obj2.get_object()["payouttype"].as<std::string>());
        BOOST_CHECK_EQUAL(obj1.get_object()["paytype"].as<std::string>(), obj2.get_object()["paytype"].as<std::string>());
    }

    void require_equal_battery(const fc::variant &obj1, const fc::variant &obj2) {
        BOOST_CHECK_EQUAL(true, obj1.is_object() && obj2.is_object());
        BOOST_CHECK_EQUAL(obj1.get_object()["posting_battery"].get_object()["charge"].as<uint16_t>(),        obj2.get_object()["posting_battery"].get_object()["charge"].as<uint16_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["battery_of_votes"].get_object()["charge"].as<uint16_t>(),       obj2.get_object()["battery_of_votes"].get_object()["charge"].as<uint16_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["limit_battery_posting"].get_object()["charge"].as<uint16_t>(),  obj2.get_object()["limit_battery_posting"].get_object()["charge"].as<uint16_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["limit_battery_comment"].get_object()["charge"].as<uint16_t>(),  obj2.get_object()["limit_battery_comment"].get_object()["charge"].as<uint16_t>());
        BOOST_CHECK_EQUAL(obj1.get_object()["limit_battery_of_votes"].get_object()["charge"].as<uint16_t>(), obj2.get_object()["limit_battery_of_votes"].get_object()["charge"].as<uint16_t>());
    }

    abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(golos_publication_tests)

BOOST_FIXTURE_TEST_CASE(create_post, golos_publication_tester) try {
    BOOST_CHECK_EQUAL(success(), create_battery(N(jackiechan)));
    produce_blocks(1);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    auto stats = get_posts(N(jackiechan), 0);
    require_equal( stats, mvo()
                   ("id", 0)
                   ("date", 1577836804)
                   ("account", "jackiechan")
//                   ("permlink", "permlink")
                   ("parentacc", "")
                   ("parentprmlnk", "")
                   ("curatorprcnt", 333)
                   ("payouttype", "payouttype")
                   ("paytype", "paytype")
                   );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_create_post_and_battery_limit_post, golos_publication_tester) try {
    BOOST_CHECK_EQUAL(success(), create_battery(N(jackiechan)));
    produce_blocks(1);

    auto account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 0))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 0))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink0",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 2500))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 10000))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );
    produce_blocks(10);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Battery overrun"), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink1",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 2500))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 10000))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );
    produce_blocks(UNLOCKED_FOR_CREATE_POST);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink2",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));
    produce_blocks(UNLOCKED_FOR_CREATE_POST);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink3",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));
    produce_blocks(UNLOCKED_FOR_CREATE_POST);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink4",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 10000))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 10000))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    produce_blocks(UNLOCKED_FOR_CREATE_POST);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post( // TODO no check in smart-contract
                            N(jackiechan),
                            "permlink5",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 12499))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 10000))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_create_comment_and_battery_limit_comment, golos_publication_tester) try {
    BOOST_CHECK_EQUAL(success(), create_battery(N(jackiechan)));
    produce_blocks(1);

    auto account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 0))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 0))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink0",
                            N(jackiechan),
                            "permlink",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery = get_battery(N(jackiechan));
    require_equal_battery(account_battery, mvo()
                          ("posting_battery", mvo()("charge", 0))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 0))
                          ("limit_battery_comment", mvo()("charge", 10000))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );
    produce_blocks(10);

    BOOST_CHECK_EQUAL(error("assertion failure with message: Battery overrun"), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink1",
                            N(jackiechan),
                            "permlink0",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));
    produce_blocks(30);

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(jackiechan),
                            "permlink2",
                            N(jackiechan),
                            "permlink0",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_upvote_and_battery_limit_vote, golos_publication_tester) try {
    BOOST_CHECK_EQUAL(success(), create_battery(N(brucelee)));
    BOOST_CHECK_EQUAL(success(), create_battery(N(chucknorris)));
    produce_blocks(1);

    auto account_battery_brucelee = get_battery(N(brucelee));
    require_equal_battery(account_battery_brucelee, mvo()
                          ("posting_battery", mvo()("charge", 0))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 0))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    auto account_battery_chucknorris = get_battery(N(chucknorris));
    require_equal_battery(account_battery_chucknorris, mvo()
                          ("posting_battery", mvo()("charge", 0))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 0))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::create_post(
                            N(brucelee),
                            "permlink0",
                            N(),
                            "",
                            333,
                            "payouttype",
                            {{N(beneficiary), 777}},
                            "paytype",
                            "headerpost",
                            "bodypost",
                            "languagepost",
                            {{"tag"}},
                            "jsonmetadata"));

    account_battery_brucelee = get_battery(N(brucelee));
    require_equal_battery(account_battery_brucelee, mvo()
                          ("posting_battery", mvo()("charge", 2500))
                          ("battery_of_votes", mvo()("charge", 0))
                          ("limit_battery_posting", mvo()("charge", 10000))
                          ("limit_battery_comment", mvo()("charge", 0))
                          ("limit_battery_of_votes", mvo()("charge", 0))
                          );

    BOOST_CHECK_EQUAL(success(), golos_publication_tester::upvote(N(chucknorris), N(brucelee), "permlink0", asset::from_string("500.0000 GOLOS")));
    BOOST_CHECK_EQUAL(error("assertion failure with message: Battery overrun"), golos_publication_tester::upvote(N(chucknorris), N(brucelee), "permlink0", asset::from_string("500.0000 GOLOS")));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
