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
#include "fixed_point_math.h"

/** *********************************************************************** **
 ** Determine maximum sizes for stack and var/val/num/con arrays.           **
 ** Note that sizes may have chache-effects e.g. Play here for tuning       **
 ** *********************************************************************** **/
namespace atmsp
{

constexpr size_t SIZE = 64;             // Stack, values-array and bytecode-operators
constexpr size_t MAXNUM = SIZE / 4;     // Numeric-array. Holds all numbers
constexpr size_t MAXVAR = SIZE / 8;     // Variables-array. Holds all variables
constexpr size_t MAXCON = SIZE / 16;    // Constants-array. Holds all constants
/** *********************************************************************** **
 ** Bytecode struct. Executes the bytecode produced by the parser. Once     **
 ** the bytecode contains a valid set of instructions, it acts completely   **
 ** independent from the parser itself.                                     **
 ** *********************************************************************** **/
template <typename T>
struct Machine 
{
	void ppush()  {	stk.push(*val[valInd++]); }

	void padd()   {T t(stk.pop()); stk.setTop(t+stk.top()); }
	void psub()   { T t(stk.pop()); stk.setTop(stk.top()-t); }
	void pmul()   { T t(stk.pop()); stk.setTop(t*stk.top()); }
	void pdiv()   { T t(stk.pop()); eosio_assert((t != T(0)), "atmsp::Machine: division by zero"); stk.setTop(stk.top()/t); }
	void pchs()   { stk.setTop(-stk.top()); }

    void pabs()   { stk.setTop(abs(stk.top())); }
    void psqrt()  { T t(stk.top()); eosio_assert((t >= T(0)), "atmsp::Machine: square root of a negative number"); stk.setTop(sqrt(t)); }

	void ppow()   { eosio_assert(false, (std::string("atmsp::Machine: unsupported operator ppow for ") + std::to_string(static_cast<double>(stk.pop()))).c_str()); }
	void ppow2()  { stk.setTop(stk.top()*stk.top()); }
	void ppow3()  { stk.setTop(stk.top()*stk.top()*stk.top()); }
	void ppow4()  { stk.setTop((stk.top()*stk.top()) * (stk.top()*stk.top())); }

	void psin()   { eosio_assert(false, "atmsp::Machine: unsupported operator psin"); }
	void pcos()   { eosio_assert(false, "atmsp::Machine: unsupported operator pcos"); }
	void ptan()   { eosio_assert(false, "atmsp::Machine: unsupported operator ptan"); }

	void psinh()  { eosio_assert(false, "atmsp::Machine: unsupported operator psinh"); }
	void ptanh()  { eosio_assert(false, "atmsp::Machine: unsupported operator ptanh"); }
	void pcosh()  { eosio_assert(false, "atmsp::Machine: unsupported operator pcosh"); }

	void pexp()   { eosio_assert(false, "atmsp::Machine: unsupported operator pexp"); }
	void plog()   { eosio_assert(false, "atmsp::Machine: unsupported operator plog"); }
	void plog10() { stk.setTop(log10(stk.top())); }
	void plog2()  { stk.setTop(log2(stk.top())); }

	void pasin()  { eosio_assert(false, "atmsp::Machine: unsupported operator pasin"); }
	void pacos()  { eosio_assert(false, "atmsp::Machine: unsupported operator pacos"); }
	void patan()  { eosio_assert(false, "atmsp::Machine: unsupported operator patan"); }
	void patan2() { eosio_assert(false, "atmsp::Machine: unsupported operator patan2"); }

	void pmax()   { T t(stk.pop()); if (t>stk.top()) stk.setTop(t); }
	void pmin()   { T t(stk.pop()); if (t<stk.top()) stk.setTop(t); }
	void psig()   { stk.top()>0 ? stk.setTop((T)1) : stk.top()<(T)0 ? stk.setTop((T)-1) : stk.setTop((T)0); }

