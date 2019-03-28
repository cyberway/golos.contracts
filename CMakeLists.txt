cmake_minimum_required(VERSION 3.5)
project(golos_dapp_contracts VERSION 0.1.0)

set(EOSIO_CDT_VERSION_MIN "1.4")
set(EOSIO_CDT_VERSION_SOFT_MAX "1.5")
#set(EOSIO_CDT_VERSION_HARD_MAX "")

find_package(eosio.cdt)

### Check the version of eosio.cdt
set(VERSION_MATCH_ERROR_MSG "")
EOSIO_CHECK_VERSION(VERSION_OUTPUT "${EOSIO_CDT_VERSION}"
                                   "${EOSIO_CDT_VERSION_MIN}"
                                   "${EOSIO_CDT_VERSION_SOFT_MAX}"
                                   "${EOSIO_CDT_VERSION_HARD_MAX}"
                                   VERSION_MATCH_ERROR_MSG)
if(VERSION_OUTPUT STREQUAL "MATCH")
   message(STATUS "Using eosio.cdt version ${EOSIO_CDT_VERSION}")
elseif(VERSION_OUTPUT STREQUAL "WARN")
   message(WARNING "Using eosio.cdt version ${EOSIO_CDT_VERSION} even though it exceeds the maximum supported version of ${EOSIO_CDT_VERSION_SOFT_MAX}; continuing with configuration, however build may fail.\nIt is recommended to use eosio.cdt version ${EOSIO_CDT_VERSION_SOFT_MAX}.x")
else() # INVALID OR MISMATCH
   message(FATAL_ERROR "Found eosio.cdt version ${EOSIO_CDT_VERSION} but it does not satisfy version requirements: ${VERSION_MATCH_ERROR_MSG}\nPlease use eosio.cdt version ${EOSIO_CDT_VERSION_SOFT_MAX}.x")
endif(VERSION_OUTPUT STREQUAL "MATCH")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(TEST_BUILD_TYPE "Debug")
   set(CMAKE_BUILD_TYPE "Release")
else()
   set(TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

macro(add_contract_with_abi TARGET ABIFILE)
   add_executable( ${TARGET}.wasm ${ARGN} )
   target_compile_options( ${TARGET}.wasm PUBLIC -abigen -std=c++17)
   configure_file(${ABIFILE} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.abi COPYONLY)
   install (TARGETS ${TARGET}.wasm DESTINATION ${CMAKE_INSTALL_PREFIX}/${TARGET}/)
   install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.abi DESTINATION ${CMAKE_INSTALL_PREFIX}/${TARGET}/)
endmacro()

set(TOKEN_INCLUDE_PATH "cyberway.contracts/cyber.token/include")
set(TOKEN_HEADER_FILE "cyber.token")
set(TOKEN_CLASS "eosio::token")
set(TOKEN_NAME "cyber.token")
set(VESTING_NAME "gls.vesting")

configure_file(${CMAKE_SOURCE_DIR}/common/config.hpp.in ${CMAKE_SOURCE_DIR}/common/config.hpp)

# include_directories(AFTER ${BOOST_ROOT}/include)
add_subdirectory(golos.ctrl)
add_subdirectory(golos.emit)
add_subdirectory(golos.config)
add_subdirectory(golos.vesting)
add_subdirectory(golos.publication)
add_subdirectory(golos.charge)
add_subdirectory(golos.social)
add_subdirectory(golos.referral)

if (APPLE)
   set(OPENSSL_ROOT "/usr/local/opt/openssl")
elseif (UNIX)
   set(OPENSSL_ROOT "/usr/include/openssl")
endif()
set(SECP256K1_ROOT "/usr/local")

include(UnitTestsExternalProject.txt)
