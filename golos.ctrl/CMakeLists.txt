add_contract_with_checked_abi(golos.ctrl golos.ctrl golos.ctrl.abi src/golos.ctrl.cpp)
install_contract(golos.ctrl)
target_include_directories(golos.ctrl.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/cyber.bios/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
