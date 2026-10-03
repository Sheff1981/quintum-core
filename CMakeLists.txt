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
find_package(Threads REQUIRED)

option(
    QUINTUM_BUILD_GUI
    "Build the Qt 6 desktop wallet"
    OFF
)

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

FetchContent_Declare(
    monocypher_source
    URL https://github.com/LoupVaillant/Monocypher/releases/download/4.0.3/monocypher-4.0.3.tar.gz
    URL_HASH SHA256=8cc9bc341a66249016db9bd70e9142d8d0aef9945973744b1ac05dbc55d8ee66
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(monocypher_source)

add_library(monocypher STATIC
    ${monocypher_source_SOURCE_DIR}/src/monocypher.c
    ${monocypher_source_SOURCE_DIR}/src/optional/monocypher-ed25519.c
)
target_include_directories(monocypher PUBLIC
    ${monocypher_source_SOURCE_DIR}/src
    ${monocypher_source_SOURCE_DIR}/src/optional
)

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
    src/crypto/random.cpp
    src/consensus/tx_auth.cpp
    src/primitives/transaction.cpp
    src/chain/utxo.cpp
    src/primitives/block.cpp
    src/consensus/pow.cpp
    src/consensus/chainparams.cpp
    src/consensus/difficulty.cpp
    src/consensus/genesis.cpp
    src/consensus/block_limits.cpp
    src/consensus/time.cpp
    src/consensus/monetary.cpp
    src/chain/chainstate.cpp
    src/chain/storage.cpp
    src/mining/block_template.cpp
    src/node/node.cpp
    src/node/mempool.cpp
    src/policy/fees.cpp
    src/net/protocol.cpp
    src/net/peer.cpp
    src/net/address.cpp
    src/net/discovery.cpp
    src/net/sync.cpp
    src/net/relay.cpp
    src/net/runtime.cpp
    src/wallet/address.cpp
    src/wallet/mnemonic.cpp
    src/wallet/metadata.cpp
    src/wallet/file_security.cpp
    src/wallet/secure.cpp
    src/wallet/fee_policy.cpp
    src/wallet/index.cpp
    src/wallet/wallet.cpp
)
target_compile_features(quintum_core PUBLIC cxx_std_23)
target_include_directories(quintum_core PUBLIC src)
target_link_libraries(quintum_core PUBLIC secp256k1 monocypher)
if(WIN32)
    target_link_libraries(quintum_core PUBLIC ws2_32 bcrypt advapi32)
endif()
quintum_strict_warnings(quintum_core)

add_executable(quintumd
    src/main.cpp
)
target_link_libraries(quintumd PRIVATE quintum_core)
quintum_strict_warnings(quintumd)