	void pfloor() { eosio_assert(false, "atmsp::Machine: nsupported operator pfloor"); }
	void pround() {	eosio_assert(false, "atmsp::Machine: unsupported operator pround"); }

	using basic_operator = void (Machine<T>::*)();

    static size_t constexpr OPERATORS_NUM = 31;
	static constexpr std::array<basic_operator, OPERATORS_NUM> all_operators = 
    {{
			&Machine<T>::ppush,  &Machine<T>::padd,  &Machine<T>::psub,  &Machine<T>::pmul,
			&Machine<T>::pdiv,   &Machine<T>::pchs,  &Machine<T>::pabs,  &Machine<T>::psqrt,
			&Machine<T>::ppow,   &Machine<T>::ppow2, &Machine<T>::ppow3, &Machine<T>::ppow4,
			&Machine<T>::psin,   &Machine<T>::pcos,  &Machine<T>::ptan,  &Machine<T>::psinh,
			&Machine<T>::ptanh,  &Machine<T>::pcosh, &Machine<T>::pexp,  &Machine<T>::plog,
			&Machine<T>::plog10, &Machine<T>::plog2, &Machine<T>::pasin,  &Machine<T>::pacos, 
            &Machine<T>::patan, &Machine<T>::patan2, &Machine<T>::pmax,   &Machine<T>::pmin,  
            &Machine<T>::psig,  &Machine<T>::pfloor, &Machine<T>::pround
	}};
private:    
    class Stack 
    {
        std::array<T, SIZE> data;
        size_t sp;
    public:
        Stack() : sp(0) {}
        void clear() { sp = 0; }

        void push(T const &elem) 
        {
            eosio_assert(sp < (SIZE - 1), "atmsp::Machine: stack overflow");
            data[(++sp) - 1] = elem; 
        }
        T pop()
        {
            eosio_assert(sp > 0, "atmsp::Machine::Stack::pop(): empty stack");
            return data[(sp--) - 1]; 
        }

        T top()const
        {
            eosio_assert(sp > 0, "atmsp::Machine::Stack::top(): empty stack");
            return data[sp - 1]; 
        }
        void setTop(T const &elem)
        {
            eosio_assert(sp > 0, "atmsp::Machine::Stack::setTop(): empty stack");
            data[sp - 1] = elem; 
        }
    };
    
	Stack stk;
public:   
    std::array<basic_operator, SIZE> fun;
    size_t opCnt, valInd;
    
	/// All num, var and con values are consecutively mapped into the val-array.
	/// So in run() the bytecode operators work on the val-array only

    std::array<T*, SIZE> val;        
    std::array<T, MAXNUM> num;
    std::array<T, MAXVAR> var;
    std::array<T, MAXCON> con;
    
	/// Bytecode execution
	T run() 
    {
		stk.clear(); 
        valInd = 0;
		for (size_t i=0; i<opCnt; i++) 
            (*this.*fun[i])();
		return stk.top();
	}
};

template <typename T>
constexpr std::array<typename Machine<T>::basic_operator, Machine<T>::OPERATORS_NUM> Machine<T>::all_operators;
/** *********************************************************************** **
 ** Parser class. Parses a string and generates the bytecode. For certain   **
 ** kind of strings ("x^2", ...) the bytecode is optimzed for speed.        **
 ** *********************************************************************** **/
template <typename T>
class Parser 
{
	/// Search-helper for constant list
	struct CONTYPE 
    {
		std::string name;
		T val;

		CONTYPE() : val((T)0) {}
		CONTYPE(const std::string &n) : name(n), val((T)0) {}
		CONTYPE(const std::string &n, T v) : name(n), val(v) {}

		bool operator == (const CONTYPE &ct) { return name == ct.name; }
	};

	/// Recursive bytecode generation
	char *cp;                           // Character-pointer for parsing
	void expression(Machine<T> &bc);    // Handle expression as 1.st recursion level
	void term(Machine<T> &bc);          // Handle terms as 2.nd recursion level
	void factor(Machine<T> &bc);        // Handle factors as last recursion level

