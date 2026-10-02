cmake_minimum_required(VERSION 3.25)

project(quintum_core
    VERSION 0.0.1
    DESCRIPTION "QUINTUM Core"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(quintum_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

add_library(quintum_core STATIC
    src/crypto/sha256.cpp
    src/primitives/transaction.cpp
    src/chain/utxo.cpp
    src/primitives/block.cpp
    src/consensus/pow.cpp
    src/consensus/monetary.cpp
    src/chain/chainstate.cpp
)
target_compile_features(quintum_core PUBLIC cxx_std_23)
target_include_directories(quintum_core PUBLIC src)
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

add_test(NAME foundation COMMAND quintum_tests)

add_executable(quintum_tx_tests
    tests/transaction_tests.cpp
)
target_link_libraries(quintum_tx_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_tx_tests)

add_test(NAME transaction COMMAND quintum_tx_tests)

add_executable(quintum_utxo_tests
    tests/utxo_tests.cpp
)
target_link_libraries(quintum_utxo_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_utxo_tests)

add_test(NAME utxo COMMAND quintum_utxo_tests)

add_executable(quintum_block_tests
    tests/block_tests.cpp
)
target_link_libraries(quintum_block_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_block_tests)

add_test(NAME block COMMAND quintum_block_tests)

add_executable(quintum_pow_tests
    tests/pow_tests.cpp
)
target_link_libraries(quintum_pow_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_pow_tests)

add_test(NAME pow COMMAND quintum_pow_tests)

add_executable(quintum_chainstate_tests
    tests/chainstate_tests.cpp
)
target_link_libraries(quintum_chainstate_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_chainstate_tests)

add_test(NAME chainstate COMMAND quintum_chainstate_tests)
