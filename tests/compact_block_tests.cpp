#include "net/compact_block.hpp"
#include "net/sync.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"

#include <array>
#include <cassert>
#include <span>
#include <cstdint>
#include <utility>

namespace {

quintum::Transaction coinbase()
{
    using namespace quintum;

    Transaction tx;
    tx.inputs.push_back(
        TxInput{
            .previous_output = OutPoint{},
            .unlocking_script = Bytes{0x01U},
        }
    );
    tx.outputs.push_back(
        TxOutput{
            .value = 50U,
            .locking_script = Bytes{0x00U},
        }
    );

    return tx;
}

quintum::Transaction regular_tx(
    quintum::Byte marker)
{
    using namespace quintum;

    Transaction tx;
    TxInput input;
    input.previous_output.txid[0] = marker;
    input.previous_output.index =
        static_cast<std::uint32_t>(
            marker
        );
    input.unlocking_script =
        Bytes{0x01U, marker};

    tx.inputs.push_back(
        std::move(input)
    );
    tx.outputs.push_back(
        TxOutput{
            .value =
                static_cast<Amount>(
                    1'000U + marker),
            .locking_script =
                Bytes{
                    0x01U,
                    marker,
                    0x51U,
                },
        }
    );
    tx.lock_time = marker;

    return tx;
}

quintum::Block sample_block(
    bool include_regular = true)
{
    using namespace quintum;

    Block block;
    block.header.version = 3U;
    block.header.previous_block[0] = 0x42U;
    block.header.timestamp = 1'800'000'000ULL;
    block.header.bits = 0x2100ffffU;
    block.header.nonce = 77U;
    block.transactions.push_back(
        coinbase()
    );

    if (include_regular) {
        block.transactions.push_back(
            regular_tx(7U)
        );
    }

    update_merkle_root(block);
    return block;
}

void test_compact_roundtrip_and_missing_fallback()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& limits =
        consensus::regtest_params().limits;
    const Block block =
        sample_block();
    constexpr std::uint64_t nonce{
        0x1122334455667788ULL
    };

    const Bytes compact_bytes =
        serialize_compact_block(
            block,
            nonce
        );

    assert(!compact_bytes.empty());
    assert(compact_bytes.size() <
           serialize_block_payload(block).size());

    const auto compact =
        parse_compact_block(
            compact_bytes,
            limits
        );

    assert(compact.has_value());
    assert(compact->nonce == nonce);
    assert(compact->short_ids.size() == 1U);
    // Deterministic BIP152-style vector for this fixed
    // header, nonce and transaction serialization.
    assert(compact->short_ids.front() ==
           0x7abd1ff4a05fULL);
    assert(compact->prefilled.size() == 1U);
    assert(compact->prefilled.front().index == 0U);
    assert(transaction_id(
               compact->prefilled.front().
                   transaction) ==
           transaction_id(
               block.transactions.front()));

    Mempool empty_pool;

    const auto partial =
        reconstruct_compact_block(
            *compact,
            empty_pool
        );

    assert(partial.ok());
    assert(!partial.complete());
    assert(partial.missing_indexes.size() == 1U);
    assert(partial.missing_indexes.front() == 1U);

    const BlockTransactionsRequest request{
        .block_hash =
            block_hash(block.header),
        .indexes =
            partial.missing_indexes,
    };

    const Bytes request_bytes =
        serialize_getblocktxn(request);

    const auto parsed_request =
        parse_getblocktxn(
            request_bytes,
            limits.max_block_transactions
        );

    assert(parsed_request.has_value());
    assert(parsed_request->block_hash ==
           request.block_hash);
    assert(parsed_request->indexes ==
           request.indexes);

    const BlockTransactions response{
        .block_hash =
            block_hash(block.header),
        .transactions = {
            {
                1U,
                block.transactions[1],
            },
        },
    };

    const Bytes response_bytes =
        serialize_blocktxn(response);

    const auto parsed_response =
        parse_blocktxn(
            response_bytes,
            limits
        );

    assert(parsed_response.has_value());

    const auto completed =
        complete_compact_block(
            *compact,
            empty_pool,
            *parsed_response
        );

    assert(completed.complete());
    assert(completed.block->transactions.size() ==
           block.transactions.size());
    assert(transaction_id(
               completed.block->transactions[1]) ==
           transaction_id(
               block.transactions[1]));
    assert(completed.block->header.merkle_root ==
           block.header.merkle_root);
}

void test_coinbase_only_block_reconstructs_without_request()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& limits =
        consensus::regtest_params().limits;
    const Block block =
        sample_block(false);

    const auto compact =
        parse_compact_block(
            serialize_compact_block(
                block,
                9U
            ),
            limits
        );

    assert(compact.has_value());
    assert(compact->short_ids.empty());

