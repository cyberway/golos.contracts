add_contract_with_checked_abi(golos.emit golos.emit golos.emit.abi src/golos.emit.cpp)
install_contract(golos.emit)
target_include_directories(golos.emit.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..
   )
