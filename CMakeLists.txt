cmake_minimum_required(VERSION 3.25)

project(quintum_core
    VERSION 0.0.1
    DESCRIPTION "QUINTUM Core"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SECP256K1_INSTALL OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_CTIME_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_ECDH OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_RECOVERY OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_EXTRAKEYS OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_SCHNORRSIG OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_MUSIG OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_ELLSWIFT OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_SILENTPAYMENTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    secp256k1
    URL https://github.com/bitcoin-core/secp256k1/releases/download/v0.8.0/libsecp256k1-0.8.0.tar.gz
    URL_HASH SHA256=dd685546f9e717b9adde329acd5a4cd8083d40f710fdb0a603ee5a83f908132b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(secp256k1)

function(quintum_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

function(quintum_test_assertions target)
    # Release builds normally define NDEBUG and disable assert().
    # Consensus tests must execute their assertions on every CI platform.
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

add_library(quintum_core STATIC
    src/crypto/sha256.cpp
    src/crypto/secp256k1.cpp
    src/consensus/tx_auth.cpp
    src/primitives/transaction.cpp
    src/chain/utxo.cpp
    src/primitives/block.cpp
    src/consensus/pow.cpp
    src/consensus/monetary.cpp
    src/chain/chainstate.cpp
)
target_compile_features(quintum_core PUBLIC cxx_std_23)
target_include_directories(quintum_core PUBLIC src)
target_link_libraries(quintum_core PUBLIC secp256k1)
quintum_strict_warnings(quintum_core)

add_executable(quintumd
    src/main.cpp
)
target_link_libraries(quintumd PRIVATE quintum_core)
quintum_strict_warnings(quintumd)

enable_testing()

add_executable(quintum_tests
    tests/foundation_tests.cpp
)
target_link_libraries(quintum_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_tests)
quintum_test_assertions(quintum_tests)

add_test(NAME foundation COMMAND quintum_tests)

add_executable(quintum_tx_tests
    tests/transaction_tests.cpp
)
target_link_libraries(quintum_tx_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_tx_tests)
quintum_test_assertions(quintum_tx_tests)

add_test(NAME transaction COMMAND quintum_tx_tests)

add_executable(quintum_utxo_tests
    tests/utxo_tests.cpp
)
target_link_libraries(quintum_utxo_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_utxo_tests)
quintum_test_assertions(quintum_utxo_tests)

add_test(NAME utxo COMMAND quintum_utxo_tests)

add_executable(quintum_block_tests
    tests/block_tests.cpp
)
target_link_libraries(quintum_block_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_block_tests)
quintum_test_assertions(quintum_block_tests)

add_test(NAME block COMMAND quintum_block_tests)

add_executable(quintum_pow_tests
    tests/pow_tests.cpp
)
target_link_libraries(quintum_pow_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_pow_tests)
quintum_test_assertions(quintum_pow_tests)

add_test(NAME pow COMMAND quintum_pow_tests)

add_executable(quintum_monetary_tests
    tests/monetary_tests.cpp
)
target_link_libraries(quintum_monetary_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_monetary_tests)
quintum_test_assertions(quintum_monetary_tests)

add_test(NAME monetary COMMAND quintum_monetary_tests)

add_executable(quintum_crypto_tests
    tests/crypto_tests.cpp
)
target_link_libraries(quintum_crypto_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_crypto_tests)
quintum_test_assertions(quintum_crypto_tests)

add_test(NAME crypto COMMAND quintum_crypto_tests)

add_executable(quintum_tx_auth_tests
    tests/tx_auth_tests.cpp
)
target_link_libraries(quintum_tx_auth_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_tx_auth_tests)
quintum_test_assertions(quintum_tx_auth_tests)

add_test(NAME tx_auth COMMAND quintum_tx_auth_tests)

add_executable(quintum_chainstate_tests
    tests/chainstate_tests.cpp
)
target_link_libraries(quintum_chainstate_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_chainstate_tests)
quintum_test_assertions(quintum_chainstate_tests)

add_test(NAME chainstate COMMAND quintum_chainstate_tests)
