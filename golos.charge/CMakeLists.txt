add_contract(golos.charge golos.charge golos.charge.cpp)
install_contract(golos.charge)
target_include_directories(golos.charge.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
