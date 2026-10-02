# QUINTUM Proof of Work

Status: **DRAFT — pre-mainnet**

## Purpose

Proof of Work makes block creation objectively expensive and lets independent nodes compare competing chains by cumulative work.

## Compact target

The block header stores a 32-bit `bits` value containing:

- an 8-bit exponent;
- a 23-bit mantissa;
- a sign bit that is forbidden for valid PoW targets.

The decoder rejects:

- zero targets;
- negative targets;
- targets that overflow 256 bits;
- non-canonical encodings when validating or mining a header.

## Hash interpretation

The 32 bytes returned by double-SHA-256 are interpreted as one unsigned 256-bit integer in big-endian byte order for PoW comparison.

A header satisfies Proof of Work when:

`block_hash <= target`

## Mining loop

`mine_header()`:

1. decodes and validates the compact target;
2. hashes the current header;
3. checks the hash against the target;
4. increments the 64-bit nonce when the target is not met;
5. stops when a valid hash is found or the supplied attempt limit is exhausted.

This is real nonce search. There is no simulated success path.

## Chain work

Work contributed by one block target is calculated exactly as:

`floor(2^256 / (target + 1))`

The implementation uses deterministic 256-bit integer arithmetic without platform-specific wide integer extensions.

Cumulative chain work is formed by adding the work of each connected valid block.

Future chain selection will use greatest cumulative valid work, not merely the greatest block height.

## Test vectors

Compact value:

`0x1d00ffff`

decodes to:

`00000000ffff0000000000000000000000000000000000000000000000000000`

Its exact per-block work is:

`0000000000000000000000000000000000000000000000000000000100010001`

An intentionally easy development target `0x2100ffff` is used to verify the mining loop deterministically.

## Not frozen yet

Before QUINTUM genesis, the following still require explicit consensus decisions:

- mainnet PoW limit;
- initial mainnet difficulty;
- target block interval;
- difficulty adjustment algorithm;
- testnet/regtest special rules.
