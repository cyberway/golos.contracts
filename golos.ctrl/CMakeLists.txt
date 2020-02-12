add_contract(golos.ctrl golos.ctrl src/golos.ctrl.cpp)
install_contract(golos.ctrl)
target_include_directories(golos.ctrl.wasm
   PUBLIC
   ${TOKEN_INCLUDE_PATH}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/cyber.bios/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)
