#define UNIT_TEST_ENV
#include "math_expr_test/types.h"

#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
 
#include <math_expr_test/fixed_point_math.h>
 
#include <math_expr_test/math_expr_test.wast.hpp>
#include <math_expr_test/math_expr_test.abi.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <math.h> 
  
using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;
using namespace sg14;
 
double log2(double arg)
{
    return log10(arg) / log10(2.0);
} 

using mvo = fc::mutable_variant_object;
constexpr char all_vars_string[] = "x0,x1,x2,x3,x4,x5,x6,x7";
constexpr double accuracy = 0.995;

class math_expr_tester : public tester 
{
    abi_serializer abi_ser;
public:

    math_expr_tester() 
    {
        produce_blocks( 2 );
        create_accounts( {N(math.expr) } );
        produce_blocks( 2 );
        set_code( N(math.expr), math_expr_test_wast );
        set_abi( N(math.expr), math_expr_test_abi );
        produce_blocks();
        const auto& accnt = control->db().get<account_object, by_name>( N(math.expr) );
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);
    }
    
    action_result set_expr(const std::string& expr_str, const std::string& vars_str) 
    { 
        return push_action(N(math.expr), N(set), mvo()( "expr_str", expr_str)("vars_str", vars_str));
    }
   
    ExprType get_result()
    {
        vector<char> data = get_row_by_account(N(math.expr), N(math.expr), N(exprresults), N(math.expr));
        BOOST_REQUIRE_EQUAL(data.empty(), false);
        auto res = abi_ser.binary_to_variant( "exprresult", data, abi_serializer_max_time );
        return ExprType::from_data(res["value"].as<BaseType>());
    }
   
    action_result calculate(const std::vector<ExprType>& vars)
    {
        std::vector<BaseType> bases(vars.size());
        for(size_t i = 0; i < vars.size(); i++)
            bases[i] = vars[i].data();
        return push_action(N(math.expr), N(calculate), mvo()("vars", reinterpret_cast<const std::vector<BaseType>&>(vars)));
    }
    
    action_result set_and_calc(const std::string& expr_str, const std::vector<ExprType>& vars)
    { 
        produce_blocks(2);
        action_result ret = set_expr(expr_str, all_vars_string);
        if(ret != success()) return ret;        
        produce_blocks(1);
        ret = calculate(vars);
        produce_blocks(1);
        return ret;
    }

    action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
        string action_type_name = abi_ser.get_action_type(name);
        action act;
        act.account = N(math.expr);
        act.name    = name;
        act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer_max_time );
        auto ret = base_tester::push_action( std::move(act), uint64_t(signer));
        return ret;
    }
};

BOOST_AUTO_TEST_SUITE(math_expr_tests)

#define BRACED_INIT_LIST(...) {__VA_ARGS__}
#define CHECK_MATH_EQUAL(EXPR, EXPRSTR, V)\
{\
	vector<double> x = BRACED_INIT_LIST V;\
    vector<ExprType> vars(x.size());\
    for(size_t i = 0; i < x.size(); i++)\
        vars[i] = x[i];\
\
	double val_double = (EXPR);\
    BOOST_REQUIRE_EQUAL(success(), set_and_calc(EXPRSTR, vars));\
	ExprType val_fixed = get_result();\
    double val_result = static_cast<double>(val_fixed);\
    BOOST_CHECK_MESSAGE((val_double * accuracy) <= val_result, (std::string(EXPRSTR) + ": " + std::to_string(val_result) + " != " + std::to_string(val_double)));\
	BOOST_CHECK_MESSAGE((val_double / accuracy) >= val_result, (std::string(EXPRSTR) + ": " + std::to_string(val_result) + " != " + std::to_string(val_double)));\
}

#define CHECK_MATH(EXPR, V)\
{\
    std::string exprStr(#EXPR);\
	exprStr.erase(std::remove_if(exprStr.begin(), exprStr.end(),\
		[](char ch) { return ((ch == '[') || (ch == ']')); }\
		), exprStr.end());\
    CHECK_MATH_EQUAL(EXPR, exprStr, V);\
}
  
BOOST_FIXTURE_TEST_CASE(math_tests, math_expr_tester) try 
{
    CHECK_MATH(x[0] / x[3],                     (30.4, 3.15, 2.1, 1.1));   
    CHECK_MATH(((x[0] / x[1]) / (x[2])) + x[3], (30.4, 3.15, 2.1, 1.5));
    CHECK_MATH(sqrt(x[0]) + sqrt(x[2]) * sqrt(x[1]) / sqrt(x[2]),        (30.4, 3.15, 2.1, 1.5));
    CHECK_MATH(((x[0] / x[1]) / (x[2])) * x[3], (30.4, 3.15, 2.1, 1.5));
    CHECK_MATH(abs(x[0]) / sqrt(abs(x[3])) + sqrt(abs(x[2])) + abs(x[1]), (-30.4, -3.15, -2.1, -1.1));
    
    CHECK_MATH(log2(x[0]) + log2(x[2]) * log2(x[1]) / log2(x[2]),        (30.4, 3.15, 2.1, 1.5));
    CHECK_MATH(log10(x[0]) + log10(x[2]) * log10(x[1]) / log10(x[2]),        (30.4, 3.15, 2.1, 1.5));    
    CHECK_MATH(log10(x[0]) + log10(x[2]) * log10(x[1]) / log10(x[2]),        (310.4, 0.15, 5.1, 88.5));
    
    CHECK_MATH_EQUAL(pow(x[2], 2) + pow(x[3], 2) + pow(x[0], 3) * pow(x[0], 4), "(x2^2) + (x3^2) + (x0^3) * (x0^4)", (2, 3, 4, 2));
    
    BOOST_CHECK_EQUAL("assertion failure with message: atmsp::Parser: unknown var", set_and_calc("x0 x1", {ExprType(2.0), ExprType(0.0)}));
    BOOST_CHECK_EQUAL("assertion failure with message: atmsp::Machine: division by zero", set_and_calc("x0 / x1", {ExprType(2.0), ExprType(0.0)}));
    BOOST_CHECK_EQUAL("assertion failure with message: atmsp::Machine: square root of a negative number", set_and_calc("sqrt(x0 - x1)", {ExprType(2.0), ExprType(10.5)}));

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
