add_executable(golos.charge.wasm ${CMAKE_CURRENT_SOURCE_DIR}/golos.charge.cpp)
target_include_directories(golos.charge.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.charge.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB ABI_FILES "*.abi")
configure_file("${ABI_FILES}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

install (TARGETS golos.charge.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.charge/)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/golos.charge.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.charge/)
