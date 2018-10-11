#define UNIT_TEST_ENV
#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <math.h>
#include <map>
#include <Runtime/Runtime.h> 
#include <fc/variant_object.hpp>
#include "contracts.hpp"

#include "../reward_calcs/types.h"
#include "../reward_calcs/config.h"

#define PRECESION 0
#define TOKEN_NAME "GOLOS"
constexpr int64_t MAXTOKENPROB = 5000;
using namespace golos::config;
 
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
    balance_data balance {130.0, 130.0};
    pool_data pool {0.001, 170, -0.01, -0.01};
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
    
    void set_balance_token(account_name acc, double val) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "tokenamount") = { val, delta.balance.tokenamount };
    }
    
    void set_balance_vesting(account_name acc, double val) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "vestamount") = { val, delta.balance.vestamount };
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
    void update_cur_time() { _cur_time = control->head_block_time().time_since_epoch();};
    
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
    double tokenprop;
    std::list<vote> votes;
    double get_rshares_sum()const {
        double ret = 0.0;
        for(auto& v : votes)
            ret += v.weight * v.vesting;
        return ret;
    };
    message(message_key k, double tokenprop_) : key(k), tokenprop(tokenprop_) {};
};

class cutted_func {
    std::function<double(double)> _f;
public:
    cutted_func(std::function<double(double)>&& f) : _f(std::move(f)) {};
    double operator()(double arg)const {return _f(std::max(0.0, arg));};
};

struct rewardrules {
    cutted_func mainfunc;
    cutted_func curationfunc;
    cutted_func timepenalty;
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
    
    std::map<account_name, abi_serializer> _serializers;
    symbol _token_symbol;

