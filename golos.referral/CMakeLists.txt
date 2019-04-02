add_contract_with_abi(golos.referral abi/golos.referral.abi src/golos.referral.cpp)
target_include_directories(golos.referral.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../${GOLOS_TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
