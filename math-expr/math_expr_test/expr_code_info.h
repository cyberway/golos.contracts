#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include "atmsp.h"

namespace exprinfo 
{   
using index_t = uint64_t;
struct value
{
    enum kind_t: uint8_t{NUM, VAR, CONST, UNDEF};
    uint8_t kind;
    index_t ind;
    value(kind_t k = UNDEF, size_t i = 0): kind(k), ind(i){}
    
#ifndef EOSLIB_DISABLED
    EOSLIB_SERIALIZE(value, (kind)(ind))    
#endif
};

struct bytecode
{
    std::vector<index_t> operators;
    std::vector<value> values;
    std::vector<BaseType> nums;
    std::vector<BaseType> consts;
    
    template <size_t S>
    static bool findvalindex(ExprType* p, const std::array<ExprType, S>& arr, size_t& ret)
    {
        ret = 0;
        for(auto i = arr.begin(); i < arr.end(); i++)
        {
            if(&(*i) == p)
                return true;
            ++ret;
        }
        return false;
    }
    
    void load(atmsp::Machine<ExprType> &bc)
    {
        operators.clear();
        values.clear();
        nums.clear();
        consts.clear();
        
        for(size_t i = 0; i < bc.opCnt; i++)
        {
            auto b = atmsp::Machine<ExprType>::all_operators.begin();
            auto e = atmsp::Machine<ExprType>::all_operators.end();
            auto iter = std::find(b, e, bc.fun[i]);
            if(iter == e)
                eosio_assert(false, "exprinfo::bytecode::load, operator doesn't exist");
            operators.push_back(static_cast<index_t>(std::distance(b, iter)));
        }
               
        for(auto v = bc.val.begin(); v != bc.val.end(); v++)
        {
            size_t valindex = 0;
            if(findvalindex(*v, bc.num, valindex))
                values.emplace_back(value(value::NUM, valindex));
            else if(findvalindex(*v, bc.var, valindex))
                values.emplace_back(value(value::VAR, valindex));
            else if(findvalindex(*v, bc.con, valindex))
                values.emplace_back(value(value::CONST, valindex));
            else
                break;
        }
        
        //TODO: remember real dims of /val, num, con/ while parsing
        nums.resize(bc.num.size());
        for(size_t i = 0; i < bc.num.size(); i++)
            nums[i] = bc.num[i].data();
        consts.resize(bc.con.size());
        for(size_t i = 0; i < bc.con.size(); i++)
            consts[i] = bc.con[i].data();
    
    }
    
    void apply(atmsp::Machine<ExprType> &bc)const
    {
        bc.opCnt = operators.size();
        eosio_assert(bc.opCnt <= bc.fun.size(),      "exprinfo::bytecode::apply, unsupported operators.size()");
        eosio_assert(values.size() <= bc.val.size(), "exprinfo::bytecode::apply, unsupported values.size()");
        eosio_assert(nums.size() <= bc.num.size(),   "exprinfo::bytecode::apply, unsupported nums.size()");
        eosio_assert(consts.size() <= bc.con.size(), "exprinfo::bytecode::apply, unsupported consts.size()");
        for(size_t i = 0; i < bc.opCnt; i++)
            bc.fun[i] = atmsp::Machine<ExprType>::all_operators[static_cast<size_t>(operators[i])];
            
        for(size_t i = 0; i < bc.num.size(); i++)
            bc.num[i] = ExprType::from_data(nums[i]);

        for(size_t i = 0; i < bc.con.size(); i++)
            bc.con[i] = ExprType::from_data(consts[i]);
        
        for(size_t i = 0; i < values.size(); i++)
        {
            auto& cur = values[i];
            size_t ind = static_cast<size_t>(cur.ind);
            switch(cur.kind) 
            {
                case value::NUM:
                    eosio_assert(ind <= bc.num.size(), "exprinfo::bytecode::apply, unsupported num index");
                    bc.val[i] = &(bc.num[ind]); 
                    break;
                case value::VAR:
                    eosio_assert(ind <= bc.var.size(), "exprinfo::bytecode::apply, unsupported var index");
                    bc.val[i] = &(bc.var[ind]); 
                    break;
                case value::CONST:
                    eosio_assert(ind <= bc.con.size(), "exprinfo::bytecode::apply, unsupported con index");
                    bc.val[i] = &(bc.con[ind]);
                    break;
                default: eosio_assert(false, "exprinfo::bytecode::apply, unknown kind of value");
            }
        }           
    }
    
#ifndef EOSLIB_DISABLED
    EOSLIB_SERIALIZE(bytecode, (operators)(values)(nums)(consts))
#endif
};

}
