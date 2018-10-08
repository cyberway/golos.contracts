/** *********************************************************************** **
 ** Copyright (C) 1989-2013 Heinz van Saanen                                **
 **                                                                         **
 ** This file may be used under the terms of the GNU General Public         **
 ** License version 3 or later as published by the Free Software Foundation **
 ** and appearing in the file LICENSE.GPL included in the packaging of      **
 ** this file.                                                              **
 **                                                                         **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE **
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR        **
 ** PURPOSE.                                                                **
 **                                                                         **
 ** Verson: 1.0.4                                                           **
 ** *********************************************************************** **/
#pragma once
#include <string>
#include <array>
#include <string_view>
#include "fixed_point_math.h"

/** *********************************************************************** **
 ** Determine maximum sizes for stack and var/val/num/con arrays.           **
 ** Note that sizes may have chache-effects e.g. Play here for tuning       **
 ** *********************************************************************** **/
namespace atmsp
{
	
struct memory_info
{
	size_t operations;
	size_t values;
	size_t nums;
	size_t vars;
	size_t cons;
};    

constexpr memory_info SIZE { 64, 64, 16, 8, 4 };

 /** *********************************************************************** **
 ** Bytecode struct. Executes the bytecode produced by the parser. Once     **
 ** the bytecode contains a valid set of instructions, it acts completely   **
 ** independent from the parser itself.                                     **
 ** *********************************************************************** **/
template <typename T>
struct machine {
	void ppush()const  {	_stk.push( *val[_val_idx++] ); }

	void padd()const   { T t(_stk.pop()); _stk.set_top(t + _stk.top()); }
	void psub()const   { T t(_stk.pop()); _stk.set_top(_stk.top() - t); }
	void pmul()const   { T t(_stk.pop()); _stk.set_top(t * _stk.top()); }
	void pdiv()const   { T t(_stk.pop()); eosio_assert((t != static_cast<T>(0)), "atmsp::machine: division by zero"); _stk.set_top(_stk.top() / t); }
	void pchs()const   { _stk.set_top(-_stk.top()); }

	void pabs()const   { _stk.set_top(abs(_stk.top())); }
	void psqrt()const  { T t(_stk.top()); eosio_assert((t >= static_cast<T>(0)), "atmsp::machine: square root of a negative number"); _stk.set_top(sqrt(t)); }

	void ppow()const   { eosio_assert(false, (std::string("atmsp::machine: unsupported operator ppow for ") + std::to_string(static_cast<double>(_stk.pop()))).c_str()); }
	void ppow2()const  { _stk.set_top(_stk.top()*_stk.top()); }
	void ppow3()const  { _stk.set_top(_stk.top()*_stk.top() * _stk.top()); }
	void ppow4()const  { _stk.set_top((_stk.top() * _stk.top()) * (_stk.top() * _stk.top())); }

	void psin()const   { eosio_assert(false, "atmsp::machine: unsupported operator psin"); }
	void pcos()const   { eosio_assert(false, "atmsp::machine: unsupported operator pcos"); }
	void ptan()const   { eosio_assert(false, "atmsp::machine: unsupported operator ptan"); }

	void psinh()const  { eosio_assert(false, "atmsp::machine: unsupported operator psinh"); }
	void ptanh()const  { eosio_assert(false, "atmsp::machine: unsupported operator ptanh"); }
	void pcosh()const  { eosio_assert(false, "atmsp::machine: unsupported operator pcosh"); }

	void pexp()const   { eosio_assert(false, "atmsp::machine: unsupported operator pexp"); }
	void plog()const   { eosio_assert(false, "atmsp::machine: unsupported operator plog"); }
	void plog10()const { _stk.set_top( log10( _stk.top() ) ); }
	void plog2()const  { _stk.set_top( log2( _stk.top() ) ); }

	void pasin()const  { eosio_assert(false, "atmsp::machine: unsupported operator pasin"); }
	void pacos()const  { eosio_assert(false, "atmsp::machine: unsupported operator pacos"); }
	void patan()const  { eosio_assert(false, "atmsp::machine: unsupported operator patan"); }
	void patan2()const { eosio_assert(false, "atmsp::machine: unsupported operator patan2"); }

