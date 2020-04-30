cmake_minimum_required(VERSION 3.5)

if (NOT DEFINED PROJECT_NAME)
    project(golos_dapp_contracts VERSION 0.1.0)
endif(NOT DEFINED PROJECT_NAME)

set(CYBERWAY_CDT_VERSION_MIN "1.6")
set(CYBERWAY_CDT_VERSION_SOFT_MAX "1.7")
#set(CYBERWAY_CDT_VERSION_HARD_MAX "")

find_package(cyberway.cdt)

### Check the version of cyberway.cdt
set(VERSION_MATCH_ERROR_MSG "")
CYBERWAY_CHECK_VERSION(VERSION_OUTPUT "${CYBERWAY_CDT_VERSION}"
                                   "${CYBERWAY_CDT_VERSION_MIN}"
                                   "${CYBERWAY_CDT_VERSION_SOFT_MAX}"
                                   "${CYBERWAY_CDT_VERSION_HARD_MAX}"
                                   VERSION_MATCH_ERROR_MSG)
if(VERSION_OUTPUT STREQUAL "MATCH")
   message(STATUS "Using cyberway.cdt version ${CYBERWAY_CDT_VERSION}")
elseif(VERSION_OUTPUT STREQUAL "WARN")
   message(WARNING "Using cyberway.cdt version ${CYBERWAY_CDT_VERSION} even though it exceeds the maximum supported version of ${CYBERWAY_CDT_VERSION_SOFT_MAX}; continuing with configuration, however build may fail.\nIt is recommended to use cyberway.cdt version ${CYBERWAY_CDT_VERSION_SOFT_MAX}.x")
else() # INVALID OR MISMATCH
   message(FATAL_ERROR "Found cyberway.cdt version ${CYBERWAY_CDT_VERSION} but it does not satisfy version requirements: ${VERSION_MATCH_ERROR_MSG}\nPlease use cyberway.cdt version ${CYBERWAY_CDT_VERSION_SOFT_MAX}.x")
endif(VERSION_OUTPUT STREQUAL "MATCH")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(TEST_BUILD_TYPE "Debug")
   set(CMAKE_BUILD_TYPE "Release")
else()
   set(TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

macro(install_contract TARGET)
   install (TARGETS ${TARGET}.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/${TARGET}/)
   install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/${TARGET}/)
endmacro()

macro(add_contract_with_checked_abi CONTRACT_NAME TARGET ABIFILE)
    add_contract(${CONTRACT_NAME} ${TARGET} ${ARGN})
    get_target_property(BINOUTPUT ${TARGET}.wasm BINARY_DIR)
    if(ABICHECK STREQUAL "true")
      add_custom_command(TARGET ${TARGET}.wasm POST_BUILD
         COMMAND ${PROJECT_SOURCE_DIR}/scripts/deployutils/abiprinter.py <${BINOUTPUT}/${TARGET}.abi >${BINOUTPUT}/${TARGET}.abi.pretty)
      add_custom_target(${TARGET}.abicheck ALL
         COMMAND ${CYBERWAY_ABIDIFF} ${CMAKE_CURRENT_SOURCE_DIR}/${ABIFILE} ${BINOUTPUT}/${TARGET}.abi
         DEPENDS ${TARGET}.wasm ${ABIFILE}
      )
    else()
      add_custom_command(TARGET ${TARGET}.wasm POST_BUILD
      COMMAND ${PROJECT_SOURCE_DIR}/scripts/deployutils/abiprinter.py <${BINOUTPUT}/${TARGET}.abi >${CMAKE_CURRENT_SOURCE_DIR}/${ABIFILE})
   endif()
endmacro()

macro(add_contract_with_abi TARGET ABIFILE)
   add_executable( ${TARGET}.wasm ${ARGN} )
   target_compile_options( ${TARGET}.wasm PUBLIC -std=c++17)
   configure_file(${ABIFILE} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.abi COPYONLY)
   install_contract(${TARGET})
endmacro()

if (NOT DEFINED TOKEN_INCLUDE_PATH)
    set(TOKEN_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cyberway.contracts/cyber.token/include")
endif(NOT DEFINED TOKEN_INCLUDE_PATH)

if (NOT DEFINED TOKEN_HEADER_FILE)
    set(TOKEN_HEADER_FILE "cyber.token/cyber.token.hpp")
endif(NOT DEFINED TOKEN_HEADER_FILE)

if (NOT DEFINED TOKEN_CLASS)
    set(TOKEN_CLASS "eosio::token")
endif(NOT DEFINED TOKEN_CLASS)

if (NOT DEFINED TOKEN_NAME)
    set(TOKEN_NAME "cyber.token")
endif(NOT DEFINED TOKEN_NAME)

if (NOT DEFINED VESTING_NAME)
    set(VESTING_NAME "gls.vesting")
endif(NOT DEFINED VESTING_NAME)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/config.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/common/config.hpp)
set(CYBERWAY_ABIDIFF ${CYBERWAY_CDT_ROOT}/bin/cyberway-abidiff)

# include_directories(AFTER ${BOOST_ROOT}/include)
add_subdirectory(golos.ctrl)
add_subdirectory(golos.emit)
add_subdirectory(golos.config)
add_subdirectory(golos.vesting)
add_subdirectory(golos.publication)
add_subdirectory(golos.charge)
add_subdirectory(golos.social)
add_subdirectory(golos.referral)
add_subdirectory(golos.memo)

if (APPLE)
   set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
   set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

include(UnitTestsExternalProject.txt)
