#define UNIT_TEST_ENV
#include "math_expr_test/types.h"

 
#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <reward_calcs/reward_calcs.wast.hpp>
#include <reward_calcs/reward_calcs.abi.hpp>
 
#include <Runtime/Runtime.h>  

#include <fc/variant_object.hpp>
#include "reward_calcs/exttypes.h"
#include <math.h>
#include <map>
  
struct balance_data {
    double tokenamount = 0.0; 
    double vestamount = 0.0;
};

struct pool_data {
    double msgs = 0.0;
    double funds = 0.0;   
    double rshares = 0.0;
    double rsharesfn = 0.0;   
};

struct message_data {
    double absshares = 0.0;
    double netshares = 0.0; 
    double voteshares = 0.0;    
    double sumcuratorsw = 0.0;
};   
     
constexpr struct {
    balance_data balance {130.0, 1.0};
    pool_data pool {0.001, 130, -0.01, -0.01};
    message_data message {-0.01, -0.01, -0.01, -0.01};   
} delta;  

constexpr double balance_delta = 0.01;
constexpr double funds_delta = 0.01;
constexpr double pool_rshares_delta = 0.01;
constexpr double pool_rsharesfn_delta = 0.01;
  
using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;
 
using mvo = fc::mutable_variant_object;

double log2(double arg) {
    return log(arg) / log(2.0);
} 

struct message_key {
    account_name author;
    uint64_t id;
    bool operator ==(const message_key& rhs) const {
        return (author == rhs.author) && (id == rhs.id);
    } 
};

struct aprox_val_t {
    double val;
    double delta;
    bool operator ==(const aprox_val_t& rhs) const {
        BOOST_REQUIRE_MESSAGE(((rhs.delta >= 0.0) && (delta >= 0.0)) || ((rhs.delta <= 0.0) && (delta <= 0.0)), "aprox_val_t operator ==(): wrong comparison mode");
        if(((rhs.val > 0.0) && (val < 0.0)) || ((rhs.val < 0.0) && (val > 0.0)))
            return false;
        double d = std::min(double(std::abs(rhs.delta)), std::abs(double(delta)));
        if(delta < 0.0)
        {
            double a = std::min(std::abs(rhs.val), std::abs(val));
            double b = std::max(std::abs(rhs.val), std::abs(val));
            return (b < 1.e-20) || ((a / b) > (1.0 - d));
        }
        else
            return ( (val - d) <= rhs.val ) && ( rhs.val <= (val + d) ); 
    } 
    
    aprox_val_t& operator = (const double &arg) { val = arg; return *this; }
    aprox_val_t& operator +=(const double &arg) { val += arg; return *this; }    
};

std::ostream& operator<< (std::ostream& os, const aprox_val_t& rhs) {
    os << std::to_string(rhs.val) << " (" << std::to_string(double(rhs.delta)) << ") ";
    return os;
}

