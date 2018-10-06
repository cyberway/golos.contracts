#include <eosiolib/eosio.hpp>
#include "types.h"
#include <string>
#include <vector>
#include <numeric>
  
#include "expr_code_info.h" 
using namespace exprinfo;
//@abi table  
struct exprresult {    
	uint64_t id;
	base_t value;
	uint64_t primary_key() const {return id;}
	EOSLIB_SERIALIZE(exprresult, (id)(value))   
};
 
using expr_results = eosio::multi_index<N(exprresults), exprresult>;

//@abi table
struct mathexprs { 
	account_name caller;
	bytecode func;
	uint64_t primary_key() const {return caller;}
	EOSLIB_SERIALIZE(mathexprs, (caller)(func))
};

using math_exprs_table = eosio::multi_index<N(mathexprs), mathexprs>;
	
class math_expr_test : public eosio::contract {
public:
	using contract::contract;
	  
	/// @abi action
	void setdata(base_t value) {
                
		require_auth( _self );
        
		expr_results table(_self, _self);
		auto itr = table.find(_self);
		if (itr == table.end()) 
		{
			table.emplace(_self, [&](auto& o) { o.id = _self; o.value = 0;}); 
			itr = table.find(_self);
		}
		table.modify(itr, _self, [&](auto& o) {o.value = value;});
	}

	/// @abi action
	void set(const std::string& expr_str, const std::string& vars_str) {
             
		require_auth( _self );
        
		mathexprs exprs;
		exprs.caller = _self;
		
		atmsp::parser<expr_t> pa;
		atmsp::machine<expr_t> bc;
   
		pa(bc, expr_str, vars_str);
		
		exprs.func.from_machine(bc);        
		
		math_exprs_table table(_self, _self);    
		auto itr = table.find(_self);
		if(itr == table.end()) 
			table.emplace(_self, [&](auto& a) {a = exprs;});       
		else 
			table.modify(itr, _self, [&](auto& a) {a = exprs;});
	}
 
	 /// @abi action
	void calculate(const std::vector<base_t>& vars) {
        
		require_auth( _self );
			
		math_exprs_table table(_self, _self);    
		auto itr = table.find(_self);
		
		eosio_assert(itr != table.end(), "math_expr_test::calculate, can't find expr");
		atmsp::machine<expr_t> bc;
		itr->func.to_machine(bc);

		eosio_assert(vars.size() == bc.used_mem.vars, (
										std::string("math_expr_test::calculate, wrong input size: ") + 
										std::to_string(vars.size()) + " != " + 
										std::to_string(bc.used_mem.vars)).c_str());
		for(size_t i = 0; i < vars.size(); i++)
			bc.var[i] = expr_t::from_data(vars[i]);
		setdata(bc.run().data());	   
	} 
};

EOSIO_ABI( math_expr_test, (setdata)(set)(calculate))