	/// Little helper functions
	bool isVar(const char *cp);       // Variable detection
	std::string skipAlphaNum();       // Variable/constant extraction

	void init();                      // Init function, constant and error message lists

	/// Basic counters and indices
	size_t opCnt, varCnt, valInd, numInd, varInd, conInd, funInd;
    
    template <typename V, size_t maxSize>
    class List 
    {
        size_t num;         // Current number of list objects
        std::array<V, maxSize> list;
    public:
        List() : num(0) {}
        List(std::array<V, maxSize>&& arg):num(maxSize), list(std::move(arg)){}
        void clear() { num = 0; }
        size_t size()const { return num; }
        const V &operator [] (const size_t index)const { return list[index]; }
        V &operator [] (const size_t index) { return list[index]; }

        void push(V const &elem) 
        {
            eosio_assert(num < maxSize, "atmsp::Parser: list overflow");
            list[num++] = elem;
        }

        bool find(V const &elem, size_t &index)
        {
            for (size_t i=0; i<num; i++)
                if ( list[i] == elem ) { index = i; return true; }
            return false;
        }
    };    

    static size_t constexpr STD_FUNCS_NUM = 21;
	List<std::string, STD_FUNCS_NUM> funLst;//TODO: static constexpr
	List<std::string, MAXVAR> varLst;    // Extracted variables from varString "x,y,.."
	List<CONTYPE, MAXCON> conLst;        // Our constants. $e and $pi are default

public:

	Parser() { init(); }

	void addConstant(const std::string &name, T value) 
    {
        eosio_assert(name[0] == '$', "atmsp::Parser: wrong constant name");
		conLst.push(CONTYPE(name, value));
	}

