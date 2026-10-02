#include "node/mempool.hpp"

#include "consensus/monetary.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace quintum {

std::optional<std::uint32_t> Mempool::next_height(
    const Chainstate& chain) const noexcept
{
    const auto height = chain.height();
    if (!height ||
        *height ==
            std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return *height + 1U;
}

MempoolAcceptResult Mempool::accept(
    const Chainstate& chain,
    const Transaction& transaction)
{
    MempoolAcceptResult out;
    out.txid = transaction_id(transaction);

    const auto height = next_height(chain);
    if (!height) {
        out.error = chain.empty()
            ? MempoolError::chain_not_ready
            : MempoolError::height_overflow;
        return out;
    }

    if (transaction.is_coinbase()) {
        out.error = MempoolError::coinbase_forbidden;
        return out;
    }

    if (contains(out.txid)) {
        out.error = MempoolError::duplicate;
        return out;
    }

    const auto serialized_size =
        serialized_transaction_size(transaction);

    if (!serialized_size ||
        *serialized_size >
            static_cast<std::size_t>(
                chain.params().limits
                    .max_block_serialized_bytes)) {
        out.error = MempoolError::transaction_too_large;
        return out;
    }

    const auto script_limit =
        static_cast<std::size_t>(
            chain.params().limits.max_script_bytes);

    const bool input_script_too_large =
        std::any_of(
            transaction.inputs.begin(),
            transaction.inputs.end(),
            [&](const TxInput& input) {
                return input.unlocking_script.size() >
                       script_limit;
            }
        );

    const bool output_script_too_large =
        std::any_of(
            transaction.outputs.begin(),
            transaction.outputs.end(),
            [&](const TxOutput& output) {
                return output.locking_script.size() >
                       script_limit;
            }
        );

    if (input_script_too_large ||
        output_script_too_large) {
        out.error = MempoolError::script_too_large;
        return out;
    }

    if (entries_.size() >=
            kMaxMempoolTransactions ||
        *serialized_size >
            kMaxMempoolBytes - total_bytes_) {
        out.error = MempoolError::mempool_full;
        return out;
    }

    UtxoSet view = chain.utxos();

    for (const auto& entry : entries_) {
        const auto existing =
            view.apply_transaction(
                entry.transaction,
                *height
            );

        if (!existing.ok()) {
            out.error =
                MempoolError::inconsistent_existing_pool;
            out.transaction_error = existing.error;
            return out;
        }
    }

    const auto applied =
        view.apply_transaction(
            transaction,
            *height
        );

    if (!applied.ok()) {
        out.error = MempoolError::transaction_rejected;
        out.transaction_error = applied.error;
        return out;
    }

    if (!consensus::money_range(applied.fee)) {
        out.error = MempoolError::transaction_rejected;
        out.transaction_error =
            UtxoApplyError::money_out_of_range;
        return out;
    }

    out.fee = applied.fee;

    entries_.push_back(
        MempoolEntry{
            .transaction = transaction,
            .txid = out.txid,
            .fee = applied.fee,
            .serialized_size = *serialized_size,
        }
    );
    total_bytes_ += *serialized_size;

    return out;
}

void Mempool::reconcile(
    const Chainstate& chain)
{
    const auto height = next_height(chain);
    if (!height) {
        clear();
        return;
    }

    UtxoSet view = chain.utxos();
    std::vector<MempoolEntry> surviving;
    surviving.reserve(entries_.size());

    std::size_t surviving_bytes{0U};

    for (auto& entry : entries_) {
        const auto applied =
            view.apply_transaction(
                entry.transaction,
                *height
            );

        if (!applied.ok()) {
            continue;
        }

        if (entry.serialized_size >
            kMaxMempoolBytes - surviving_bytes) {
            continue;
        }

        entry.fee = applied.fee;
        surviving_bytes += entry.serialized_size;
        surviving.push_back(std::move(entry));
    }

    entries_ = std::move(surviving);
    total_bytes_ = surviving_bytes;
}

bool Mempool::contains(
    const Hash256& txid) const noexcept
{
    return std::any_of(
        entries_.begin(),
        entries_.end(),
        [&](const MempoolEntry& entry) {
            return entry.txid == txid;
        }
    );
}

const Transaction* Mempool::transaction(
    const Hash256& txid) const noexcept
{
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const MempoolEntry& entry) {
            return entry.txid == txid;
        }
    );

    return it == entries_.end()
        ? nullptr
        : &it->transaction;
}

std::vector<Transaction> Mempool::transactions(
    std::size_t limit) const
{
    const std::size_t count =
        std::min(limit, entries_.size());

    std::vector<Transaction> out;
    out.reserve(count);

    for (std::size_t i = 0U; i < count; ++i) {
        out.push_back(entries_[i].transaction);
    }

    return out;
}

std::vector<Hash256> Mempool::transaction_ids(
    std::size_t limit) const
{
    const std::size_t count =
        std::min(limit, entries_.size());

    std::vector<Hash256> out;
    out.reserve(count);

    for (std::size_t i = 0U; i < count; ++i) {
        out.push_back(entries_[i].txid);
    }

    return out;
}

std::size_t Mempool::size() const noexcept
{
    return entries_.size();
}

std::size_t Mempool::total_bytes() const noexcept
{
    return total_bytes_;
}

void Mempool::clear() noexcept
{
    entries_.clear();
    total_bytes_ = 0U;
}

} // namespace quintum
