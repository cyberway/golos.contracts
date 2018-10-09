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
    void delete_post(account_name account, std::string permlink);
    void upvote(account_name voter, account_name author, std::string permlink, asset weight);
    void downvote(account_name voter, account_name author, std::string permlink, asset weight);
    void unvote(account_name voter, account_name author, std::string permlink);
    void close_post();
    void close_post_timer();
    void set_vote(account_name voter, account_name author, std::string permlink, asset weight);
    bool get_post(account_name account, std::string permlink, structures::post &post);

  private: // check battery limits
    void create_battery_user(account_name name);

    void limit_posts(scope_name scope);
    void limit_comments(scope_name scope);
    void limit_of_votes(scope_name scope);
    void posting_battery(scope_name scope);
    void vesting_battery(scope_name scope, uint16_t persent_battery);
};

}