if(QUINTUM_BUILD_GUI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
    qt_standard_project_setup()

    qt_add_executable(
        quintum_qt
        MANUAL_FINALIZATION
        src/qt/main.cpp
        src/qt/mainwindow.cpp
        src/qt/mainwindow.hpp
    )

    target_link_libraries(
        quintum_qt
        PRIVATE
            quintum_core
            Threads::Threads
            Qt6::Widgets
    )

    quintum_strict_warnings(quintum_qt)

    set_target_properties(
        quintum_qt
        PROPERTIES
            OUTPUT_NAME "QUINTUM"
            WIN32_EXECUTABLE ON
            MACOSX_BUNDLE ON
    )

    qt_finalize_executable(quintum_qt)
endif()

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

add_executable(quintum_difficulty_tests
    tests/difficulty_tests.cpp
)
target_link_libraries(quintum_difficulty_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_difficulty_tests)
quintum_test_assertions(quintum_difficulty_tests)

add_test(NAME difficulty COMMAND quintum_difficulty_tests)

add_executable(quintum_timestamp_limits_tests
    tests/timestamp_limits_tests.cpp
)
target_link_libraries(quintum_timestamp_limits_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_timestamp_limits_tests)
quintum_test_assertions(quintum_timestamp_limits_tests)

add_test(NAME timestamp_limits COMMAND quintum_timestamp_limits_tests)

add_executable(quintum_genesis_tests
    tests/genesis_tests.cpp
)
target_link_libraries(quintum_genesis_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_genesis_tests)
quintum_test_assertions(quintum_genesis_tests)

add_test(NAME genesis COMMAND quintum_genesis_tests)

add_executable(quintum_chainstate_tests
    tests/chainstate_tests.cpp
)
target_link_libraries(quintum_chainstate_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_chainstate_tests)
quintum_test_assertions(quintum_chainstate_tests)

add_test(NAME chainstate COMMAND quintum_chainstate_tests)


add_executable(quintum_storage_tests
    tests/storage_tests.cpp
)
target_link_libraries(quintum_storage_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_storage_tests)
quintum_test_assertions(quintum_storage_tests)

add_test(NAME storage COMMAND quintum_storage_tests)

add_executable(quintum_node_runtime_tests
    tests/node_runtime_tests.cpp
)
target_link_libraries(quintum_node_runtime_tests PRIVATE quintum_core)
quintum_strict_warnings(quintum_node_runtime_tests)
quintum_test_assertions(quintum_node_runtime_tests)

add_test(NAME node_runtime COMMAND quintum_node_runtime_tests)

add_executable(quintum_p2p_tests
    tests/p2p_tests.cpp
)
target_link_libraries(
    quintum_p2p_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_p2p_tests)
quintum_test_assertions(quintum_p2p_tests)

add_test(NAME p2p COMMAND quintum_p2p_tests)

add_executable(quintum_discovery_tests
    tests/discovery_tests.cpp
)
target_link_libraries(
    quintum_discovery_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_discovery_tests)
quintum_test_assertions(quintum_discovery_tests)

add_test(NAME discovery COMMAND quintum_discovery_tests)

add_executable(quintum_sync_tests
    tests/sync_tests.cpp
)
target_link_libraries(
    quintum_sync_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_sync_tests)
quintum_test_assertions(quintum_sync_tests)

add_test(NAME sync COMMAND quintum_sync_tests)

add_executable(quintum_relay_tests
    tests/relay_tests.cpp
)
target_link_libraries(
    quintum_relay_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_relay_tests)
quintum_test_assertions(quintum_relay_tests)

add_test(NAME relay COMMAND quintum_relay_tests)

add_executable(quintum_network_runtime_tests
    tests/network_runtime_tests.cpp
)
target_link_libraries(
    quintum_network_runtime_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_network_runtime_tests)
quintum_test_assertions(quintum_network_runtime_tests)

add_test(NAME network_runtime COMMAND quintum_network_runtime_tests)


add_executable(quintum_wallet_tests
    tests/wallet_tests.cpp
)
target_link_libraries(
    quintum_wallet_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_wallet_tests)
quintum_test_assertions(quintum_wallet_tests)

add_test(NAME wallet COMMAND quintum_wallet_tests)

add_executable(quintum_stage22_wallet_tests
    tests/stage22_wallet_tests.cpp
)
target_link_libraries(
    quintum_stage22_wallet_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage22_wallet_tests)
quintum_test_assertions(quintum_stage22_wallet_tests)

add_test(NAME stage22_wallet COMMAND quintum_stage22_wallet_tests)

add_executable(quintum_stage23_recovery_tests
    tests/stage23_recovery_tests.cpp
)
target_link_libraries(
    quintum_stage23_recovery_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage23_recovery_tests)
quintum_test_assertions(quintum_stage23_recovery_tests)

add_test(NAME stage23_recovery COMMAND quintum_stage23_recovery_tests)

add_executable(quintum_stage24_fee_policy_tests
    tests/stage24_fee_policy_tests.cpp
)
target_link_libraries(
    quintum_stage24_fee_policy_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage24_fee_policy_tests)
quintum_test_assertions(quintum_stage24_fee_policy_tests)

add_test(NAME stage24_fee_policy COMMAND quintum_stage24_fee_policy_tests)

add_executable(quintum_stage25_desktop_wallet_tests
    tests/stage25_desktop_wallet_tests.cpp
)
target_link_libraries(
    quintum_stage25_desktop_wallet_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage25_desktop_wallet_tests)
quintum_test_assertions(quintum_stage25_desktop_wallet_tests)

add_test(NAME stage25_desktop_wallet COMMAND quintum_stage25_desktop_wallet_tests)

add_executable(quintum_stage27_desktop_operations_tests
    tests/stage27_desktop_operations_tests.cpp
)
target_link_libraries(
    quintum_stage27_desktop_operations_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage27_desktop_operations_tests)
quintum_test_assertions(quintum_stage27_desktop_operations_tests)

add_test(
    NAME stage27_desktop_operations
    COMMAND quintum_stage27_desktop_operations_tests
)

add_executable(quintum_stage28_release_hardening_tests
    tests/stage28_release_hardening_tests.cpp
)
target_link_libraries(
    quintum_stage28_release_hardening_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage28_release_hardening_tests)
quintum_test_assertions(quintum_stage28_release_hardening_tests)

add_test(
    NAME stage28_release_hardening
    COMMAND quintum_stage28_release_hardening_tests
)
add_executable(quintum_stage29_public_testnet_tests
    tests/stage29_public_testnet_tests.cpp
)
target_link_libraries(
    quintum_stage29_public_testnet_tests
    PRIVATE quintum_core Threads::Threads
)
quintum_strict_warnings(quintum_stage29_public_testnet_tests)
quintum_test_assertions(quintum_stage29_public_testnet_tests)

add_test(
    NAME stage29_public_testnet
    COMMAND quintum_stage29_public_testnet_tests
)
