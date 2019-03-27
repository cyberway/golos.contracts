add_contract_with_abi(golos.config abi/golos.config.abi src/golos.config.cpp)
target_include_directories(golos.config.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.emit/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