struct statemap : public std::map<std::string, aprox_val_t> {
    static std::string get_pool_str(uint64_t id) {
        return "pool #" +  std::to_string(id) + ": ";
    }
    static std::string get_balance_str(account_name acc) {
        return "balance of " +  acc.to_string() + ": ";
    }
    static std::string get_message_str(const message_key& msg) {
        return "message of " + msg.author.to_string() + " #" + std::to_string(msg.id) + ": ";
    }
    static std::string get_vote_str(account_name voter, const message_key& msg) {
        return "vote of " + voter.to_string() + " for message of " + msg.author.to_string() + " #" + std::to_string(msg.id) + ": ";
    }
    void set_pool(uint64_t id, const pool_data& data = {}) {
        auto prefix = statemap::get_pool_str(id);
        operator[](prefix + "msgs") = { data.msgs, delta.pool.msgs };
        operator[](prefix + "funds") = { data.funds, delta.pool.funds };
        operator[](prefix + "rshares") = { data.rshares, delta.pool.rshares };
        operator[](prefix + "rsharesfn") = { data.rsharesfn, delta.pool.rsharesfn };
    }
    void set_balance(account_name acc, const balance_data data = {}) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "tokenamount") = { data.tokenamount, delta.balance.tokenamount };
        operator[](prefix + "vestamount") = { data.vestamount, delta.balance.vestamount };
    }
    void set_message(const message_key& msg, const message_data& data = {}) {

        auto prefix = statemap::get_message_str(msg);
        operator[](prefix + "absshares") = { data.absshares, delta.message.absshares };
        operator[](prefix + "netshares") = { data.netshares, delta.message.netshares };
        operator[](prefix + "voteshares") = { data.voteshares, delta.message.voteshares };
        operator[](prefix + "sumcuratorsw") = { data.sumcuratorsw, delta.message.sumcuratorsw };
    }
    
    pool_data get_pool(uint64_t id) {
        auto prefix = statemap::get_pool_str(id);
        return {
            operator[](prefix + "msgs").val,
            operator[](prefix + "funds").val,
            operator[](prefix + "rshares").val,
            operator[](prefix + "rsharesfn").val
        };
    }
    
    void remove_same(const statemap& rhs) {
        for(auto itr_rhs = rhs.begin(); itr_rhs != rhs.end(); itr_rhs++) {
            auto itr = find(itr_rhs->first);
            if(itr->second == itr_rhs->second)
                erase(itr);
        }
    }   
};

std::ostream& operator<< (std::ostream& os, const statemap& rhs) {
    for(auto itr = rhs.begin(); itr != rhs.end(); ++itr)
        os << itr->first << " = " << itr->second << "\n";
    return os; 
}
   

class extended_tester : public tester {
    fc::microseconds _cur_time;
    void update_cur_time() { _cur_time = control->head_block_time().time_since_epoch(); 
        BOOST_TEST_MESSAGE( "cur time = " << _cur_time.to_seconds() << "\n" );};
    
protected:
    const fc::microseconds& cur_time()const { return _cur_time; };
    
public:
    vector<vector<char> > get_all_rows(uint64_t code, uint64_t scope, uint64_t table, bool strict = true)const {
        vector<vector<char> > ret;
        const auto& db = control->db();
        const auto* t_id = db.find<chain::table_id_object, chain::by_code_scope_table>(boost::make_tuple(code, scope, table));
        if(strict)
            BOOST_REQUIRE_EQUAL(true, static_cast<bool>(t_id));
        else if(!static_cast<bool>(t_id))
            return ret;
        const auto& idx = db.get_index<chain::key_value_index, chain::by_scope_primary>();
        for(auto itr = idx.lower_bound(boost::make_tuple(t_id->id, 0)); (itr != idx.end()) && (itr->t_id == t_id->id); ++itr) {
            ret.push_back(vector<char>());
            auto& data = ret.back();
            data.resize(itr->value.size());
            memcpy(data.data(), itr->value.data(), data.size());
        }
        return ret; 
    }
    
    void step(uint32_t n = 1) {
        produce_blocks(n);
        update_cur_time();
    }
    
    void run(const fc::microseconds& t) {
        produce_min_num_of_blocks_to_spend_time_wo_inactive_prod(t);
        update_cur_time();
    }
};

struct vote {
    account_name voter;
    double weight;
    double vesting;
};

struct message {
    message_key key;
    std::list<vote> votes;
    double get_rshares_sum()const {
        double ret = 0.0;
        for(auto& v : votes)
            ret += v.weight * v.vesting;
        return ret;
    };
    message(message_key k) : key(k) {};
};

struct rewardrules {
    std::function<double(double)> mainfunc;
    std::function<double(double)> curationfunc;
    std::function<double(double)> timepenalty;
    double curatorsprop;
    rewardrules(
        std::function<double(double)>&&  mainfunc_,
        std::function<double(double)>&& curationfunc_,
        std::function<double(double)>&& timepenalty_,    
        double curatorsprop_) :   
             
       
        mainfunc(std::move(mainfunc_)),
        curationfunc(std::move(curationfunc_)),
        timepenalty(std::move(timepenalty_)),
        curatorsprop(curatorsprop_) {};
};
 
