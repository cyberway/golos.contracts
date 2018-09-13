#include "golos.publication.hpp"
#include <eosiolib/transaction.hpp>

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        publication(receiver).apply(code, action);
    }
}

publication::publication(account_name self)
    : contract(self)
    , _post_table(_self, _self)
    , _content_table(_self, _self)
{}


void publication::apply(uint64_t code, uint64_t action) {
    if (N(createpost) == action)
        execute_action(this, &publication::create_post);
    if (N(updatepost) == action)
        execute_action(this, &publication::update_post);
    if (N(deletepost) == action)
        execute_action(this, &publication::delete_post);
//    if (N(upvote) == action)
//        execute_action(this, &publication::upvote);
//    if (N(downvote) == action)
//        execute_action(this, &publication::downvote);
}

void publication::create_post(account_name account, std::string permlink,
                              std::string parentid, std::string parentprmlnk,
                              uint64_t curatorprcnt, std::string payouttype,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string paytype, std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {

    require_auth(account);

    _post_table.emplace(account, [&]( auto &item ) {
        item.id = _post_table.available_primary_key();
        item.date = now();
        item.account = account;
        item.permlink = permlink;
        item.parentid = parentid;
        item.parentprmlnk = parentprmlnk;
        item.curatorprcnt = curatorprcnt;
        item.payouttype = payouttype;
        item.beneficiaries = beneficiaries;
        item.paytype = paytype;
    });


    _content_table.emplace(account, [&]( auto &item ) {
        item.id = _content_table.available_primary_key();
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::update_post(account_name account, std::string permlink,
                              std::string parentid, std::string parentprmlnk,
                              uint64_t curatorprcnt, std::string payouttype,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string paytype, std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    auto posttable_obj = _post_table.find(account);

    eosio_assert(posttable_obj != _post_table.end(), "Post doesn't exist.");
    eosio_assert(posttable_obj->account != account, "Account doesn't exist.");
    eosio_assert(posttable_obj->permlink != permlink, "Permlink is incorrect.");

    _post_table.modify(posttable_obj, account, [&]( auto &item ) {
        item.permlink = permlink;
        item.parentid = parentid;
        item.parentprmlnk = parentprmlnk;
        item.curatorprcnt = curatorprcnt;
        item.payouttype = payouttype;
        item.beneficiaries = beneficiaries;
        item.paytype = paytype;
    });

    auto contenttable_obj = _content_table.find(account);

    _content_table.modify(contenttable_obj, account, [&]( auto &item ) {
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::delete_post(account_name account, std::string permlink) {
    require_auth(account);

    auto posttable_obj = _post_table.find(account);

    eosio_assert(posttable_obj != _post_table.end(), "Post doesn't exist.");
    eosio_assert(permlink != posttable_obj->permlink, "Permlink is incorrect.");

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj)
        if (posttable_obj->account == account && posttable_obj->permlink == permlink) {
            if (posttable_obj->parentid != "") {
                for (auto obj = _post_table.begin(); obj != _post_table.end(); ++obj) {
                    if (obj->id == posttable_obj->parentid)
                        _post_table.erase(obj);
                }
                _post_table.erase(posttable_obj);
                return;
            }
        }
}

