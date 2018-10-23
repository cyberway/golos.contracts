set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} ")
add_executable(reward_calcs.wasm ${CMAKE_CURRENT_SOURCE_DIR}/reward_calcs.cpp)
target_include_directories(reward_calcs.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../cyberway.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(reward_calcs.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB ABI_FILES "*.abi")
configure_file("${ABI_FILES}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
