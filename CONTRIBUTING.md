# Contributing to QUINTUM Core

Thank you for helping test and improve QUINTUM.

QUINTUM Core is under active pre-alpha development. Contributions should prioritize correctness, reproducibility, reviewability, and network safety.

## Before contributing

For bugs, open an issue with clear reproduction steps when practical.

For substantial changes, discuss the problem first in an issue before investing in a large implementation.

Small fixes, tests, documentation improvements, and narrowly scoped corrections may be submitted directly as pull requests.

## Pull requests

A useful pull request should:

- have one clear purpose;
- explain the problem being solved;
- include tests for behavioral changes where practical;
- keep unrelated refactoring out of the same change;
- update documentation when externally visible behavior changes;
- avoid silently changing consensus or network behavior;
- build cleanly in the relevant CI jobs.

## Consensus-sensitive changes

Changes affecting any of the following require extra review:

- block or transaction validity;
- proof-of-work rules;
- subsidy or issuance;
- maturity rules;
- serialization or hashing;
- chain selection;
- P2P protocol behavior;
- genesis or network identifiers;
- wallet rules that could affect funds.

Such pull requests should include explicit rationale, deterministic regression coverage, and documentation of compatibility implications.

## Testing

External Windows testnet testing is documented in [docs/TESTNET_TESTING.md](docs/TESTNET_TESTING.md).

When reporting failures, include enough information for another person to reproduce the issue.

## Security and wallet safety

Never include private keys, recovery phrases, wallet passwords, or other secrets in issues, pull requests, logs, screenshots, or test fixtures.

If a report could expose users or funds to a serious security risk, do not publish exploit details unnecessarily in a public issue. Open a minimal issue asking for a private reporting channel if no dedicated security contact is yet documented.

## License and branding

Contributions to QUINTUM Core are made under the repository's [MIT License](LICENSE).

The software license does not grant rights to present a fork or derivative project as official QUINTUM. See [TRADEMARKS.md](TRADEMARKS.md).

## Development principle

QUINTUM is built in public. Changes should therefore be understandable from the code, tests, commit history, and documentation.
