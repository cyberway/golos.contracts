add_contract_with_abi(golos.publication golos.publication.abi golos.publication.cpp)
target_include_directories(golos.publication.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/cyber.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.referral/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
