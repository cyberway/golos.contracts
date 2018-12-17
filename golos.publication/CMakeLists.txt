set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} -std=c++17")
add_executable(golos.publication.wasm ${CMAKE_CURRENT_SOURCE_DIR}/golos.publication.cpp)
target_include_directories(golos.publication.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.vesting
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.publication.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB ABI_FILES "*.abi")
configure_file("${ABI_FILES}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

install (TARGETS golos.publication.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.publication/)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/golos.publication.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.publication/)
