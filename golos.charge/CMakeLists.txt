add_contract_with_abi(golos.charge golos.charge.abi golos.charge.cpp)
target_include_directories(golos.charge.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
