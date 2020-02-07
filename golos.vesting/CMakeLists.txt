add_contract(golos.vesting golos.vesting golos.vesting.cpp)
install_contract(golos.vesting)
target_include_directories(golos.vesting.wasm
   PUBLIC
  #  ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   )
