# QUINTUM Protocol Primitives

Status: **DRAFT — pre-mainnet**

This document records the low-level byte rules used by QUINTUM.

## Integer serialization

Multi-byte unsigned integers are serialized in **little-endian** byte order.

Example:

`0x12345678` → `78 56 34 12`

These rules must remain deterministic on every supported CPU and operating system.

## CompactSize

Variable-length unsigned integer counts use canonical CompactSize encoding:

- 0..252: one byte
- 253..65535: marker `0xfd` + uint16 little-endian
- 65536..4294967295: marker `0xfe` + uint32 little-endian
- larger uint64 values: marker `0xff` + uint64 little-endian

Non-canonical encodings are rejected during decoding.

## Hash primitive

QUINTUM currently uses SHA-256 and double-SHA-256 as foundational deterministic hash functions.

`double_sha256(data) = SHA256(SHA256(data))`

The exact places where single or double SHA-256 are consensus-critical will be specified before genesis.

## Test vectors

SHA-256 of empty bytes:

`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

Double-SHA-256 of empty bytes:

`5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456`

SHA-256 of ASCII `abc`:

`ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`

Double-SHA-256 of ASCII `abc`:

`4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358`
