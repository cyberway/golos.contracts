#pragma once
#include <eosiolib/eosio.hpp>
#include "types.h"
#include "config.hpp"
#include <common/calclib/atmsp_storable.h>
#include "utils.hpp"
namespace structures {
struct limits {
    enum kind_t: enum_t {POST, COMM, VOTE, POSTBW, UNDEF};
    std::vector<atmsp::storable::bytecode> restorers;//(funcs of: prev_charge (p), vesting (v), elapsed_seconds (t))
    std::vector<limitedact> limitedacts;
    std::vector<int64_t> vestingprices;//disabled if < 0
    std::vector<int64_t> minvestings;

    const limitedact& get_limited_act(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        eosio_assert(k < limitedacts.size(), "limits: undefined kind");
        return limitedacts[k];
    }

    const atmsp::storable::bytecode* get_restorer(kind_t kind)const {
        uint8_t num = get_limited_act(kind).restorernum;
        if(num < restorers.size())
            return &(restorers[num]);
        eosio_assert(num == disabled_restorer, "limits: wrong restorer num");
        return nullptr;
    }

    int64_t get_vesting_price(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        return (k < vestingprices.size()) ? vestingprices[k] : -1;
    }

    int64_t get_min_vesting_for(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        eosio_assert(k < minvestings.size(), "limits: wrong act type");
        return minvestings[k];
    }

    size_t get_charges_num()const {
        size_t ret = 0;
        for(auto& act : limitedacts)
            ret = std::max(ret, static_cast<size_t>(act.chargenum) + 1);
        return ret;
    }

    void check() {
        eosio_assert(vestingprices.size() < 3, "limits::check: vesting payment may be available only for messages");
        eosio_assert(minvestings.size() == 3, "limits::check: undefined min vesting value");
        for(auto& act : limitedacts)
            eosio_assert((act.restorernum < restorers.size()) || (act.restorernum == disabled_restorer), "limits::check: wrong restorer num");
    }
};

struct chargeinfo {
    uint64_t lastupdate;
    base_t data;

    void update(atmsp::machine<fixp_t>& machine, const atmsp::storable::bytecode* restorer, uint64_t cur_time, fixp_t vesting, fixp_t price, elaf_t w = elaf_t(1)) {
        if(restorer != nullptr) {
            int64_t elapsed_seconds = static_cast<int64_t>((cur_time - lastupdate) / eosio::seconds(1).count());
            auto prev = FP(data);

            using namespace golos::config::limit_restorer_domain;
            auto restored = set_and_run(machine, *restorer,
                {prev, vesting, fp_cast<fixp_t>(elapsed_seconds, false)},
                {{fixp_t(0), max_prev}, {fixp_t(0), max_vesting}, {fixp_t(0), max_elapsed_seconds}});

            if(restored > prev)
                data = fixp_t(0).data();
            else
                data = std::min(fp_cast<fixp_t>(elap_t(FP(data)) - elap_t(restored), false), max_res).data();
        }
        if(price > fixp_t(0)) {
            auto added = std::max(fp_cast<fixp_t>(elap_t(price) * w, false), std::numeric_limits<fixp_t>::min());
            data = std::min(fp_cast<fixp_t>(elap_t(FP(data)) + elap_t(added), false), golos::config::limit_restorer_domain::max_res).data();
        }
        lastupdate = cur_time;
    }
};

struct usercharges {
    uint64_t poolcreated; //charges depend on a pool
    std::vector<chargeinfo> vals;

    uint64_t primary_key() const { return poolcreated; }

    elaf_t calc_power(atmsp::machine<fixp_t>& machine, const limits& lims, limits::kind_t kind, uint64_t cur_time, fixp_t vesting, elaf_t w = elaf_t(1)) {
        auto& lim_act = lims.get_limited_act(kind);
        auto restorer = lims.get_restorer(kind);
        eosio_assert(vals.size() > lim_act.chargenum, "usercharges::update: LOGIC ERROR! wrong charge num");
        auto& chg = vals[lim_act.chargenum];
        eosio_assert(chg.lastupdate <= cur_time, "usercharges::update: LOGIC ERROR! chg.lastupdate > cur_time");
        chg.update(machine, restorer, cur_time, vesting, FP(lim_act.chargeprice), w);
        elap_t cutoff = FP(lim_act.cutoffval);
        elap_t charge = FP(chg.data);
        return (charge > cutoff) ? elaf_t(cutoff / charge) : elaf_t(1);
    }
};
}
