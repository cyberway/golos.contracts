#pragma once
#include <eosiolib/crypto.hpp>

namespace golos {


uint64_t hash64(const char* buf, size_t len) {
    ::capi_checksum256 hash;
    ::sha256(buf, len, &hash);
    return *(reinterpret_cast<const uint64_t *>(&hash));
}

uint64_t hash64(const std::string& arg) {
    return hash64(arg.c_str(), arg.size());
}


} // golos
