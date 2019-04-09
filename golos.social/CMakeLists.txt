add_contract_with_abi(golos.social golos.social.abi golos.social.cpp)
target_include_directories(golos.social.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
