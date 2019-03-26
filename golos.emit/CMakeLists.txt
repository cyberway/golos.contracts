add_contract_with_abi(golos.emit abi/golos.emit.abi src/golos.emit.cpp)
target_include_directories(golos.emit.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
