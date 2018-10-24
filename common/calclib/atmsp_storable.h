#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include "atmsp.h"

using namespace fixed_point_utils;
namespace atmsp { namespace storable {   
using index_t = uint64_t;
struct [[eosio::table]] value { 
    enum kind_t: enum_t {NUM, VAR, CONST, UNDEF};
    enum_t kind = UNDEF;
    index_t idx = 0;
};
 
struct [[eosio::table]] bytecode {
    index_t varssize;
    std::vector<index_t> operators;
    std::vector<value> values;   
    std::vector<base_t> nums;
    std::vector<base_t> consts;
        
    std::string get_str()const {
        std::string ret("operators: ");
        for(auto i : operators)
            ret += std::to_string(i) + ("; ");
        ret += "\nvalues: ";
        for(auto i : values)
            ret += std::to_string(i.kind) + ":" + std::to_string(i.idx) + ("; ");
        ret += "\nnums: ";
        for(auto i : nums)
            ret += std::to_string(static_cast<double>(fixp_t::from_data(i))) + ("; ");
        ret += "\nconsts: ";
        for(auto i : consts)
            ret += std::to_string(static_cast<double>(fixp_t::from_data(i))) + ("; ");
        ret += "\n";
        return ret;        
    }
        
    
    template <size_t S>
    static bool find_val_index(fixp_t* p, const std::array<fixp_t, S>& arr, size_t& ret) {
        ret = 0;
        for (auto i = arr.begin(); i < arr.end(); i++) {
            if (&(*i) == p)
                return true;
            ++ret;
        }
        return false;
    }
    
    void from_machine(atmsp::machine<fixp_t> &bc) {
        operators.clear();
        values.clear();
        varssize = bc.used_mem.vars;
        nums.clear();
        consts.clear();
        
        for (size_t i = 0; i < bc.used_mem.operations; i++) {
            auto b = atmsp::machine<fixp_t>::OPERATORS.begin();
            auto e = atmsp::machine<fixp_t>::OPERATORS.end();
            auto iter = std::find(b, e, bc.fun[i]);
            if(iter == e)
                eosio_assert(false, "exprinfo::bytecode::load, operator doesn't exist");
            operators.push_back(static_cast<index_t>(std::distance(b, iter)));
        }
               
        for (size_t i = 0; i < bc.used_mem.values; i++) {
            size_t valindex = 0;
            if (find_val_index(bc.val[i], bc.num, valindex))
                values.emplace_back(value{value::NUM, valindex});
            else if (find_val_index(bc.val[i], bc.var, valindex))
                values.emplace_back(value{value::VAR, valindex});
            else if (find_val_index(bc.val[i], bc.con, valindex))
                values.emplace_back(value{value::CONST, valindex});
            else
                break;
        }
        
        nums.resize(bc.used_mem.nums);
        for (size_t i = 0; i < bc.used_mem.nums; i++)
            nums[i] = bc.num[i].data();

        consts.resize(bc.used_mem.cons);
        for (size_t i = 0; i < bc.used_mem.cons; i++)
            consts[i] = bc.con[i].data();    
    }
    
    void to_machine(atmsp::machine<fixp_t> &bc)const {
        bc.used_mem = {
            .operations = operators.size(),
            .values = values.size(),
            .nums = nums.size(),
            .vars = static_cast<size_t> (varssize),
            .cons = consts.size()        
        };   
        
        eosio_assert(bc.used_mem.operations <= bc.fun.size(), "exprinfo::bytecode::apply, unsupported operators.size()");
        eosio_assert(bc.used_mem.values <= bc.val.size(),     "exprinfo::bytecode::apply, unsupported values.size()");
        eosio_assert(bc.used_mem.nums <= bc.num.size(),       "exprinfo::bytecode::apply, unsupported nums.size()");
        eosio_assert(bc.used_mem.vars <= bc.var.size(),       "exprinfo::bytecode::apply, unsupported vars.size()");
        eosio_assert(bc.used_mem.cons <= bc.con.size(),       "exprinfo::bytecode::apply, unsupported consts.size()");
        
        for (size_t i = 0; i < bc.used_mem.operations; i++)
            bc.fun[i] = atmsp::machine<fixp_t>::OPERATORS[static_cast<size_t>(operators[i])];
            
        for (size_t i = 0; i < bc.used_mem.nums; i++)
            bc.num[i] = fixp_t::from_data(nums[i]);

        for (size_t i = 0; i < bc.used_mem.cons; i++)
            bc.con[i] = fixp_t::from_data(consts[i]);
        
        for (size_t i = 0; i < bc.used_mem.values; i++) {
            auto& cur = values[i];
            size_t idx = static_cast<size_t>(cur.idx);
            switch (cur.kind) {
                case value::NUM:
                    eosio_assert(idx <= bc.num.size(), "exprinfo::bytecode::apply, unsupported num index");
                    bc.val[i] = &(bc.num[idx]); 
                    break;
                case value::VAR:
                    eosio_assert(idx <= bc.var.size(), "exprinfo::bytecode::apply, unsupported var index");
                    bc.val[i] = &(bc.var[idx]); 
                    break;
                case value::CONST:
                    eosio_assert(idx <= bc.con.size(), "exprinfo::bytecode::apply, unsupported con index");
                    bc.val[i] = &(bc.con[idx]);
                    break;
                default: eosio_assert(false, "exprinfo::bytecode::apply, unknown kind of value");
            }
        }
    }
};

}} // namespace atmsp::storable
