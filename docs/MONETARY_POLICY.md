# QUINTUM Monetary Policy

Status: **DECIDED FOR NEXT RANDOMX TESTNET — NOT YET IMPLEMENTED — NOT MAINNET FROZEN**

This document records the monetary policy agreed for the next incompatible RandomX test network. The current running SHA-256 Testnet still follows the old code until the consensus migration is implemented and tested.

## Atomic unit

- 1 QTM = 100,000,000 atomic units.
- Decimal places: 8.
- Smallest unit: 0.00000001 QTM.

## Genesis

- Spendable Genesis issuance: **0 QTM**.
- Genesis is an identity anchor only and gives no spendable balance to the founder, miner or project.

## Target block interval

- Target spacing: **120 seconds / 2 minutes**.
- Expected blocks per hour: **30**.
- Expected blocks per 24-hour day: **720**.

## Primary subsidy schedule

The first six subsidy eras contain exactly 1,000,000 mineable blocks each. Height 0 is Genesis and has no spendable subsidy.

| Heights | Total subsidy/block | Miner 95% | Founder 5% | Era issuance |
| --- | ---: | ---: | ---: | ---: |
| 1..1,000,000 | 50.00000000 QTM | 47.50000000 | 2.50000000 | 50,000,000.00000000 |
| 1,000,001..2,000,000 | 25.00000000 | 23.75000000 | 1.25000000 | 25,000,000.00000000 |
| 2,000,001..3,000,000 | 12.50000000 | 11.87500000 | 0.62500000 | 12,500,000.00000000 |
| 3,000,001..4,000,000 | 6.25000000 | 5.93750000 | 0.31250000 | 6,250,000.00000000 |
| 4,000,001..5,000,000 | 3.12500000 | 2.96875000 | 0.15625000 | 3,125,000.00000000 |
| 5,000,001..6,000,000 | 1.56250000 | 1.48437500 | 0.07812500 | 1,562,500.00000000 |

Exact primary issuance through height 6,000,000:

- total: **98,437,500.00000000 QTM**;
- miners: **93,515,625.00000000 QTM**;
- founder: **4,921,875.00000000 QTM**;
- founder share: **exactly 5.00000000% of primary subsidy issuance**.

At the 120-second target, 1,000,000 blocks represent 120,000,000 seconds, or about 3.80257 years. Six primary eras therefore represent about 22.8154 years at target spacing.

## Founder reward

For heights 1 through 6,000,000:

- exactly 5% of the scheduled block subsidy is paid to the publicly documented founder payout script;
- the founder reward is created only when a valid PoW block is mined;
- it is not a hidden premine and there is no separate mint function;
- the founder payout script/address must be pinned and published before RandomX Testnet activation;
- the founder reward is subject to normal coinbase maturity;
- transaction fees are not shared with the founder.

After height 6,000,000, the founder consensus reward is permanently **0 QTM**.

## Tail emission

Beginning at height **6,000,001**:

- block subsidy: **1.00000000 QTM per block forever**;
- founder share: **0 QTM**;
- the full tail subsidy goes to the miner.

At target spacing:

- 30 QTM/hour;
- 720 QTM/day;
- 262,800 QTM per 365-day year;
- 262,980 QTM per 365.25-day average calendar year.

Because tail emission is constant in QTM while circulating supply grows, percentage monetary inflation declines over time.

## Transaction fees

- **100% of transaction fees go to the miner of the block.**
- Founder reward is calculated only from scheduled subsidy, never from fees.

## Coinbase maturity

Coinbase outputs become spendable only after **500 blocks**.

At the 120-second target:

- 500 blocks × 120 seconds = 60,000 seconds;
- 60,000 seconds = **16 hours 40 minutes**.

The same maturity applies to miner subsidy and founder subsidy outputs because both are coinbase outputs.

## Consensus invariants

There is no:

- hidden premine;
- developer mint key;
- master balance override;
- RPC bypass for issuance;
- mechanism to increase the founder percentage after activation;
- mechanism to re-enable the founder reward after height 6,000,000 without a network-breaking consensus fork.

## Implementation status

The current code still implements the legacy pre-RandomX candidate (21M / 10-minute / SHA-256 schedule). This document is the authoritative target specification for the next RandomX Testnet implementation. Code, tests, GUI and exchange documentation must be updated together before activation.
