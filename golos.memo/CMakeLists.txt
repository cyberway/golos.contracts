add_contract_with_abi(golos.memo golos.memo.abi golos.memo.cpp)
target_include_directories(golos.memo.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
