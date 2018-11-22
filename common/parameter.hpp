#pragma once
#include <eosiolib/eosio.hpp>
#include <variant>

namespace golos {

template<typename T, int L>
struct param_wrapper: T {
    param_wrapper() = default;
    param_wrapper(T& x): T(x) {};
    param_wrapper(T&& x): T(std::move(x)) {};
    operator T() { return this; }
};


// Base to inherit other parameters
struct parameter {

    // validates parameter value(s), throws assert if invalid
    virtual void validate() const {};

    // validates change from previous to new values
    // virtual void validate(const parameter& prev) const = 0;

    // compares parameters to use in median calculation
    // returns true by default, which means no comparer set, override if can use median
    virtual bool operator<(const parameter& other) const { return true; };

    // used to detect is parameter can be used in median.
    // by default checks if default comparer set, this means parameter can't be used in median
    virtual bool can_median() const { return !(this < this); }
};

struct param_helper {
    template<typename T>
    static void check_params(const std::vector<T>& params) {
        eosio::print("check_params, size: ", params.size(), "\n");
        eosio_assert(params.size(), "empty params not allowed");
        check_dups(params);
        eosio::print("check_params: no dups\n");
        validate_params(params);
        eosio::print("check_params: valid\n");
    }

    template<typename T>
    static void validate_params(const std::vector<T>& params) {
        for (const auto& param: params) {
            std::visit([](const parameter& p) {
                p.validate();
            }, param);
        }
    }

    template<typename T>
    static void check_dups(const std::vector<T>& params) {
        using idx_type = decltype(params[0].index());
        std::set<idx_type> types;
        bool first = true;
        idx_type prev_idx = 0;
        for (const auto& p: params) {
            auto i = p.index();
            if (!first) {
                eosio_assert(i > prev_idx, "parameters must be ordered by variant index");
            }
            eosio_assert(types.count(i) == 0, "params contain several copies of the same parameter");
            types.emplace(i);
            prev_idx = i;
            first = false;
        }
    }

    // TODO: helper to get values from top witnesses
    template<typename T>
    static T get_median(std::vector<T>& params) {
        auto l = params.size();
        eosio_assert(l > 0, "empty params not allowed");
        T r = params[0];
        eosio_assert(r.can_median(), "parameter can't be used as median");   // must not normally happen
        if (l > 1) {
            std::nth_element(params.begin(), params.begin() + l / 2, params.end());
            r = params[l / 2];
        }
        return r;
    }

    template<typename T>
    static bool have_majority(const std::vector<T>& params, size_t min_count, T& major) {
        auto l = params.size();
        eosio_assert(l > 0, "empty params not allowed");
        // eosio_assert(l >= min_count, "min_count is too large to reach");
        if (l < min_count) {
            return false; //min_count is too large to reach
        }
        eosio_assert(l/2 + 1 <= min_count, "min_count is too small, it must be at least 50%+1");
        std::map<T,int> same;
        int max_count = -1;
        T max_value;
        for (const auto& p: params) {
            auto count = same[p];
            count++;
            same[p] = count;
            if (count > max_count) {
                max_count = count;
                max_value = p;
                if (count >= min_count) {
                    major = max_value;
                    return true;
                }
            }
            if (l + max_count < min_count) {
                return false;   // no chance to reach min_count
            }
            l--;
        }
        return false;
    }

    template<typename P, typename T, typename S, typename F>
    static bool update_state(T& top, S& state, F field, int threshold) {
        std::vector<P> param_top(top.size());
        std::transform(top.begin(), top.end(), param_top.begin(), [&](const auto& p) {
            return p.*field;
        });
        P major;
        bool majority = param_helper::have_majority(param_top, threshold, major);
        if (majority && major != state.*field) {
            state.*field = major;
            return true;
        }
        return false;
    }

};


} // golos
