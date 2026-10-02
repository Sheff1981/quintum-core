# QUINTUM Monetary Policy

Status: **DRAFT — pre-mainnet**

These values are implemented and tested as the current consensus candidate. They are not **MAINNET FROZEN** until the genesis specification is deliberately finalized.

## Atomic unit

One QUINTUM is represented internally as:

`1 QUINTUM = 100,000,000 atomic units`

This gives **8 decimal places**.

The public name of the smallest unit is not chosen yet.

## Monetary range

Consensus arithmetic rejects values above:

`21,000,000 QUINTUM`

or:

`2,100,000,000,000,000 atomic units`

This is a hard money-range safety bound used for transaction/output validation.

## Initial block subsidy

At height 0:

`50 QUINTUM`

or:

`5,000,000,000 atomic units`

## Halving schedule

The subsidy halves every:

`210,000 blocks`

Examples:

- heights 0..209,999: 50 QUINTUM
- heights 210,000..419,999: 25 QUINTUM
- heights 420,000..629,999: 12.5 QUINTUM
- and so on

Subsidy arithmetic uses atomic integer units only. Fractions smaller than one atomic unit are discarded naturally by integer halving.

The scheduled subsidy reaches zero after 33 non-zero subsidy eras.

If every block claims the full allowed subsidy, the exact total scheduled subsidy is:

`20,999,999.9769 QUINTUM`

or:

`2,099,999,997,690,000 atomic units`

This is below the 21,000,000 QUINTUM money-range ceiling.

## Genesis subsidy and spendable maximum

The Mainnet Genesis coinbase claims the normal height-0 subsidy of **50 QUINTUM**, but sends it to the consensus-reserved unspendable locking-script version `0x00`.

Therefore:

- scheduled subsidy total remains **20,999,999.9769 QUINTUM**;
- Genesis subsidy is permanently unspendable;
- maximum theoretically spendable subsidy supply is **20,999,949.9769 QUINTUM**, before accounting for any later voluntarily burned or under-claimed rewards.

There is no private key, developer key or recovery mechanism for the Genesis output.

## Coinbase reward rule

A valid block may create at most:

`block subsidy + total transaction fees in that block`

The miner may claim less.

The miner may **not** claim even one atomic unit more.

The rule is checked only after all non-coinbase transactions have been evaluated so that the actual fee total is known.

## Coinbase maturity

A coinbase output cannot be spent until it has at least:

`100 blocks of maturity`

If a coinbase was created at height H, it becomes spendable at height:

`H + 100`

This prevents newly mined outputs from being spent immediately while the chain tip is still vulnerable to short reorganizations.

## Money-range checks

The current consensus validation rejects:

- any output above 21,000,000 QUINTUM;
- any transaction whose output sum exceeds the monetary range;
- any input accumulation that exceeds the monetary range;
- immature coinbase spends;
- block coinbase rewards above subsidy + fees.

## No privileged issuance

There is no:

- developer mint key;
- hidden premine path;
- administrator balance override;
- RPC that bypasses consensus issuance;
- special developer-only block reward.

All issuance must come through a valid coinbase transaction under the same consensus rules every node enforces.

## Parameters still not frozen

This document defines the current monetary candidate. Mainnet Genesis has now been constructed and code-pinned as a reproducible candidate.

Before **MAINNET FROZEN**, we still must deliberately confirm:

- final public-testnet validation;
- final review of network identifiers and ports;
- address format;
- smallest-unit public name;
- ticker.
