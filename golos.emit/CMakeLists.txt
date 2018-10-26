set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} ")
add_executable(golos.emit.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/golos.emit.cpp)
target_include_directories(golos.emit.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.ctrl/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.contracts/eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.emit.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.emit.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
