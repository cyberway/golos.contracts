add_contract(golos.referral golos.referral src/golos.referral.cpp)
install_contract(golos.referral)
target_include_directories(golos.referral.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
