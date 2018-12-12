set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} -std=c++17")
add_executable(golos.referal.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/golos.referal.cpp)
target_include_directories(golos.referal.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.referal.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.referal.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
