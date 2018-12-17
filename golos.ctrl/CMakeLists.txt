set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} -std=c++17")
add_executable(golos.ctrl.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/golos.ctrl.cpp)
target_include_directories(golos.ctrl.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.system/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.ctrl.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.ctrl.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

install (TARGETS golos.ctrl.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.ctrl/)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/golos.ctrl.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.ctrl/)
