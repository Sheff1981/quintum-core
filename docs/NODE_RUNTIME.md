# Node Runtime and Mining Integration

Stage 14 turns the validated local blockchain into an executable persistent node runtime.

## Startup

`NodeRuntime` owns a `PersistentChainstate`.

On startup it:

1. opens and validates the existing chainstate database;
2. reconstructs the chain through normal consensus replay;
3. if no database exists, independently verifies the pinned network Genesis;
4. commits that exact Genesis as height 0;
5. exposes the restored active tip for further block production.

A corrupted or wrong-network database is never silently replaced with a new chain.

## Block template

`mining::create_block_template()` builds the next candidate block from the active tip.

It:

- requires an initialized chain;
- derives the next height;
- uses the active tip as `previous_block`;
- chooses a valid monotonic timestamp;
- derives required `bits` from the existing difficulty rules;
- validates candidate transactions against a temporary UTXO view;
- sums transaction fees with overflow checks;
- creates one height-committed coinbase;
- pays at most subsidy + fees;
- requires a valid compressed secp256k1 P2PK mining key;
- recomputes the Merkle root;
- enforces existing block resource limits.

No consensus constants are changed by the template builder.

## Mining

`NodeRuntime::mine_block()` performs real Proof of Work:

1. build a candidate block;
2. iterate the header nonce through `consensus::mine_header()`;
3. independently re-check the resulting header against the network PoW limit;
4. submit the full block through normal `Chainstate::connect_block()` validation;
5. durably commit the accepted block and updated chainstate.

The mining path does not bypass consensus validation.

## Development CLI

`quintumd` can now open a persistent development data directory and optionally mine blocks.

Supported development arguments:

- `--regtest`, `--testnet`, `--mainnet`;
- `--datadir PATH`;
- `--mine-blocks N`;
- `--miner-pubkey HEX`;
- `--max-attempts N`.

Mining requires a valid compressed secp256k1 public key. Private keys are never accepted by this CLI.

## Scope

Stage 14 deliberately does not introduce a wallet, mempool or P2P networking. Those layers must use the same consensus and persistent-node path rather than creating alternative block acceptance logic.


## Live persistence/relay verification — 2026-10-03

The persistent runtime was verified on the first public Testnet VPS.

A Windows peer mined two valid Testnet blocks and relayed them over the public P2P connection. The VPS accepted them through the normal runtime path. After the service was stopped and the same datadir was reopened, `quintumd` reported:

- network: `testnet`;
- height: `2`;
- the persisted active tip hash;
- clean shutdown at height 2.

This is an external end-to-end confirmation that remote P2P block acceptance reaches the same durable `PersistentChainstate` used after restart.
