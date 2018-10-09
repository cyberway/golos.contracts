#pragma once
#include "objects.hpp"

namespace eosio {

class publication : public eosio::contract {
  public:
    publication(account_name self);
    void apply(uint64_t code, uint64_t action);

  private:
    void create_post(account_name account, std::string permlink,
                     account_name parentacc, std::string parentprmlnk,
                     uint64_t curatorprcnt, std::string payouttype,
                     std::vector<structures::beneficiary> beneficiaries, std::string paytype,
                     std::string headerpost, std::string bodypost,
                     std::string languagepost, std::vector<structures::tag> tags,
                     std::string jsonmetadata);

    void update_post(account_name account, std::string permlink,
                     std::string headerpost, std::string bodypost,
                     std::string languagepost, std::vector<structures::tag> tags,
                     std::string jsonmetadata);
    void delete_post(account_name account, std::string permlink, account_name parentacc, std::string parentprmlnk);
    void upvote(account_name voter, account_name author, std::string permlink, int64_t weight);
    void downvote(account_name voter, account_name author, std::string permlink, int64_t weight);
    void unvote(account_name voter, account_name author, std::string permlink);
    void close_post(account_name account, std::string permlink);
    void close_post_timer(account_name account, std::string permlink);
    void set_vote(account_name voter, account_name author, std::string permlink, int64_t weight);
    bool get_post(account_name account, std::string permlink, structures::post &post);
    checksum256 get_checksum256(std::string &permlink);
    void set_child_count(bool state, account_name parentacc, std::string &parentprmlnk);

  private: // check battery limits
    void create_battery_user(account_name name);

    void limit_posts(scope_name scope);
    void limit_comments(scope_name scope);
    void limit_of_votes(scope_name scope);
    void posting_battery(scope_name scope);
    void vesting_battery(scope_name scope, uint16_t persent_battery);
};

}