struct rewardpool {   
    uint64_t id;
    double funds;
    rewardrules rules;
    std::list<message> messages;
    
    double get_rshares_sum()const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += m.get_rshares_sum();
        return ret;
    }
    
    //yup, it's dumb. but we are testing here, we don't need performance, we prefer straightforward logic
    double get_rsharesfn_sum()const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += rules.mainfunc(m.get_rshares_sum());
        return ret;    
    }    
    
    double get_ratio()const {
        BOOST_REQUIRE_MESSAGE(funds >= 0.0, "rewardpool: wrong payment mode");
        auto r = get_rshares_sum();
        return (r > 0) ? (funds / r) : std::numeric_limits<double>::max();
    }
    rewardpool(uint64_t id_, rewardrules&& rules_) : id(id_), funds(0.0), rules(rules_) {};
    
};
 
struct state {
    std::map<account_name, balance_data> balances;
    std::vector<rewardpool> pools;
    void clear() { balances.clear(); pools.clear(); };
};
 
   
class reward_calcs_tester : public extended_tester {
    abi_serializer abi_ser;
protected:
    account_name _contract_acc;
    std::vector<account_name> _users;
    statemap _req;
    statemap _res;
    state _state;
public: 

    reward_calcs_tester() :
        _contract_acc(N(reward.calcs)), 
        _users{ N(alice),  N(alice1),  N(alice2), N(alice3), N(alice4), N(alice5),
                N(bob), N(bob1), N(bob2), N(bob3), N(bob4), N(bob5),
                N(why), N(has), N(my), N(imagination), N(become), N(poor) } {
        step(2);
        create_accounts({_contract_acc});
        create_accounts(_users);
        step(2);
        set_code(_contract_acc, reward_calcs_wast);
        set_abi(_contract_acc, reward_calcs_abi);
        step();
        const auto& accnt = control->db().get<account_object, by_name>( _contract_acc );
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);
    } 
     
    action_result issue(int64_t amount) {        
        BOOST_REQUIRE_MESSAGE(!_state.pools.empty(), "issue: _state.pools is empty");
        size_t choice = 0;
        auto min_ratio = std::numeric_limits<double>::max();
        for(size_t i = 0; i < _state.pools.size(); i++) {
            double cur_ratio = _state.pools[i].get_ratio();
            if(cur_ratio <= min_ratio) {
                min_ratio = cur_ratio;
                choice = i;
            }
        }
        _state.pools[choice].funds += amount;
        
        return push_action(_contract_acc, N(issue), mvo()( "amount", amount));
    }

    action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
        string action_type_name = abi_ser.get_action_type(name);
        action act;
        act.account = _contract_acc;
        act.name    = name;          
       
        act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer_max_time );        
             
        auto ret = base_tester::push_action( std::move(act), uint64_t(signer));
        return ret;
    }   
       
    variant binary_to_variant(const std::string& name, const vector<char>& data) const {
        BOOST_REQUIRE_EQUAL(data.empty(), false);
        return abi_ser.binary_to_variant(name.c_str(), data, abi_serializer_max_time);
    }
    
    action_result payto(account_name user, int64_t amount, payment_t mode) {
        
        switch (mode) {
                case payment_t::TOKEN:   _state.balances[user].tokenamount += amount; break;            
                case payment_t::VESTING: _state.balances[user].vestamount  += amount;  break;
                default: BOOST_REQUIRE_MESSAGE(false, "payto: wrong payment mode");
        }
        
        return push_action(_contract_acc, N(payto), mvo() 
                ("user", user)
                ("amount", amount)
                ("mode", static_cast<enum_t>(mode))); 
    }
         
    action_result setrules(     
        const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
        int64_t curatorsprop,
        std::function<double(double)>&&  mainfunc_,
        std::function<double(double)>&& curationfunc_,
        std::function<double(double)>&& timepenalty_
        ) {     
        auto ret = push_action(_contract_acc, N(setrules), mvo()
                ("mainstr", mainfunc.str)
                ("mainmaxarg", mainfunc.maxarg)
                
                ("crtnstr", curationfunc.str)
                ("crtnmaxarg", curationfunc.maxarg)
                
                ("pnltstr", timepenalty.str)
                ("pnltmaxarg", timepenalty.maxarg)
            
                ("curatorsprop", curatorsprop));
                
        if(ret == success()) {   
            double unclaimed_funds = 0.0;
            for(auto& p : _state.pools)
                if(!p.messages.size())
                    unclaimed_funds += p.funds;
            _state.pools.erase(std::remove_if(_state.pools.begin(), _state.pools.end(),
                    [](const rewardpool& p){return !p.messages.size();}),
                    _state.pools.end());       
            
            auto rows = get_all_rows(_contract_acc, _contract_acc, N(rewardpools), false);
            auto created = binary_to_variant("rewardpool", *rows.rbegin())["created"].as<uint64_t>();        
            _state.pools.emplace_back(rewardpool(created, 
            rewardrules(std::move(mainfunc_), std::move(curationfunc_), std::move(timepenalty_), static_cast<double>(curatorsprop) / 10000.0)));
            _state.pools.back().funds += unclaimed_funds;
        }
        return ret;
    }
    
    action_result addmessage(const message_key& key) {
        _state.pools.back().messages.emplace_back(message(key));
         return push_action(key.author, N(addmessage), mvo()
                ("author", key.author)
                ("id", key.id));
    }
    
    action_result addvote(const message_key& msg_key, account_name voter, int64_t weight) {
    
        auto ret = push_action(voter, N(addvote), mvo()
                    ("author", msg_key.author)
                    ("msgid", msg_key.id)
                    ("voter", voter)
                    ("weight", weight));
                    
        for(auto& p : _state.pools)            
            for(auto& m : p.messages)
                if(m.key == msg_key)
                {
                    m.votes.emplace_back(vote{voter, static_cast<double>(weight) / 10000.0, _state.balances[voter].vestamount});
                    return ret;
                }
        
        if(ret == success())
            BOOST_REQUIRE_MESSAGE(false, "addvote: ret == success(), but message not found in state");
        return ret;
    
    }
    
    action_result payrewards(const message_key& msg_key) {
        
        auto ret = push_action(_contract_acc, N(payrewards), mvo()
                    ("author", msg_key.author)
                    ("id", msg_key.id));    
        
        for(auto itr_p = _state.pools.begin(); itr_p != _state.pools.end(); itr_p++) {
            auto& p = *itr_p;
            for(auto itr_m = p.messages.begin(); itr_m != p.messages.end(); itr_m++)
            {   
                auto m = *itr_m;                  
                if(m.key == msg_key)
                {                   
                    double pool_rsharesfn_sum = p.get_rsharesfn_sum();
                                 
                    int64_t payout = 0;
                    if(p.messages.size() == 1) 
                        payout = p.funds;
                    else if(pool_rsharesfn_sum > 1.e-20)
                        payout = (p.rules.mainfunc(m.get_rshares_sum()) * p.funds) / pool_rsharesfn_sum;
                        
                    auto curation_payout = p.rules.curatorsprop * payout;                    
                    
                    double unclaimed_funds = 0.0;//TODO 
                    
                    std::list<std::pair<account_name, double> > cur_rewards;
                    double curators_fn_sum = 0.0;
                    double rshares_sum = 0.0;    
                    for(auto& v : m.votes)
                    {   
                        if(v.weight > 0.0)
                          rshares_sum += v.weight * v.vesting;
                        double new_cur_fn = p.rules.curationfunc(rshares_sum);
                        cur_rewards.emplace_back(std::make_pair(v.voter, new_cur_fn - curators_fn_sum));
                        curators_fn_sum = new_cur_fn;
                    }  
                    if(curators_fn_sum > 1.e-20)
                        for(auto& r : cur_rewards)
                            r.second *= (curation_payout / curators_fn_sum);
                    
                    //TODO: different payment modes
                    for(auto& r : cur_rewards)
                        _state.balances[r.first].tokenamount += r.second;
                        
                    _state.balances[m.key.author].tokenamount += payout - curation_payout;
                    
                    p.messages.erase(itr_m);
                    p.funds -= (payout - unclaimed_funds);
                    
                    if(p.messages.size() == 0) {
                        auto itr_p_next = itr_p;
                        if((++itr_p_next) != _state.pools.end()) {                
                           itr_p_next->funds += p.funds;
                           _state.pools.erase(itr_p);
                        }
                    }               
                                     
                    return ret;
                }
            }
        }              
        if(ret == success()) 
                BOOST_REQUIRE_MESSAGE(false, "payrewards: ret == success(), but message not found in state");
        return ret;    
    }
    
    void fill_from_state(statemap& s) const {
        s.clear();            
        for(auto& b : _state.balances)
            s.set_balance(b.first, {b.second.tokenamount, b.second.vestamount});
            
        for(auto& p : _state.pools) {
            double pool_rshares_sum = 0.0;
            double pool_rsharesfn_sum = 0.0;
            
            for(auto& m : p.messages) {
                double absshares = 0.0;
                double netshares = 0.0;
                double voteshares = 0.0;
                for(auto& v : m.votes) {                    
                    absshares +=  std::abs(v.weight) * v.vesting;
                    double currshares = v.weight * v.vesting;
                    netshares += currshares;                    
                    if(v.weight > 0.0)
                        voteshares += currshares;                    
                }                
                s.set_message(m.key, {absshares, netshares, voteshares, p.rules.curationfunc(voteshares)});           
                pool_rshares_sum += netshares;               
                pool_rsharesfn_sum += p.rules.mainfunc(netshares);
            }
            s.set_pool(p.id, {static_cast<double>(p.messages.size()), p.funds, pool_rshares_sum, pool_rsharesfn_sum});
        }
    }
    
    void fill_from_tables(statemap& s) const {
        s.clear();
        //balances:
        { 
            auto rows = get_all_rows(_contract_acc, _contract_acc, N(balances), false);
            for(auto itr = rows.begin(); itr != rows.end(); ++itr) {
                auto cur = binary_to_variant("balance", *itr);
                s.set_balance(cur["user"].as<account_name>(), {
                    static_cast<double>(cur["tokenamount"].as<int64_t>()),
                    static_cast<double>(cur["vestamount"].as<int64_t>())
                });
            }
        }      
        //pools
        {     
            auto rows = get_all_rows(_contract_acc, _contract_acc, N(rewardpools), false);
            for(auto itr = rows.begin(); itr != rows.end(); ++itr) { 
                auto cur = binary_to_variant("rewardpool", *itr);
                 s.set_pool(cur["created"].as<uint64_t>(), {
                    static_cast<double>(cur["state"]["msgs"].as<uint64_t>()),
                    static_cast<double>(cur["state"]["funds"].as<int64_t>()),
                    static_cast<double>(FP(cur["state"]["rshares"].as<base_t>())),
                    static_cast<double>(FP(cur["state"]["rsharesfn"].as<base_t>()))
                });
            }
        }
        //messages
        { 
            for(auto& user : _users) {
                auto rows = get_all_rows(_contract_acc, user, N(messages), false);
                for(auto itr = rows.begin(); itr != rows.end(); ++itr) {
                    auto cur = binary_to_variant("message", *itr);
                    s.set_message(message_key{user, cur["id"].as<uint64_t>()}, {
                        static_cast<double>(FP(cur["state"]["absshares"].as<base_t>())),
                        static_cast<double>(FP(cur["state"]["netshares"].as<base_t>())), 
                        static_cast<double>(FP(cur["state"]["voteshares"].as<base_t>())), 
                        static_cast<double>(FP(cur["state"]["sumcuratorsw"].as<base_t>()))
                    }); 
                }            
            }
        } 
    }
    
    void check(const std::string& s = std::string()) {
        if(!s.empty())
            BOOST_TEST_MESSAGE(s);
        step();
        fill_from_tables(_res);
        fill_from_state(_req);
        if(_res != _req) {
            BOOST_TEST_MESSAGE( "_res != _req\n diff: " );
            _res.remove_same(_req);
            BOOST_TEST_MESSAGE(_res);
            BOOST_TEST_MESSAGE( "_req:" );
            BOOST_TEST_MESSAGE(_req);
            BOOST_REQUIRE(false);
        }
    }
};

