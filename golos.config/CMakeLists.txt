add_contract_with_checked_abi(golos.config golos.config golos.config.abi src/golos.config.cpp)
install_contract(golos.config)
target_include_directories(golos.config.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.emit/include
   )