    void setup_code(account_name account, const vector<uint8_t>& wasm_data, const std::vector<char>& abi_data) {        
        set_code(account, wasm_data);        
        set_abi(account, abi_data.data());  

        const auto& accnt = control->db().get<account_object, by_name>(account);
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        _serializers[account].set_abi(abi, abi_serializer_max_time);
    }
       
protected:    
    account_name _forum_name;
    account_name _issuer;
    std::vector<account_name> _users;
    statemap _req;
    statemap _res;
    state _state;
    
public: 
    reward_calcs_tester() : _token_symbol(PRECESION, TOKEN_NAME),
        _forum_name(N(reward.calcs)), _issuer(N(issuer.acc)),
        _users{ N(alice),  N(alice1),  N(alice2), N(alice3), N(alice4), N(alice5),
                N(bob), N(bob1), N(bob2), N(bob3), N(bob4), N(bob5),
                N(why), N(has), N(my), N(imagination), N(become), N(poor) } {
        step(2); 
        create_accounts({_forum_name});
        create_accounts({_issuer});
        create_accounts({vesting_name}); 
        create_accounts({N(eosio.token)});
        create_accounts(_users); 
        step(2);  
        
        setup_code(_forum_name, contracts::reward_calcs_wasm(), contracts::reward_calcs_abi());
        setup_code(N(eosio.token), contracts::token_wasm(), contracts::token_abi());
        setup_code(vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
         
        BOOST_REQUIRE_EQUAL(success(), push_action( N(eosio.token), N(create), 
            mvo()( "issuer", _issuer)( "maximum_supply", eosio::chain::asset(2000000000000, _token_symbol)), N(eosio.token)));
            
        step();     
            
        BOOST_REQUIRE_EQUAL(success(), push_action(_issuer, N(issue), 
            mvo()( "to", _issuer)( "quantity", eosio::chain::asset(500000000000, _token_symbol))
            ( "memo", "HERE COULD BE YOUR ADVERTISEMENT"), N(eosio.token)));
       
       step();   
       
       BOOST_REQUIRE_EQUAL(success(), push_action( _forum_name, N(open), 
            mvo()( "owner", _forum_name)
            ( "symbol", _token_symbol)
            ( "ram_payer", _forum_name), 
            N(eosio.token)));  
                   
       BOOST_REQUIRE_EQUAL(success(), push_action( _issuer, N(createvest), 
            mvo()
            ( "creator", _issuer)
            ( "symbol", _token_symbol)
            ( "issuers", std::vector<account_name>({_issuer, _forum_name})), 
            vesting_name));
        step();
        
        BOOST_REQUIRE_EQUAL(success(), push_action( _forum_name, N(open), 
            mvo()( "owner", _forum_name)
            ( "symbol", _token_symbol)
            ( "ram_payer", _forum_name), 
            vesting_name));    

        for(auto& u : _users) {
            BOOST_REQUIRE_EQUAL(success(), push_action( u, N(open), 
            mvo()( "owner", u)
            ( "symbol", _token_symbol)
            ( "ram_payer", u), 
            vesting_name));
            step(); 
            add_funds_to(u, 500000);
            step();   
            buy_vesting_for(u, 500000);
            step();   
        }
        check();      
    }
    
    action_result add_funds_to(account_name user, int64_t amount) {
        _state.balances[user].tokenamount  += amount;
        return push_action( _issuer, N(transfer), mvo()
                            ( "from", _issuer)
                            ( "to", user)
                            ( "quantity", eosio::chain::asset(amount, _token_symbol))
                            ( "memo", ""), 
                            N(eosio.token));
    }
    
    action_result buy_vesting_for(account_name user, int64_t amount) {
        _state.balances[user].tokenamount  -= amount;
        auto ret = push_action(user, N(transfer), 
                mvo()( "from", user)( "to", account_name(vesting_name))(
                    "quantity", eosio::chain::asset(amount, _token_symbol))( "memo", ""), 
                N(eosio.token));
                
        //we are not checking token->vesting conversion here, so:
        set_vesting_balance_from_table(user, _res);
        _state.balances[user].vestamount = _res[statemap::get_balance_str(user) + "vestamount"].val;
        
        return ret;
    }
    
    void fill_depleted_pool(int64_t amount, size_t excluded) {
        size_t choice = 0;
        auto min_ratio = std::numeric_limits<double>::max();
        for(size_t i = 0; i < _state.pools.size(); i++) 
            if(i != excluded) {
                double cur_ratio = _state.pools[i].get_ratio();
                if(cur_ratio <= min_ratio) {
                    min_ratio = cur_ratio;
                    choice = i;
                }
            }
        _state.pools[choice].funds += amount; 
    }
         
    action_result add_funds_to_forum(int64_t amount) {
  
        BOOST_REQUIRE_MESSAGE(!_state.pools.empty(), "issue: _state.pools is empty");
        fill_depleted_pool(amount, _state.pools.size());        
        
        eosio::chain::asset quantity(amount, _token_symbol);
        return push_action( _issuer, N(transfer), mvo()
                            ( "from", _issuer)
                            ( "to", _forum_name)
                            ( "quantity", quantity)
                            ( "memo", ""), 
                            N(eosio.token));
    }
    
    action_result push_action( const account_name& signer, const action_name &name, const variant_object &data, const account_name& code ) {
        auto abi_ser = _serializers[code];
        string action_type_name = abi_ser.get_action_type(name);//(token_mode ? abi_ser_token : abi_ser).get_action_type(name);
        
        action act;
        act.account = code;
        act.name    = name;    
        act.data    = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);       
             
        auto ret = base_tester::push_action( std::move(act), uint64_t(signer));
        return ret;    
    }
         
    action_result setrules(      
        const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty,
        int64_t curatorsprop,
        std::function<double(double)>&&  mainfunc_,
        std::function<double(double)>&& curationfunc_,
        std::function<double(double)>&& timepenalty_   
        ) { 
        auto ret = push_action(_forum_name, N(setrules), mvo()                
                ("mainfunc", mvo()("str", mainfunc.str)("maxarg", mainfunc.maxarg))
                ("curationfunc", mvo()("str", curationfunc.str)("maxarg", curationfunc.maxarg))
                ("timepenalty", mvo()("str", timepenalty.str)("maxarg", timepenalty.maxarg))
                ("curatorsprop", curatorsprop)
                ("maxtokenprop", MAXTOKENPROB)
                ("tokensymbol", _token_symbol), _forum_name
                );
                
        if(ret == success()) {   
            double unclaimed_funds = 0.0;
            for(auto& p : _state.pools)
                if(!p.messages.size())
                    unclaimed_funds += p.funds;
            _state.pools.erase(std::remove_if(_state.pools.begin(), _state.pools.end(),
                    [](const rewardpool& p){return !p.messages.size();}),
                    _state.pools.end());       
            
            auto rows = get_all_rows(_forum_name, _forum_name, N(rewardpools), false);
            auto created = _serializers[_forum_name].binary_to_variant("rewardpool", *rows.rbegin(), abi_serializer_max_time)["created"].as<uint64_t>();        
            _state.pools.emplace_back(rewardpool(created, 
            rewardrules(std::move(mainfunc_), std::move(curationfunc_), std::move(timepenalty_), static_cast<double>(curatorsprop) / static_cast<double>(ONE_HUNDRED_PERCENT))));
            _state.pools.back().funds += unclaimed_funds;
        }
        return ret;
    }
    
