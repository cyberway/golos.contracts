add_contract(golos.publication golos.publication golos.publication.cpp)
install_contract(golos.publication)
target_include_directories(golos.publication.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.referral/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
