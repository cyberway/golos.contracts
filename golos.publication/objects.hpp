#pragma once
#include "config.hpp"

#include <eosiolib/eosio.hpp>

using namespace eosio;

namespace structures {

struct beneficiary {
    beneficiary() = default;

    account_name account;
    uint64_t deductprcnt;
};

struct tag {
    tag() = default;

    std::string tag_name;
};

struct content {
    content() = default;

    uint64_t id;
    std::string headerpost;
    std::string bodypost;
    std::string languagepost;
    std::vector<structures::tag> tags;
    std::string jsonmetadata;

    uint64_t primary_key() const {
        return id;
    }

    EOSLIB_SERIALIZE(content, (id)(headerpost)(bodypost)(languagepost)
                     (tags)(jsonmetadata))
};

struct createpost {
    createpost() = default;

    account_name account;
    std::string permlink;
    std::string parentid;
    std::string parentprmlnk;
    uint64_t curatorprcnt;
    std::string payouttype;
    std::vector<structures::beneficiary> beneficiaries;
    std::string paytype;
};

struct post : createpost {
    post() = default;

    uint64_t id;
    uint64_t date;
//    std::string withoutpayout;
//    uint64_t tokenshare;


    uint64_t primary_key() const {
        return id;
    }

    account_name account_key() const {
        return account;
    }

    EOSLIB_SERIALIZE(post, (id)(date)(account)(permlink)(parentid)(parentprmlnk)(curatorprcnt)(payouttype)
                     (beneficiaries)(paytype))
};

}

namespace tables {
    using id_index = indexed_by<N(posttable), const_mem_fun<structures::post, uint64_t, &structures::post::primary_key>>;
    using account_index = indexed_by<N(posttable), const_mem_fun<structures::post, account_name, &structures::post::account_key>>;
    using post_table = eosio::multi_index<N(posttable), structures::post, id_index, account_index>;
    using content_table = eosio::multi_index<N(contenttable), structures::content>;
}