    action_result addmessage(const message_key& key, int64_t tokenprop) {
        _state.pools.back().messages.emplace_back(message(key, static_cast<double>(std::min(tokenprop, MAXTOKENPROB)) / static_cast<double>(ONE_HUNDRED_PERCENT)));
         return push_action(key.author, N(addmessage), mvo()("author", key.author)("id", key.id)("tokenprop", tokenprop), _forum_name);
    }
    
    action_result addvote(const message_key& msg_key, account_name voter, int64_t weight) {
    
        auto ret = push_action(voter, N(addvote), mvo()
                    ("author", msg_key.author)
                    ("msgid", msg_key.id)
                    ("voter", voter)
                    ("weight", weight), _forum_name);
                    
        for(auto& p : _state.pools)            
            for(auto& m : p.messages)
                if(m.key == msg_key) {
                    m.votes.emplace_back(vote{voter, std::min(static_cast<double>(weight) / static_cast<double>(ONE_HUNDRED_PERCENT), 1.0), _state.balances[voter].vestamount});
                    return ret;
                }
        
        if(ret == success())
            BOOST_REQUIRE_MESSAGE(false, "addvote: ret == success(), but message not found in state");
        return ret;    
    }
    
    action_result payrewards(const message_key& msg_key) {
        
        auto ret = push_action(_forum_name, N(payrewards), mvo()
                    ("author", msg_key.author)
                    ("id", msg_key.id), _forum_name);    
        
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
                    
                    for(auto& r : cur_rewards)
                        _state.balances[r.first].vestamount += r.second;
                    
                    auto author_payout =  payout - curation_payout;
                    _state.balances[m.key.author].tokenamount += author_payout * m.tokenprop;
                    _state.balances[m.key.author].vestamount += author_payout * (1.0 - m.tokenprop);
                    
                    p.messages.erase(itr_m);
                    p.funds -= (payout - unclaimed_funds);
                    
                    if(p.messages.size() == 0) {
                        auto itr_p_next = itr_p;
                        if((++itr_p_next) != _state.pools.end()) {                
                           fill_depleted_pool(p.funds, std::distance(_state.pools.begin(), itr_p));
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
    
    void set_token_balance_from_table(account_name user, statemap& s) {
         auto rows = get_all_rows(N(eosio.token), user, N(accounts), false);
        BOOST_REQUIRE_EQUAL(rows.size() <= 1, true); //for now we are checking just one token
        if(rows.empty())  
            s.set_balance_token(user, 0.0);
        else {
            auto cur = _serializers[N(eosio.token)].binary_to_variant("account", *rows.begin(), abi_serializer_max_time);
            s.set_balance_token(user, static_cast<double>(cur["balance"].as<eosio::chain::asset>().to_real())); 
        }            
    }
    
    void set_vesting_balance_from_table(account_name user, statemap& s) {
         auto rows = get_all_rows(vesting_name, user, N(balances), false);
        BOOST_REQUIRE_EQUAL(rows.size() <= 1, true); //--/---/---/---/---/---/ just one vesting
        if(rows.empty())  
            s.set_balance_vesting(user, 0.0);
        else {
            auto cur = _serializers[vesting_name].binary_to_variant("user_balance", *rows.begin(), abi_serializer_max_time);
            s.set_balance_vesting(user, static_cast<double>(cur["vesting"].as<eosio::chain::asset>().to_real())); 
        }            
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
    
    void fill_from_tables(statemap& s) {
        s.clear();
        
        for(auto& user : _users) {
            set_token_balance_from_table(user, s);
            set_vesting_balance_from_table(user, s);
            
        }        
        
        //pools
        {     
            auto rows = get_all_rows(_forum_name, _forum_name, N(rewardpools), false);
            for(auto itr = rows.begin(); itr != rows.end(); ++itr) { 
                auto cur = _serializers[_forum_name].binary_to_variant("rewardpool", *itr, abi_serializer_max_time);
                 s.set_pool(cur["created"].as<uint64_t>(), {
                    static_cast<double>(cur["state"]["msgs"].as<uint64_t>()),
                    //static_cast<double>(cur["state"]["funds"].as<int64_t>()),
                    static_cast<double>(cur["state"]["funds"].as<eosio::chain::asset>().to_real()),
                    static_cast<double>(FP(cur["state"]["rshares"].as<base_t>())),
                    static_cast<double>(FP(cur["state"]["rsharesfn"].as<base_t>()))
                });
            }
        }
         
        //messages
        { 
            for(auto& user : _users) {
                auto rows = get_all_rows(_forum_name, user, N(messages), false);
                for(auto itr = rows.begin(); itr != rows.end(); ++itr) {
                    auto cur = _serializers[_forum_name].binary_to_variant("message", *itr, abi_serializer_max_time);
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
        
        _res.remove_same(_req);
        if(!_res.empty()){
            BOOST_TEST_MESSAGE( "_res != _req\n diff: " );
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
    
    auto bignum = static_cast<base_t>(1.e13);   
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check monotonic failed for time penalty func",
        setrules({"x", bignum}, {"log2(x + 1.0)", bignum}, {"x", bignum}, 2500, 
        [](double x){ return x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return x; }));
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::positive_safe_cast: arg > max possible value",
        setrules({"x", std::numeric_limits<base_t>::max()}, {"sqrt(x)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 0.0; })); 
          
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check positive failed for time penalty func", 
        setrules({"x", bignum}, {"sqrt(x)", bignum}, {"10.0-x", 15}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 10.0 - x; }));        
    
    BOOST_REQUIRE_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"10.0-x", 10}, 2500, 
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 10.0 - x; }));    
    check("set first rule");
    
    BOOST_REQUIRE_EQUAL(success(), add_funds_to_forum(50000));    
    check("add_funds_to_forum 50");
    
    std::vector<message_key> msg_keys;  
    msg_keys.emplace_back(message_key{ .author = _users[0], .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back(), 5000));   
    check("new_msg");
    
    BOOST_REQUIRE_EQUAL("assertion failure with message: forum::check monotonic failed for reward func",
        setrules({"x^2", bignum}, {"log2(x + 1.0)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x * x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 0.0; }));
  
    BOOST_REQUIRE_EQUAL(success(), setrules({"x^2", static_cast<base_t>(sqrt(bignum))}, {"log2(x + 1.0)", bignum}, {"0", bignum}, 2500, 
        [](double x){ return x * x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 0.0; }));
    check("set second rule");            
    
    msg_keys.emplace_back(message_key{ .author = N(alice5), .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back(), 10000));    
    check("new_msg1");
    msg_keys.emplace_back(message_key{ .author = N(bob), .id = static_cast<uint64_t>(cur_time().to_seconds()) });     
    BOOST_REQUIRE_EQUAL(success(), addmessage(msg_keys.back(), 1000));        
    check("new_msg2"); 
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[1], -5000));
    check("new_votes-5a");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[2], 7000));
    check("new_votes7a");
    
    BOOST_REQUIRE_EQUAL(success(), add_funds_to_forum(8000));
    check("add_funds_to_forum 8");
      
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[0], _users[3], 18000));
    check("new_votes18a");
    
    BOOST_REQUIRE_EQUAL(success(), add_funds_to_forum(9000));
    check("add_funds_to_forum 9");  
          
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[1], 5000));
    check("new_votes5b");
    
    BOOST_REQUIRE_EQUAL(success(), add_funds_to_forum(7000));
    check("add_funds_to_forum 7");  
    
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[2], 7000));
    check("new_votes7b");
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[1], _users[3], 8000));
    check("new_votes8b");
    
    BOOST_REQUIRE_EQUAL(success(), addvote(msg_keys[2], N(bob1), 10000));
    check("new_votes10c");  
    
    BOOST_REQUIRE_EQUAL(success(), add_funds_to_forum(30000));
    check("add_funds_to_forum 30");  
    
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
