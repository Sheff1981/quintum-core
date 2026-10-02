#pragma once

#include "consensus/chainparams.hpp"
#include "primitives/block.hpp"

namespace quintum::consensus {

[[nodiscard]] Transaction create_genesis_coinbase(
    const ChainParams& params
);

[[nodiscard]] Block create_genesis_block(
    const ChainParams& params
);

[[nodiscard]] bool verify_genesis(
    const ChainParams& params
);

} // namespace quintum::consensus