	void pmax()const   { T t(_stk.pop()); if (t > _stk.top()) _stk.set_top(t); }
	void pmin()const   { T t(_stk.pop()); if (t < _stk.top()) _stk.set_top(t); }
	void psig()const   { 
		_stk.top() > 0 ? 
				_stk.set_top(static_cast<T>(1)) : 
				_stk.top() < static_cast<T>(0) ? 
						_stk.set_top(static_cast<T>(-1)) :
						 _stk.set_top(static_cast<T>(0)); 
	}

	void pfloor()const { eosio_assert(false, "atmsp::machine: nsupported operator pfloor"); }
	void pround()const { eosio_assert(false, "atmsp::machine: unsupported operator pround"); }

	using basic_operator = void (machine<T>::*)()const;

	static size_t constexpr OPERATORS_NUM = 31;
	static constexpr std::array<basic_operator, OPERATORS_NUM> OPERATORS = {{
			&machine<T>::ppush,  &machine<T>::padd,   &machine<T>::psub,  &machine<T>::pmul,
			&machine<T>::pdiv,   &machine<T>::pchs,   &machine<T>::pabs,  &machine<T>::psqrt,
			&machine<T>::ppow,   &machine<T>::ppow2,  &machine<T>::ppow3, &machine<T>::ppow4,
			&machine<T>::psin,   &machine<T>::pcos,   &machine<T>::ptan,  &machine<T>::psinh,
			&machine<T>::ptanh,  &machine<T>::pcosh,  &machine<T>::pexp,  &machine<T>::plog,
			&machine<T>::plog10, &machine<T>::plog2,  &machine<T>::pasin, &machine<T>::pacos, 
			&machine<T>::patan,  &machine<T>::patan2, &machine<T>::pmax,  &machine<T>::pmin,  
			&machine<T>::psig,   &machine<T>::pfloor, &machine<T>::pround
	}};
	
private:

	template <size_t maxSize>   
	class flat_stack {
				
		std::array<T, maxSize> _data;
		size_t _sp; 
			   
	public:    
		flat_stack() : _sp(0) {}
		void clear() { _sp = 0; }

		void push( T const &elem ) {
			eosio_assert(_sp < (maxSize - 1), "atmsp::machine: stack overflow");
			_data[(++_sp) - 1] = elem; 
		}
		
		T pop() {
			eosio_assert(_sp > 0, "atmsp::machine::Stack::pop(): empty stack");
			return _data[(_sp--) - 1]; 
		}

		T top()const {
			eosio_assert(_sp > 0, "atmsp::machine::Stack::top(): empty stack");
			return _data[_sp - 1]; 
		}
		
		void set_top(T const &elem) {
			eosio_assert(_sp > 0, "atmsp::machine::Stack::set_top(): empty stack");
			_data[_sp - 1] = elem; 
		}
	};
	
	mutable flat_stack<SIZE.operations> _stk;
	mutable size_t _val_idx;
	
public:  

	std::array<basic_operator, SIZE.operations> fun;
	memory_info used_mem;
	/// All num, var and con values are consecutively mapped into the val-array.
	/// So in run() the bytecode operators work on the val-array only
	std::array<T*, SIZE.values> val;        
	std::array<T, SIZE.nums> num;
	std::array<T, SIZE.vars> var;
	std::array<T, SIZE.cons> con;
	   
	/// Bytecode execution
	T run()const {
		_stk.clear(); 
		_val_idx = 0;
		for (size_t i = 0; i < used_mem.operations; i++) 
			(*this.*fun[i])();
		return _stk.top();
	}
};

template <typename T>
constexpr std::array<typename machine<T>::basic_operator, machine<T>::OPERATORS_NUM> machine<T>::OPERATORS;

/** *********************************************************************** **
 ** parser class. Parses a string and generates the bytecode. For certain   **
 ** kind of strings ("x^2", ...) the bytecode is optimzed for speed.        **
 ** *********************************************************************** **/
template <typename T>
class parser {
	/// Search-helper for constant list
	struct const_t {
		std::string name;
		T val;
		const_t(const std::string &n = std::string(), T v = static_cast<T>(0)) : name(n), val(v) {}

