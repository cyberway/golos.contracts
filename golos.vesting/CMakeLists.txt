add_contract_with_checked_abi(golos.vesting golos.vesting golos.vesting.abi golos.vesting.cpp)
install_contract(golos.vesting)

target_include_directories(golos.vesting.wasm
   PUBLIC
  #  ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   )
