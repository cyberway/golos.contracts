#pragma once
#include "objects.hpp"

namespace eosio {

class posts : public eosio::contract {
public:
    posts(account_name self);
    void apply(uint64_t code, uint64_t action);

    void create_post(const structures::create_post &post);
    void update_post(const structures::update_post &post);
    void remove_post(const structures::remove_post &m_post);

    void upvote_post(const structures::vote &vote);
    void downvote_post(const structures::vote &vote);

private:
    tables::posts_table _table_posts;
    tables::votes_table _table_votes;
};
}