		bool operator == (const const_t &ct)const { return name == ct.name; }
	};

	/// Recursive bytecode generation	
	void expression(machine<T> &bc)const;    // Handle expression as 1.st recursion level
	void term(machine<T> &bc)const;          // Handle terms as 2.nd recursion level
	void factor(machine<T> &bc)const;        // Handle factors as last recursion level

	/// Little helper functions
	static bool is_var(const char* cp);  // Variable detection
	std::string skip_alpha_num()const;   // Variable/constant extraction
 
	template <typename V, size_t maxSize>
	struct flat_list {
		size_t _count;
		std::array<V, maxSize> _data;
		
		const V &operator [] (const size_t idx)const {
			eosio_assert(idx < _count, "atmsp::parser:list wrong index"); 
			return _data[idx]; 
		}
		
		V &operator [] (const size_t idx) {
			eosio_assert(idx < _count, "atmsp::parser:list wrong index"); 
			return _data[idx]; 
		}
		
		void clear() { _count = 0; }
		
		size_t size()const { return _count; }
			 
		void push(V const &elem) {
			eosio_assert(_count < maxSize, "atmsp::parser: list overflow");
			_data[_count++] = elem;
		}

		bool find(V const &elem, size_t &idx)const {
			for (size_t i=0; i < _count; i++)
				if ( _data[i] == elem ) { idx = i; return true; }
			return false;
		}
	};
 
	static size_t constexpr STD_FUNCS_NUM = 21;
	static constexpr flat_list<std::string_view, STD_FUNCS_NUM> FUNC_LIST { STD_FUNCS_NUM, {{
			std::string_view("abs", 3),   std::string_view("cos", 3),   std::string_view("cosh", 4),
			std::string_view("exp", 3),   std::string_view("log", 3),   std::string_view("log10", 5),
			std::string_view("log2", 4),  std::string_view("sin", 3),   std::string_view("sinh", 4),
			std::string_view("sqrt", 4),  std::string_view("tan", 3),   std::string_view("tanh", 4),            
			std::string_view("asin", 4),  std::string_view("acos", 4),  std::string_view("atan", 4),
			std::string_view("atan2", 5), std::string_view("max", 3),   std::string_view("min", 3),
			std::string_view("sig", 3),   std::string_view("floor", 5), std::string_view("round", 5)}
	}};
	
	mutable const char* _cp;                                        // Character-pointer for parsing            
	mutable flat_list<std::string, SIZE.vars> _var_list {0, {}};    // Extracted variables from varString "x,y,.."
	
	flat_list<const_t, SIZE.cons> _con_list {0, {}};

public:

	void add_constant(const std::string& name, T value) {
		eosio_assert(name[0] == '$', "atmsp::parser: wrong constant name");
		_con_list.push(const_t(name, value));
	}

