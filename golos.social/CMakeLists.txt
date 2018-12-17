add_executable(golos.social.wasm ${CMAKE_CURRENT_SOURCE_DIR}/golos.social.cpp)
target_include_directories(golos.social.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.social.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB ABI_FILES "*.abi")
configure_file("${ABI_FILES}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

install (TARGETS golos.social.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.social/)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/golos.social.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.social/)
