#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include "atmsp.h"

namespace exprinfo 
{   
using index_t = uint64_t;
struct value {
	enum kind_t: uint8_t {NUM, VAR, CONST, UNDEF};
	uint8_t kind;
	index_t idx;
	value(kind_t k = UNDEF, size_t i = 0): kind(k), idx(i){}
	
#ifndef EOSLIB_DISABLED
	EOSLIB_SERIALIZE(value, (kind)(idx))    
#endif
};

struct bytecode {
	index_t vars_size;
	std::vector<index_t> operators;
	std::vector<value> values;   
	std::vector<base_t> nums;
	std::vector<base_t> consts;
	
	template <size_t S>
	static bool find_val_index(expr_t* p, const std::array<expr_t, S>& arr, size_t& ret) {
		ret = 0;
		for (auto i = arr.begin(); i < arr.end(); i++)
		{
			if (&(*i) == p)
				return true;
			++ret;
		}
		return false;
	}
	
	void from_machine(atmsp::machine<expr_t> &bc) {
		operators.clear();
		values.clear();
		vars_size = bc.used_mem.vars;
		nums.clear();
		consts.clear();
		
		for (size_t i = 0; i < bc.used_mem.operations; i++) {
			auto b = atmsp::machine<expr_t>::OPERATORS.begin();
			auto e = atmsp::machine<expr_t>::OPERATORS.end();
			auto iter = std::find(b, e, bc.fun[i]);
			if(iter == e)
				eosio_assert(false, "exprinfo::bytecode::load, operator doesn't exist");
			operators.push_back(static_cast<index_t>(std::distance(b, iter)));
		}
			   
		for (size_t i = 0; i < bc.used_mem.values; i++) {
			size_t valindex = 0;
			if (find_val_index(bc.val[i], bc.num, valindex))
				values.emplace_back(value(value::NUM, valindex));
			else if (find_val_index(bc.val[i], bc.var, valindex))
				values.emplace_back(value(value::VAR, valindex));
			else if (find_val_index(bc.val[i], bc.con, valindex))
				values.emplace_back(value(value::CONST, valindex));
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
	
	void to_machine(atmsp::machine<expr_t> &bc)const {
		bc.used_mem = {
			.operations = operators.size(),
			.values = values.size(),
			.nums = nums.size(),
			.vars = static_cast<size_t> (vars_size),
			.cons = consts.size()        
		};   
		
		eosio_assert(bc.used_mem.operations <= bc.fun.size(), "exprinfo::bytecode::apply, unsupported operators.size()");
		eosio_assert(bc.used_mem.values <= bc.val.size(),     "exprinfo::bytecode::apply, unsupported values.size()");
		eosio_assert(bc.used_mem.nums <= bc.num.size(),       "exprinfo::bytecode::apply, unsupported nums.size()");
		eosio_assert(bc.used_mem.vars <= bc.var.size(),       "exprinfo::bytecode::apply, unsupported vars.size()");
		eosio_assert(bc.used_mem.cons <= bc.con.size(),       "exprinfo::bytecode::apply, unsupported consts.size()");
		
		for (size_t i = 0; i < bc.used_mem.operations; i++)
			bc.fun[i] = atmsp::machine<expr_t>::OPERATORS[static_cast<size_t>(operators[i])];
			
		for (size_t i = 0; i < bc.used_mem.nums; i++)
			bc.num[i] = expr_t::from_data(nums[i]);

		for (size_t i = 0; i < bc.used_mem.cons; i++)
			bc.con[i] = expr_t::from_data(consts[i]);
		
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
	
#ifndef EOSLIB_DISABLED
	EOSLIB_SERIALIZE(bytecode, (vars_size)(operators)(values)(nums)(consts))
#endif
};

}