    Mempool empty_pool;
    const auto rebuilt =
        reconstruct_compact_block(
            *compact,
            empty_pool
        );

    assert(rebuilt.complete());
    assert(rebuilt.missing_indexes.empty());
    assert(block_hash(
               rebuilt.block->header) ==
           block_hash(block.header));
}

void test_tampered_missing_transaction_is_rejected()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& limits =
        consensus::regtest_params().limits;
    const Block block =
        sample_block();

    const auto compact =
        parse_compact_block(
            serialize_compact_block(
                block,
                123U
            ),
            limits
        );

    assert(compact.has_value());

    Mempool empty_pool;

    BlockTransactions response{
        .block_hash =
            block_hash(block.header),
        .transactions = {
            {
                1U,
                regular_tx(8U),
            },
        },
    };

    const auto completed =
        complete_compact_block(
            *compact,
            empty_pool,
            response
        );

    assert(!completed.complete());
    assert(completed.error ==
           CompactBlockError::
               response_mismatch);
}

void test_strict_request_index_validation()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& limits =
        consensus::regtest_params().limits;

    BlockTransactionsRequest valid;
    valid.block_hash[0] = 0x99U;
    valid.indexes = {1U, 3U, 7U};

    const auto parsed =
        parse_getblocktxn(
            serialize_getblocktxn(valid),
            limits.max_block_transactions
        );

    assert(parsed.has_value());
    assert(parsed->indexes == valid.indexes);

    Bytes duplicate;
    duplicate.insert(
        duplicate.end(),
        valid.block_hash.begin(),
        valid.block_hash.end()
    );
    append_compact_size(duplicate, 2U);
    append_compact_size(duplicate, 1U);
    append_compact_size(duplicate, 1U);

    assert(!parse_getblocktxn(
        duplicate,
        limits.max_block_transactions
    ).has_value());
}


void test_adversarial_compact_payload_corpus()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& limits =
        consensus::regtest_params().limits;
    const Block block = sample_block();
    const Bytes valid =
        serialize_compact_block(block, 0x50U);

    assert(parse_compact_block(valid, limits));

    // Every strict prefix of a valid compact block must fail closed.
    for (std::size_t size = 0U;
         size < valid.size();
         ++size) {
        assert(!parse_compact_block(
            std::span<const Byte>{
                valid.data(),
                size
            },
            limits
        ));
    }

    // Strict parsers must reject trailing garbage.
    for (const Byte marker :
         std::array<Byte, 4U>{
             0x00U, 0x01U, 0x80U, 0xffU}) {
        Bytes trailing = valid;
        trailing.push_back(marker);
        assert(!parse_compact_block(
            trailing,
            limits
        ));
    }

    // The short-id count begins after the 88-byte header
    // and 8-byte compact-block nonce. Encoding one as fd 01 00
    // is non-canonical and must be rejected before allocation.
    Bytes noncanonical{
        valid.begin(),
        valid.begin() + 96
    };
    noncanonical.push_back(0xfdU);
    noncanonical.push_back(0x01U);
    noncanonical.push_back(0x00U);
    noncanonical.insert(
        noncanonical.end(),
        valid.begin() + 97,
        valid.end()
    );
    assert(!parse_compact_block(
        noncanonical,
        limits
    ));

    BlockTransactionsRequest request;
    request.block_hash =
        block_hash(block.header);
    request.indexes = {1U, 3U};
    const Bytes request_bytes =
        serialize_getblocktxn(request);

    for (std::size_t size = 0U;
         size < request_bytes.size();
         ++size) {
        assert(!parse_getblocktxn(
            std::span<const Byte>{
                request_bytes.data(),
                size
            },
            limits.max_block_transactions
        ));
    }

    Bytes request_trailing = request_bytes;
    request_trailing.push_back(0x00U);
    assert(!parse_getblocktxn(
        request_trailing,
        limits.max_block_transactions
    ));

    BlockTransactions response{
        .block_hash = block_hash(block.header),
        .transactions = {
            {1U, block.transactions[1]},
        },
    };
    const Bytes response_bytes =
        serialize_blocktxn(response);

    for (std::size_t size = 0U;
         size < response_bytes.size();
         ++size) {
        assert(!parse_blocktxn(
            std::span<const Byte>{
                response_bytes.data(),
                size
            },
            limits
        ));
    }

    Bytes response_trailing = response_bytes;
    response_trailing.push_back(0xffU);
    assert(!parse_blocktxn(
        response_trailing,
        limits
    ));
}

} // namespace

int main()
{
    test_compact_roundtrip_and_missing_fallback();
    test_coinbase_only_block_reconstructs_without_request();
    test_tampered_missing_transaction_is_rejected();
    test_strict_request_index_validation();
    test_adversarial_compact_payload_corpus();
    return 0;
}
