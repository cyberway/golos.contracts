set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} -std=c++17")
add_executable(golos.referral.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/golos.referral.cpp)
target_include_directories(golos.referral.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.referral.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.referral.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
