add_contract_with_abi(golos.social golos.social.abi golos.social.cpp)
target_include_directories(golos.social.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/cyber.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
