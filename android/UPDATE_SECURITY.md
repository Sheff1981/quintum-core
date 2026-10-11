# Android Update Security

QUINTUM Android updates must preserve application identity and user-owned state.

## Invariants

1. The production application ID is stable: `org.quintum.wallet`.
2. Production releases use the same protected signing identity. Release signing material is never committed to this repository.
3. Wallet material, metadata, node state and settings live in private application data and are not deleted by an ordinary package upgrade.
4. Schema changes require explicit forward migrations. Destructive migration is forbidden for wallet/key material.
5. Testnet direct-download builds may discover releases from a signed release manifest. The manifest is untrusted until its detached signature is verified against the pinned QUINTUM release public key.
6. The APK SHA-256 must match the verified manifest before installation is offered.
7. The candidate `versionCode` must be greater than the installed version.
8. Installation remains an explicit Android/user action. QUINTUM does not silently replace its own APK.
9. Store-distributed builds use the store's update path and package-signature checks.
10. Consensus-critical dependencies, RandomX, network identity and monetary rules are never hot-swapped independently of a complete reviewed application release.

## Failure behavior

A failed download, signature check, hash check, migration or node-start validation must leave the currently installed wallet data usable. Update code must never delete or overwrite the recovery seed as a rollback mechanism.

## Release pipeline requirement

Before enabling direct-download update discovery, CI/release automation must:

- build the release APK reproducibly where practical;
- compute SHA-256;
- create the release manifest;
- sign the manifest with an offline/protected release key;
- publish APK, manifest and signature together;
- verify installation over the previous supported version without data loss.

The first Android foundation intentionally defines this contract before implementing network update discovery.
