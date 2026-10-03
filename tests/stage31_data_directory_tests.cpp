#include "consensus/chainparams.hpp"
#include "net/runtime.hpp"
#include "node/datadir.hpp"
#include "node/node.hpp"
#include "wallet/wallet.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path test_root(
    const char* suffix)
{
    const auto stamp =
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        (
            std::string{"quintum-stage31-"} +
            suffix +
            "-" +
            std::to_string(stamp)
        );
}

std::vector<unsigned char> read_bytes(
    const std::filesystem::path& path)
{
    std::ifstream input(
        path,
        std::ios::binary
    );
    assert(input);

    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
}

void write_text(
    const std::filesystem::path& path,
    const char* text)
{
    std::filesystem::create_directories(
        path.parent_path()
    );

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc
    );
    assert(output);
    output << text;
    assert(output);
}

void remove_tree(
    const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

void test_new_layout_and_network_only_policy()
{
    using namespace quintum;

    const auto root =
        test_root("layout");
    remove_tree(root);

    DataDirectoryLayout layout{root};

    assert(layout.prepare(false) ==
           DataDirectoryError::none);

    assert(std::filesystem::is_directory(
        layout.blocks_directory()
    ));
    assert(std::filesystem::is_directory(
        layout.chainstate_directory()
    ));
    assert(std::filesystem::is_directory(
        layout.indexes_directory()
    ));

    assert(!std::filesystem::exists(
        layout.wallets_directory()
    ));

    assert(layout.blocks_file() ==
           root / "blocks" / "blocks.dat");
    assert(layout.chainstate_file() ==
           root / "chainstate" / "chainstate.dat");
    assert(layout.wallet_file() ==
           root / "wallets" / "default" / "wallet.dat");
    assert(layout.peers_file() ==
           root / "peers.dat");

    // A network-only seed must not move or create wallet material.
    write_text(root / "wallet.dat", "legacy-secret-wallet");

    assert(layout.prepare(false) ==
           DataDirectoryError::none);
    assert(std::filesystem::exists(
        root / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        layout.wallet_file()
    ));

    remove_tree(root);
}

void test_legacy_flat_data_migrates_without_mutation()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto root =
        test_root("legacy");
    remove_tree(root);

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 10'000U;

    std::uint32_t expected_height{0U};
    Hash256 expected_tip{};
    std::string receive_address;

    {
        NodeRuntime legacy_node{
            params,
            root
        };

        const auto started =
            legacy_node.start_at(base_time);

        assert(started.ok());
        assert(legacy_node.chain().height());
        assert(legacy_node.chain().tip_hash());

        expected_height =
            *legacy_node.chain().height();
        expected_tip =
            *legacy_node.chain().tip_hash();

        Wallet legacy_wallet{
            params,
            root
        };

        const auto wallet_started =
            legacy_wallet.start(
                "stage31-migration-password"
            );

        assert(wallet_started.ok());
        receive_address =
            wallet_started.receive_address;

        assert(legacy_wallet.set_address_label(
                   receive_address,
                   "migration-check") ==
               WalletMetadataError::none);

        const auto sync =
            legacy_wallet.sync(
                legacy_node.chain(),
                legacy_node.mempool()
            );
        assert(sync.ok());
    }

    const auto original_blocks =
        read_bytes(root / "blocks.dat");
    const auto original_state =
        read_bytes(root / "chainstate.dat");
    const auto original_wallet =
        read_bytes(root / "wallet.dat");
    const auto original_wallet_state =
        read_bytes(root / "wallet_state.dat");
    const auto original_wallet_meta =
        read_bytes(root / "wallet_meta.dat");

    net::NetworkRuntime runtime{
        params,
        root
    };

    net::NetworkRuntimeConfig config;
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.wallet_passphrase =
        "stage31-migration-password";

    const auto runtime_started =
        runtime.start(std::move(config));

    assert(runtime_started.ok());

    const auto status =
        runtime.status();

    assert(status.height ==
           std::optional<std::uint32_t>{
               expected_height});
    assert(status.tip ==
           std::optional<Hash256>{
               expected_tip});
    assert(!status.receive_address.empty());

    runtime.stop();

    DataDirectoryLayout layout{root};

    assert(!std::filesystem::exists(
        root / "blocks.dat"
    ));
    assert(!std::filesystem::exists(
        root / "chainstate.dat"
    ));
    assert(!std::filesystem::exists(
        root / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        root / "wallet_state.dat"
    ));
    assert(!std::filesystem::exists(
        root / "wallet_meta.dat"
    ));

    assert(read_bytes(layout.blocks_file()) ==
           original_blocks);
    assert(read_bytes(layout.chainstate_file()) ==
           original_state);
    assert(read_bytes(layout.wallet_file()) ==
           original_wallet);
    assert(read_bytes(layout.wallet_state_file()) ==
           original_wallet_state);
    assert(read_bytes(layout.wallet_metadata_file()) ==
           original_wallet_meta);

    remove_tree(root);
}

void test_conflict_fails_closed()
{
    using namespace quintum;

    const auto root =
        test_root("conflict");
    remove_tree(root);

    DataDirectoryLayout layout{root};

    write_text(
        root / "wallet.dat",
        "legacy-wallet"
    );
    write_text(
        layout.wallet_file(),
        "new-wallet"
    );

    assert(layout.prepare(true) ==
           DataDirectoryError::conflict);

    const auto legacy =
        read_bytes(root / "wallet.dat");
    const auto current =
        read_bytes(layout.wallet_file());

    assert(std::string(
               legacy.begin(),
               legacy.end()) ==
           "legacy-wallet");
    assert(std::string(
               current.begin(),
               current.end()) ==
           "new-wallet");

    remove_tree(root);
}

} // namespace

int main()
{
    test_new_layout_and_network_only_policy();
    test_legacy_flat_data_migrates_without_mutation();
    test_conflict_fails_closed();
    return 0;
}
