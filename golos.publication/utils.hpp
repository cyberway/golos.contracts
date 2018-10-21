#pragma once

template<typename T, typename F>
auto get_itr(T& tbl, uint64_t key, account_name payer, F&& insert_fn) {    
    auto itr = tbl.find(key);
    if(itr == tbl.end())
        itr = tbl.emplace(payer, insert_fn);
    return itr;    
}

fixp_t set_and_run(atmsp::machine<fixp_t>& machine, const atmsp::storable::bytecode& bc, const std::vector<fixp_t>& args, const std::vector<std::pair<fixp_t, fixp_t> >& domain = {}) {
    bc.to_machine(machine);
    return machine.run(args, domain);
}
