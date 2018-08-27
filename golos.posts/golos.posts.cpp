#include "golos.posts.hpp"

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        posts(receiver).apply(code, action);
    }
}

posts::posts(account_name self)
    : eosio::contract(self)
    , _table_posts(_self, _self)
    , _table_votes(_self, _self)
{}

void posts::apply(uint64_t code, uint64_t action) {
    if( N(crtpost) == action )
        create_post(unpack_action_data<structures::create_post>());
    else if ( N(updtpost) == action )
        update_post(unpack_action_data<structures::update_post>());
    else if ( N(rmvpost) == action )
        remove_post(unpack_action_data<structures::remove_post>());
    else if ( N(upvotepost) == action )
        upvote_post(unpack_action_data<structures::vote>());
    else if ( N(downvotepost) == action )
        downvote_post(unpack_action_data<structures::vote>());
}

void posts::create_post(const structures::create_post &post) {
    require_auth(post.autor);

    _table_posts.emplace(post.autor, [&](structures::post &item){
         item.id = _table_posts.available_primary_key();
         item.autor = post.autor;
         item.permlink = post.permlink;
         item.parent_id = post.parent_id;
         item.beneficiaries = post.beneficiaries;
         item.without_payment = post.without_payment;
         item.date_create = time_point_sec(now());
         item.last_update = time_point_sec(now());

//         item.shares = post.shares;
//         item.total_weight =
//         item.share_token
    });
}

void posts::update_post(const structures::update_post &post) {
    auto it_post = _table_posts.find(post.id);
    eosio_assert(it_post != _table_posts.end(), "Not found post");
    eosio_assert(it_post->autor != post.autor, "Not valid autor");
    eosio_assert(it_post->permlink != post.permlink, "Not valid permlink");

    require_auth(post.autor);

    _table_posts.modify(it_post, 0, [&](structures::post &item){
        item.without_payment = post.without_payment;
        item.beneficiaries = post.beneficiaries;
        item.root = post.root;

        item.last_update = time_point_sec(now());
    });
}

void posts::remove_post(const structures::remove_post &post) {
    require_auth(post.autor);

    auto it_post = _table_posts.find(post.id_post);
    eosio_assert(it_post != _table_posts.end(), "Not fount this post");
    eosio_assert(post.permlink != it_post->permlink, "Does not match permlink");

    _table_posts.erase(it_post);
}

void posts::upvote_post(const structures::vote &vote) {
    require_auth(vote.account);

    auto index = _table_votes.get_index<N(autor)>();
    auto it_autor = index.find(vote.autor);
    while(it_autor != index.end() && it_autor->autor == vote.autor) {

        eosio_assert(it_autor->permlink == vote.permlink && it_autor->account == vote.account,
                     "This voice already exists");

        ++it_autor;
    }

    _table_votes.emplace(vote.account, [&](structures::table_vote &item){
        item.id =  _table_votes.available_primary_key();
        item.autor = vote.autor;
        item.account = vote.account;
        item.permlink = vote.permlink;
    });
}

void posts::downvote_post(const structures::vote &vote) {
    require_auth(vote.account);

    auto index = _table_votes.get_index<N(autor)>();
    auto it_autor = index.find(vote.autor);
    while(it_autor != index.end() && it_autor->autor == vote.autor) {
        if (it_autor->permlink == vote.permlink && it_autor->account == vote.account) {
            index.erase(it_autor);
            break;
        }

        ++it_autor;
    }
}