	void parse(Machine<T> &bc, const std::string &exp, const std::string &vars);
};

template <typename T>
void Parser<T>::init() 
{	
	funLst.push("abs");  funLst.push("cos");  funLst.push("cosh");
	funLst.push("exp");  funLst.push("log");  funLst.push("log10");
	funLst.push("log2"); funLst.push("sin");  funLst.push("sinh");
	funLst.push("sqrt"); funLst.push("tan");  funLst.push("tanh");
	
	funLst.push("asin");  funLst.push("acos");  funLst.push("atan");
	funLst.push("atan2"); funLst.push("max");   funLst.push("min");
	funLst.push("sig");   funLst.push("floor"); funLst.push("round");
}


template <typename T>
bool Parser<T>::isVar(const char *c) 
{
	char *tmp = (char *)c;
	while (isalnum(*tmp) && *tmp++);
	return (*tmp == '(' ? false : true);
}

template <typename T>
std::string Parser<T>::skipAlphaNum() 
{
	char *start = cp;
	std::string alphaString(cp++);
	while (isalnum(*cp) && *cp++);
	return alphaString.substr(0, static_cast<size_t>(cp- start));
}

template <typename T>
void Parser<T>::parse(Machine<T> &bc, const std::string &exps, const std::string &vars) 
{
	// Prepare clean expression and variable strings
	std::string::size_type pos, lastPos;
	std::string es(exps), vs(vars);
    es.erase(std::remove(es.begin(), es.end(), ' '), es.end());
    vs.erase(std::remove(vs.begin(), vs.end(), ' '), vs.end());
	if (es.empty()) 
        eosio_assert(false, "atmsp::Parser: string is empty");
	cp = (char *) es.c_str();

	// Split comma separated variables into varLst
	// One instance can be parsed repeatedly. So clear() is vital here
	varLst.clear();
	pos = vs.find_first_of(',', lastPos = vs.find_first_not_of(',', 0));
	while ( std::string::npos != pos || std::string::npos != lastPos ) 
    {
		varLst.push(vs.substr(lastPos, pos-lastPos));
		pos = vs.find_first_of(',', lastPos = vs.find_first_not_of(',', pos));
	}

	// Static parenthesis check. "Abuse" free opCnt/varCnt as open/close-counters
	opCnt = varCnt = 0;
	for (size_t i=0; i<es.size(); i++)
		if (es[i] == '(')
			opCnt++;
		else if (es[i] == ')') 
        {
			varCnt++;
			if (varCnt > opCnt) 
                eosio_assert(false, "atmsp::Parser: arCnt > opCnt");
		}
	if (opCnt != varCnt) 
        eosio_assert(false, "atmsp::Parser: opCnt != varCnt");

	// Reset all our counters and indices
	// opCnt  = Operator count. For bytecode and memory checks
	// varCnt = Variable count. For check if we have a constant expression
	// valInd = All num, var and con values are mapped into the bytecode-val-array
	// numInd = Numerical numbers array index
	opCnt = varCnt = valInd = numInd = 0;    

	// Run it once for parsing and generating the bytecode
	expression(bc);
	bc.opCnt = opCnt;

	// No vars in expression? Evaluate at compile time then
	if (!varCnt) 
    {
		bc.num[0] = bc.run();
		bc.val[0] = &bc.num[0];
		bc.fun[0] = &Machine<T>::ppush;
		bc.opCnt = 1;
	}
}

template <typename T>
void Parser<T>::expression(Machine<T> &bc) 
{
	// Enter next recursion level
	term(bc);

	while ( *cp=='+' || *cp=='-' )
		if ( *cp++ == '+' ) 
        {
			term(bc);
			bc.fun[opCnt++] = &Machine<T>::padd;
		}
		else 
        {
			term(bc);
			bc.fun[opCnt++] = &Machine<T>::psub;
		}
}

template <typename T>
void Parser<T>::term(Machine<T> &bc) 
{
	// Enter next recursion level
	factor(bc);

	while (*cp=='*' || *cp=='/')
		if (*cp++ == '*') 
        {
			factor(bc);
			bc.fun[opCnt++] = &Machine<T>::pmul;
		}
		else 
        {
			factor(bc);
			bc.fun[opCnt++] = &Machine<T>::pdiv;
		}
}


template <typename T>
void Parser<T>::factor(Machine<T> &bc) 
{
	/// Check available memory
	if (numInd >= MAXNUM || valInd >= SIZE || opCnt >= SIZE)
        eosio_assert(false, "atmsp::Parser: numInd >= MAXNUM || valInd >= _SIZE || opCnt >= SIZE");

	/// Handle open parenthesis and unary operators first
	if (*cp == '(') 
    {
		++cp; expression(bc);
		if (*cp++ != ')')
            eosio_assert(false, "atmsp::Parser: unclosed parenthesis");
	}
	else if (*cp == '+') 
    {
		++cp; factor(bc);
	}
	else if (*cp == '-') 
    {
		++cp; factor(bc);
		bc.fun[opCnt++] = &Machine<T>::pchs;
	}

	/// Extract numbers starting with digit or dot
	else if (isdigit(*cp)|| *cp=='.') 
    {
        int64_t a = 0;
        int64_t b = 1;
        bool p = false;
		while(isdigit(*cp) || ((*cp=='.') && !p))
        {
            if(*cp=='.')
                p = true;
            else
            {
                int64_t d = (*cp - '0');
                if(p)
                    b *= 10;
                a = a * 10 + d;
            }
            cp++;      
        }
        bc.num[numInd] = (T(a) / T(b));
		bc.val[valInd++] = &bc.num[numInd++];
		bc.fun[opCnt++] = &Machine<T>::ppush;
	}

	/// Extract constants starting with $
	else if (*cp == '$') 
    {
		if ( !conLst.find(skipAlphaNum(), conInd) )
           eosio_assert(false, "atmsp::Parser: unknown const");
		bc.con[conInd] = conLst[conInd].val;
		bc.val[valInd++] = &bc.con[conInd];
		bc.fun[opCnt++] = &Machine<T>::ppush;
	}

	/// Extract variables
	else if (isVar(cp)) 
    {
		if ( varLst.find(skipAlphaNum(), varInd) ) 
            varCnt++;
        else 
            eosio_assert(false, "atmsp::Parser: unknown var");
		bc.val[valInd++] = &bc.var[varInd];
		bc.fun[opCnt++] = &Machine<T>::ppush;
	}

	/// Extract functions
	else 
    {
		// Search function and advance cp behind open parenthesis
		if (funLst.find(skipAlphaNum(), funInd)) 
            ++cp;
        else 
            eosio_assert(false, "atmsp::Parser: unknown func");

		// Set operator function and advance cp
		switch (funInd) 
        {
			case  0: expression(bc); bc.fun[opCnt++] = &Machine<T>::pabs;    break;
			case  1: expression(bc); bc.fun[opCnt++] = &Machine<T>::pcos;    break;
			case  2: expression(bc); bc.fun[opCnt++] = &Machine<T>::pcosh;   break;
			case  3: expression(bc); bc.fun[opCnt++] = &Machine<T>::pexp;    break;
			case  4: expression(bc); bc.fun[opCnt++] = &Machine<T>::plog;    break;
			case  5: expression(bc); bc.fun[opCnt++] = &Machine<T>::plog10;  break;
			case  6: expression(bc); bc.fun[opCnt++] = &Machine<T>::plog2;   break;
			case  7: expression(bc); bc.fun[opCnt++] = &Machine<T>::psin;    break;
			case  8: expression(bc); bc.fun[opCnt++] = &Machine<T>::psinh;   break;
			case  9: expression(bc); bc.fun[opCnt++] = &Machine<T>::psqrt;   break;
			case 10: expression(bc); bc.fun[opCnt++] = &Machine<T>::ptan;    break;
			case 11: expression(bc); bc.fun[opCnt++] = &Machine<T>::ptanh;   break;
			case 12: expression(bc); bc.fun[opCnt++] = &Machine<T>::pasin;   break;
			case 13: expression(bc); bc.fun[opCnt++] = &Machine<T>::pacos;   break;
			case 14: expression(bc); bc.fun[opCnt++] = &Machine<T>::patan;   break;
			case 15: expression(bc); bc.fun[opCnt++] = &Machine<T>::patan2;  break;
			case 16: expression(bc); ++cp; expression(bc); bc.fun[opCnt++] = &Machine<T>::pmax; break;
			case 17: expression(bc); ++cp; expression(bc); bc.fun[opCnt++] = &Machine<T>::pmin; break;
			case 18: expression(bc); bc.fun[opCnt++] = &Machine<T>::psig;    break;
			case 19: expression(bc); bc.fun[opCnt++] = &Machine<T>::pfloor;  break;
			case 20: expression(bc); bc.fun[opCnt++] = &Machine<T>::pround;  break;
		}
		++cp;
	}

	/// At last handle univalent operators like ^ or % (not implemented here)
	if (*cp == '^') 
    {
		// Exponent a positive number? Try to optimize later
		bool optPow = isdigit( *++cp ) ? true : false;
		if (*(cp+1) == '^') optPow = false;
		factor(bc);
 		// Speed up bytecode for 2^2, x^3 ...
		if (optPow) 
        {
			if ( *bc.val[valInd-1] == (T)2.0 ) 
            {
				--valInd;
				bc.fun[opCnt-1] = &Machine<T>::ppow2;
			}
			else if ( *bc.val[valInd-1] == (T)3.0 ) 
            {                
				--valInd;
				bc.fun[opCnt-1] = &Machine<T>::ppow3;
			}
			else if ( *bc.val[valInd-1] == (T)4.0 ) 
            {
				--valInd;
				bc.fun[opCnt-1] = &Machine<T>::ppow4;
			}
			// Exponent is a positive number, but not 2-4. Proceed with standard pow()
			else
				bc.fun[opCnt++] = &Machine<T>::ppow;
		}
		// Exponent is a not a number or negative. Proceed with standard pow()
		else
			bc.fun[opCnt++] = &Machine<T>::ppow;
	}
}

}
