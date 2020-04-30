add_contract_with_checked_abi(golos.memo golos.memo golos.memo.abi golos.memo.cpp)
install_contract(golos.memo)
target_include_directories(golos.memo.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
