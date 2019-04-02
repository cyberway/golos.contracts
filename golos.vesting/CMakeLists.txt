add_contract_with_abi(golos.vesting golos.vesting.abi golos.vesting.cpp)

target_include_directories(golos.vesting.wasm
   PUBLIC
  #  ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   ${CMAKE_CURRENT_SOURCE_DIR}/../${GOLOS_TOKEN_INCLUDE_PATH})