BOOST_AUTO_TEST_SUITE(reward_calcs_tests)

BOOST_FIXTURE_TEST_CASE(basic_tests, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE( "reward_calcs_tester: basic_tests" );
    _req.clear();
    _res.clear();
    step();
    BOOST_REQUIRE_EQUAL(success(), payto(_users[0], 13666, payment_t::VESTING));
    for(auto& u : _users)
        BOOST_REQUIRE_EQUAL(success(), payto(u, 50000, payment_t::VESTING));
    check();
    
    auto bignum = static_cast<base_t>(1.e13); 
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check monotonic failed for time penalty func",
        setrules({"x", bignum}, {"log2(x + 1.0)", bignum}, {"x", bignum}, 2500, 
        [](double x){ return x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return x; }));
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::positive_save_cast: arg > max possible value",
        setrules({"x", std::numeric_limits<base_t>::max()}, {"sqrt(x)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 0.0; })); 
        
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check positive failed for time penalty func", 
        setrules({"x", bignum}, {"sqrt(x)", bignum}, {"10.0-x", 15}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 10.0 - x; }));        
    
    BOOST_REQUIRE_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"10.0-x", 10}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 10.0 - x; }));    
    check("set first rule");
    
    BOOST_REQUIRE_EQUAL(success(), issue(50000));    
    check("issue 50");
    
    std::vector<message_key> msg_keys;  
    msg_keys.emplace_back(message_key{ .author = _users[0], .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back()));   
    check("new_msg");
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check monotonic failed for reward func",
        setrules({"x * x", bignum}, {"log2(x + 1.0)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x * x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 0.0; }));
  
     
    BOOST_REQUIRE_EQUAL(success(), setrules({"x * x", static_cast<base_t>(sqrt(bignum))}, {"log2(x + 1.0)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x * x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 0.0; }));
    check("set second rule");            
    
    msg_keys.emplace_back(message_key{ .author = N(alice5), .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back()));    
    check("new_msg1");
    msg_keys.emplace_back(message_key{ .author = N(bob), .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back()));        
    check("new_msg2"); 
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[1], -5000));
    check("new_votes-5a");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[2], 7000));
    check("new_votes7a");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[3], 8000));
    check("new_votes8a");
          
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[1], 5000));
    check("new_votes5b");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[2], 7000));
    check("new_votes7b");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[3], 8000));
    check("new_votes8b");
    
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[2], N(bob1), 10000));
    check("new_votes10c");
    
    BOOST_REQUIRE_EQUAL(success(), issue(30000));
    check("issue 30");
    
    BOOST_REQUIRE_EQUAL(success(), payrewards(msg_keys[0]));
    check("payrewards");
    BOOST_REQUIRE_EQUAL(success(), payrewards(msg_keys[1]));
    check("payrewards1");   
      
    fill_from_tables(_res);
    BOOST_TEST_MESSAGE( "_res: \n" << _res );
    fill_from_state(_req);
    BOOST_TEST_MESSAGE( "_req: \n" << _req );
    
} FC_LOG_AND_RETHROW()

 
BOOST_AUTO_TEST_SUITE_END()