	void operator()(machine<T>& bc, const std::string& exp, const std::string& vars)const;
};

template <typename T>
constexpr typename parser<T>:: template flat_list<std::string_view, parser<T>::STD_FUNCS_NUM> parser<T>::FUNC_LIST;

template <typename T>
bool parser<T>::is_var(const char *c) {
	auto tmp = c;
	while (isalnum(*tmp) && *tmp++);
	return (*tmp == '(' ? false : true);
}

template <typename T>
std::string parser<T>::skip_alpha_num()const {
	auto start = _cp;
	std::string alphaString(_cp++);
	while (isalnum(*_cp) && *_cp++);
	return alphaString.substr(0, static_cast<size_t>(_cp - start));
}

template <typename T>
void parser<T>::operator()(machine<T> &bc, const std::string& exps, const std::string& vars)const {
	// Prepare clean expression and variable strings
	std::string::size_type pos, lastPos;
	std::string es(exps), vs(vars);
	es.erase(std::remove(es.begin(), es.end(), ' '), es.end());
	vs.erase(std::remove(vs.begin(), vs.end(), ' '), vs.end());
	if (es.empty()) 
		eosio_assert(false, "atmsp::parser: string is empty");
	_cp = es.c_str();

	// Split comma separated variables into _var_list
	// One instance can be parsed repeatedly. So clear() is vital here
	_var_list.clear();
	pos = vs.find_first_of(',', lastPos = vs.find_first_not_of(',', 0));
	while (std::string::npos != pos || std::string::npos != lastPos) {
		_var_list.push(vs.substr(lastPos, pos-lastPos));
		pos = vs.find_first_of(',', lastPos = vs.find_first_not_of(',', pos));
	}

	size_t opn = 0;
	size_t cls = 0;
	for (size_t i = 0; i < es.size(); i++)
		if (es[i] == '(')
			opn++;
		else if (es[i] == ')') {
			cls++;
			eosio_assert(cls <= opn, "atmsp::parser: cls > opn");
		}
		
	eosio_assert(opn == cls, "atmsp::parser: cls != opn");
	
	bc.used_mem = {
		.operations = 0,
		.values = 0,
		.nums = 0,
		.vars = 0,
		.cons = _con_list.size()        
	};   

	// Run it once for parsing and generating the bytecode
	expression(bc);
	
	// No vars in expression? Evaluate at compile time then
	if (!bc.used_mem.vars) {
		bc.num[0] = bc.run();
		bc.val[0] = &bc.num[0];
		bc.fun[0] = &machine<T>::ppush;
		
		bc.used_mem = {
			.operations = 1,
			.values = 1,
			.nums = 1,
			.vars = 0,
			.cons = 0       
		};
	}
}

template <typename T>
void parser<T>::expression(machine<T> &bc)const {
	// Enter next recursion level
	term(bc);

	while ( *_cp == '+' || *_cp == '-' )
		if ( *_cp++ == '+' ) {
			term(bc);
			bc.fun[bc.used_mem.operations++] = &machine<T>::padd;
		}
		else {
			term(bc);
			bc.fun[bc.used_mem.operations++] = &machine<T>::psub;
		}
}

template <typename T>
void parser<T>::term(machine<T> &bc)const {
	// Enter next recursion level
	factor(bc);

	while (*_cp == '*' || *_cp == '/')
		if (*_cp++ == '*') {
			factor(bc);
			bc.fun[bc.used_mem.operations++] = &machine<T>::pmul;
		}
		else {
			factor(bc);
			bc.fun[bc.used_mem.operations++] = &machine<T>::pdiv;
		}
}


template <typename T>
void parser<T>::factor(machine<T> &bc)const {
	/// Check available memory
	if (bc.used_mem.nums >= SIZE.nums || bc.used_mem.values >= SIZE.values || bc.used_mem.operations >= SIZE.operations)
		eosio_assert(false, "atmsp::parser: bc.used_mem.nums >= SIZE.nums || bc.used_mem.values >= SIZE.values || bc.used_mem.operations >= SIZE.operations");

	/// Handle open parenthesis and unary operators first
	if (*_cp == '(') {
		++_cp; 
		expression(bc);
		if (*_cp++ != ')')
			eosio_assert(false, "atmsp::parser: unclosed parenthesis");
	}
	else if (*_cp == '+') {
		++_cp; 
		factor(bc);
	}
	else if (*_cp == '-') {
		++_cp; 
		factor(bc);
		bc.fun[bc.used_mem.operations++] = &machine<T>::pchs;
	}

	/// Extract numbers starting with digit or dot
	else if (isdigit(*_cp) || *_cp == '.') {
		int64_t a = 0;
		int64_t b = 1;
		bool p = false;
		while(isdigit(*_cp) || ((*_cp == '.') && !p)) {
			if(*_cp == '.')
				p = true;
			else {
				int64_t d = (*_cp - '0');
				if(p)
					b *= 10;
				a = a * 10 + d;
			}
			_cp++;      
		}
		bc.num[bc.used_mem.nums] = (T(a) / T(b));
		bc.val[bc.used_mem.values++] = &bc.num[bc.used_mem.nums++];
		bc.fun[bc.used_mem.operations++] = &machine<T>::ppush;
	}

	/// Extract constants starting with $
	else if (*_cp == '$') {
		size_t idx;
		if (!_con_list.find(skip_alpha_num(), idx))
		   eosio_assert(false, "atmsp::parser: unknown const");
		bc.con[idx] = _con_list[idx].val;
		bc.val[bc.used_mem.values++] = &bc.con[idx];
		bc.fun[bc.used_mem.operations++] = &machine<T>::ppush;
	}

	/// Extract variables
	else if (is_var(_cp)) {
		size_t idx;
		if (_var_list.find(skip_alpha_num(), idx)) 
			bc.used_mem.vars = std::max(bc.used_mem.vars, idx + 1); //sic
		else 
			eosio_assert(false, "atmsp::parser: unknown var");
		bc.val[bc.used_mem.values++] = &bc.var[idx];
		bc.fun[bc.used_mem.operations++] = &machine<T>::ppush;
	}

	/// Extract functions
	else {
		size_t idx;
		// Search function and advance cp behind open parenthesis
		if (FUNC_LIST.find(skip_alpha_num(), idx)) 
			++_cp;
		else 
			eosio_assert(false, "atmsp::parser: unknown func");

		// Set operator function and advance cp
		switch (idx) {
			case  0: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pabs;    break;
			case  1: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pcos;    break;
			case  2: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pcosh;   break;
			case  3: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pexp;    break;
			case  4: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::plog;    break;
			case  5: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::plog10;  break;
			case  6: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::plog2;   break;
			case  7: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::psin;    break;
			case  8: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::psinh;   break;
			case  9: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::psqrt;   break;
			case 10: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::ptan;    break;
			case 11: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::ptanh;   break;
			case 12: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pasin;   break;
			case 13: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pacos;   break;
			case 14: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::patan;   break;
			case 15: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::patan2;  break;
			case 16: expression(bc); ++_cp; expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pmax; break;
			case 17: expression(bc); ++_cp; expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pmin; break;
			case 18: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::psig;    break;
			case 19: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pfloor;  break;
			case 20: expression(bc); bc.fun[bc.used_mem.operations++] = &machine<T>::pround;  break;
		}
		++_cp;
	}

	/// At last handle univalent operators like ^ or % (not implemented here)
	if (*_cp == '^') {
		// Exponent a positive number? Try to optimize later
		bool opt = isdigit( *++_cp ) ? true : false;
		if (*(_cp + 1) == '^') opt = false;
		factor(bc);
 		// Speed up bytecode for 2^2, x^3 ...
		if (opt) {
			if ( *bc.val[bc.used_mem.values-1] == (T)2.0 ) {
				--bc.used_mem.values;
                --bc.used_mem.nums;
				bc.fun[bc.used_mem.operations-1] = &machine<T>::ppow2;                
			}
			else if ( *bc.val[bc.used_mem.values-1] == (T)3.0 ) {                
				--bc.used_mem.values;
                --bc.used_mem.nums;
				bc.fun[bc.used_mem.operations-1] = &machine<T>::ppow3;
			}
			else if ( *bc.val[bc.used_mem.values-1] == (T)4.0 ) {
				--bc.used_mem.values;
                --bc.used_mem.nums;
				bc.fun[bc.used_mem.operations-1] = &machine<T>::ppow4;
			}
			// Exponent is a positive number, but not 2-4. Proceed with standard pow()
			else
				bc.fun[bc.used_mem.operations++] = &machine<T>::ppow;
		}
		// Exponent is a not a number or negative. Proceed with standard pow()
		else
			bc.fun[bc.used_mem.operations++] = &machine<T>::ppow;
	}
}

}
