add_executable(golos.vesting.wasm ${CMAKE_CURRENT_SOURCE_DIR}/golos.vesting.cpp)
target_include_directories(golos.vesting.wasm
   PUBLIC
  #  ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.vesting.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.vesting.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

file(GLOB ABI_FILES "*.abi")
configure_file("${ABI_FILES}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

