set(CMAKE_CXX_FLAGS " -abigen ${CMAKE_CXX_FLAGS} -std=c++17")
add_executable(golos.config.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/golos.config.cpp)
target_include_directories(golos.config.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../golos.emit/include
   ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(golos.config.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/abi/golos.config.abi" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

install (TARGETS golos.config.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.config/)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/golos.config.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/golos.config/)
