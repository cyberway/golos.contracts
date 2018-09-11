#include <eosiolib/eosio.hpp>
#include "types.h"
#include <string>
#include <vector>
#include <numeric>
  
#include "expr_code_info.h" 
using namespace exprinfo;
//@abi table  
struct exprresult
{    
        uint64_t id;
        BaseType value;
        uint64_t primary_key() const {return id;}
        EOSLIB_SERIALIZE(exprresult, (id)(value))   
};
 
using expr_results = eosio::multi_index<N(exprresults), exprresult>;

//@abi table
struct mathexprs
{ 
    account_name caller;
    bytecode func;
    uint64_t primary_key() const {return caller;}
    EOSLIB_SERIALIZE(mathexprs, (caller)(func))
};

using math_exprs_table = eosio::multi_index<N(mathexprs), mathexprs>;
    
class math_expr_test : public eosio::contract 
{
public:
    using contract::contract;
    
    void create_res_table() 
    {        
        expr_results table(_self, _self);
        auto iter = table.find(_self);
        if (iter == table.end()) 
            table.emplace(_self, [&](auto& o) { o.id = _self; o.value = 0;});
            
    }     
    /// @abi action
    void setdata(BaseType value)
    {        
        require_auth( _self );
        expr_results table(_self, _self);
        auto iter = table.find(_self);
        if (iter == table.end()) 
        {
            create_res_table();
            iter = table.find(_self);
        }
        table.modify(iter, _self, [&](auto& o) {o.value = value;});
    }

    /// @abi action
    void set(const std::string& expr_str, const std::string& vars_str)
    {     
        require_auth( _self );
        mathexprs exprs;
        exprs.caller = _self;
        
        atmsp::Parser<ExprType> pa;
	    atmsp::Machine<ExprType> bc;
   
	    pa.parse(bc, expr_str, vars_str);
        
        exprs.func.load(bc);        
        
        math_exprs_table table(_self, _self);    
        auto item = table.find(_self);
        if(item == table.end()) 
            table.emplace(_self, [&](auto& a) {a = exprs;});       
        else 
            table.modify(item, _self, [&](auto& a) {a = exprs;});
        create_res_table();
    }
 
     /// @abi action
    void calculate(const std::vector<BaseType>& vars)
    {
        require_auth( _self );
            
        math_exprs_table table(_self, _self);    
        auto item = table.find(_self);
        
        eosio_assert(item != table.end(), "math_expr_test::calculate, can't find expr");
        auto& f0 = item->func;
        atmsp::Machine<ExprType> bc;
        f0.apply(bc);

        eosio_assert(vars.size() <= bc.var.size(), "math_expr_test::calculate, wrong input size");
        for(size_t i = 0; i < vars.size(); i++)
            bc.var[i] = ExprType::from_data(vars[i]);
        setdata(bc.run().data());
       
    } 
};

EOSIO_ABI( math_expr_test, (setdata)(set)(calculate))
