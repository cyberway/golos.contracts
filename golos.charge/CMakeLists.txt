add_contract_with_abi(golos.charge golos.charge.abi golos.charge.cpp)
target_include_directories(golos.charge.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
