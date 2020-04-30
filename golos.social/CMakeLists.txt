add_contract_with_checked_abi(golos.social golos.social golos.social.abi golos.social.cpp)
install_contract(golos.social)
target_include_directories(golos.social.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
