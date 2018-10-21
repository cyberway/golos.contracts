#pragma once
#include "objects.hpp"

namespace eosio {

class publication : public eosio::contract {
  public:
    publication(account_name self);
    void apply(uint64_t code, uint64_t action);

  private:
    void create_message(account_name account, std::string permlink,
                     account_name parentacc, std::string parentprmlnk,
                     std::vector<structures::beneficiary> beneficiaries,
                     std::string headermssg, std::string bodymssg,
                     std::string languagemssg, std::vector<structures::tag> tags,
                     std::string jsonmetadata);
    void update_message(account_name account, std::string permlink,
                     std::string headermssg, std::string bodymssg,
                     std::string languagemssg, std::vector<structures::tag> tags,
                     std::string jsonmetadata);
    void delete_message(account_name account, std::string permlink);
    void upvote(account_name voter, account_name author, std::string permlink, int16_t weight);
    void downvote(account_name voter, account_name author, std::string permlink, int16_t weight);
    void unvote(account_name voter, account_name author, std::string permlink);
    void create_acc(account_name name);
 
    void close_message(account_name account, uint64_t id);
    void close_message_timer(account_name account, uint64_t id);
    void set_vote(account_name voter, account_name author, uint64_t id, int16_t weight);
    void set_child_count(bool increase, account_name parentacc, uint64_t parent_id);
};

}

