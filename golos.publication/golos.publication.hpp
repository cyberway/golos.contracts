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
                     std::vector<structures::beneficiary> beneficiaries,
                     std::string headerpost, std::string bodypost,
                     std::string languagepost, std::vector<structures::tag> tags,
                     std::string jsonmetadata);

    void update_post(account_name account, std::string permlink,
                     std::string headerpost, std::string bodypost,
                     std::string languagepost, std::vector<structures::tag> tags,
                     std::string jsonmetadata);
    void delete_post(account_name account, std::string permlink);
    void upvote(account_name voter, account_name author, std::string permlink, int16_t weight);
    void downvote(account_name voter, account_name author, std::string permlink, int16_t weight);
    void unvote(account_name voter, account_name author, std::string permlink);
    void close_post(account_name account, std::string permlink);
    void close_post_timer(account_name account, std::string permlink);
    void set_vote(account_name voter, account_name author, std::string permlink, int16_t weight);
    template<typename Lambda>
    bool get_post(account_name account, std::string &permlink, structures::post &post, Lambda &&lambda);
    template<typename Lambda>
    bool get_post(account_name account, checksum256 &permlink, structures::post &post, Lambda &&lambda);
    checksum256 get_checksum256(std::string &permlink);
    void set_child_count(bool increase, account_name parentacc, checksum256 &parentprmlnk);

  private:
    void create_acc(account_name name);
};

}

