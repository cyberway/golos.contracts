add_contract_with_abi(golos.ctrl abi/golos.ctrl.abi src/golos.ctrl.cpp)
target_include_directories(golos.ctrl.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../${GOLOS_TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/cyber.system/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
