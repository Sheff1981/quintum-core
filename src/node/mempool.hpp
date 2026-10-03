#pragma once

#include "chain/chainstate.hpp"
#include "primitives/transaction.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quintum {

inline constexpr std::size_t kMaxMempoolTransactions = 50'000U;
inline constexpr std::size_t kMaxMempoolBytes = 64U * 1024U * 1024U;

enum class MempoolError {
    none,
    chain_not_ready,
    height_overflow,
    coinbase_forbidden,
    duplicate,
    transaction_too_large,
    script_too_large,
    mempool_full,
    inconsistent_existing_pool,
    transaction_rejected,
};

struct MempoolAcceptResult {
    MempoolError error{MempoolError::none};
    UtxoApplyError transaction_error{UtxoApplyError::none};
    Hash256 txid{};
    Amount fee{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == MempoolError::none;
    }
};

struct MempoolEntry {
    Transaction transaction{};
    Hash256 txid{};
    Amount fee{0U};
    std::size_t serialized_size{0U};
};

class Mempool {
public:
    [[nodiscard]] MempoolAcceptResult accept(
        const Chainstate& chain,
        const Transaction& transaction
    );

    // Revalidates every surviving entry against the current active UTXO view.
    // Transactions confirmed, conflicted or invalidated by a reorg are dropped.
    void reconcile(const Chainstate& chain);

    [[nodiscard]] bool contains(
        const Hash256& txid
    ) const noexcept;

    [[nodiscard]] const Transaction* transaction(
        const Hash256& txid
    ) const noexcept;

    [[nodiscard]] std::vector<Transaction> transactions(
        std::size_t limit = kMaxMempoolTransactions
    ) const;

    [[nodiscard]] std::vector<Hash256> transaction_ids(
        std::size_t limit = kMaxMempoolTransactions
    ) const;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t total_bytes() const noexcept;

    [[nodiscard]] const std::vector<MempoolEntry>&
    entries() const noexcept;

    void clear() noexcept;

private:
    [[nodiscard]] std::optional<std::uint32_t> next_height(
        const Chainstate& chain
    ) const noexcept;

    std::vector<MempoolEntry> entries_{};
    std::size_t total_bytes_{0U};
};

} // namespace quintum
