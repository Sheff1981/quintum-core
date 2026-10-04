# QUINTUM Development History — Complete Git Commit Ledger

Status: **historical provenance**  
Coverage: repository inception through technical state `c5e29dcf45375d227872dd1a21c9127b214b1c8d`  
Entries: **591**

This file preserves a one-line chronological ledger of every Git commit in the public QUINTUM repository through the source state covered by the exchange pack. The commit object itself remains the authoritative source for the full diff and metadata.

| # | UTC time | Commit | Change |
|---:|---|---|---|
| 1 | 2026-10-02T15:06:52Z | `fc0ec6001f51fc918904aad32bd080bfe6b9835a` | foundation: add README.md |
| 2 | 2026-10-02T15:06:54Z | `5ef423c9b6d8a5cf377d36671de77972692c22d2` | foundation: add docs/ru/START_HERE.md |
| 3 | 2026-10-02T15:06:57Z | `52ff7cb0b9dc8691b52d7d4a695602fc6cc22afe` | foundation: add docs/ARCHITECTURE.md |
| 4 | 2026-10-02T15:06:59Z | `802080a9c983c88328b312734b158b56bf436ccd` | foundation: add docs/CONSENSUS.md |
| 5 | 2026-10-02T15:07:01Z | `c6d7d7dca339eef69b9d822ee9bd4f75244a93af` | foundation: add docs/DECISIONS.md |
| 6 | 2026-10-02T15:07:03Z | `134c528d469af3cbbc42cb923874be7180cd111a` | foundation: add docs/ROADMAP.md |
| 7 | 2026-10-02T15:07:05Z | `ac0bec881a70c7f94d3b381608fe17f9dac525aa` | foundation: add CMakeLists.txt |
| 8 | 2026-10-02T15:07:08Z | `676d077e66fc97b2b9c2e56cba586cfbc6fdaf71` | foundation: add src/main.cpp |
| 9 | 2026-10-02T15:07:10Z | `f4a31dc797ea239dd4e5a66103bb18d2e1c8d0da` | foundation: add .gitignore |
| 10 | 2026-10-02T15:07:12Z | `2d6c855ca217795855942f46cd0c4fa5c2e4f8b8` | foundation: add LICENSE |
| 11 | 2026-10-02T15:07:56Z | `5844be919bf1d44a398b3cd44fb24ff4a6f19f82` | ci: add C++23 build on Linux and Windows |
| 12 | 2026-10-02T15:09:30Z | `e4cd6208655d40f85e6981ae865d1507ffe71e45` | core: add fixed-size foundation types |
| 13 | 2026-10-02T15:09:32Z | `2db081cb1a30a2635448287cc3d3e15fc4e89e33` | test: add foundation smoke test |
| 14 | 2026-10-02T15:09:34Z | `2514b5ac1226f698e2712dfb4ebfb7b75cf92377` | build: enable C++23 foundation tests |
| 15 | 2026-10-02T15:09:45Z | `5ad6fd50c0c48bdeb95319dadd1f98c954d68f8e` | ci: run tests on Linux and Windows |
| 16 | 2026-10-02T15:14:53Z | `43081697954734ca1917b5d7b189ca5bd4ecca08` | core: add deterministic serialization primitives |
| 17 | 2026-10-02T15:14:55Z | `305eb00835f0d645355670f81301caf7aa4c5ad3` | crypto: add SHA-256 interface |
| 18 | 2026-10-02T15:15:33Z | `bf373c51c4d673e1a7e96487bdb21142bf160406` | crypto: implement SHA-256 and double-SHA-256 |
| 19 | 2026-10-02T15:16:07Z | `fddc9b288e22aee0c073b14b248718b3c185b957` | test: cover serialization and SHA-256 vectors |
| 20 | 2026-10-02T15:16:10Z | `89f45ac79b0e33467e7a9e95a7a579419863ccae` | build: compile crypto core and extended tests |
| 21 | 2026-10-02T15:16:58Z | `abe5f832ed2cd31e9e02b1f66138b09d9ed92c52` | docs: specify serialization and hash primitives |
| 22 | 2026-10-02T15:17:00Z | `c3446a51d18ad93ddf4d80b96f110d18ae230984` | docs: start Russian step-by-step development log |
| 23 | 2026-10-02T15:17:56Z | `4a1bd772af3a173ea04a34a5fd4f13a420501a4a` | docs: mark protocol primitives verified |
| 24 | 2026-10-02T15:18:26Z | `88e613504c21432e26e78f6c31f25364ef85c850` | tx: define transaction primitives |
| 25 | 2026-10-02T15:18:28Z | `0989aa6573dddd57be67386ce1628880fd6006ee` | tx: implement deterministic transaction serialization and txid |
| 26 | 2026-10-02T15:19:02Z | `284f61dfc693311f1d613d1594a602edd4caedec` | test: add deterministic transaction vectors |
| 27 | 2026-10-02T15:19:05Z | `42ae914797dcdfb5086acaf3f8b99e9a3911aa51` | build: add transaction implementation and tests |
| 28 | 2026-10-02T15:20:09Z | `0e8bd99e5df07eab04e64f2a9a06a705e71ddd31` | docs: specify draft transaction format |
| 29 | 2026-10-02T15:20:12Z | `9bb81a4a342067a2d9b6aec5bcff9a62d389503c` | docs: record transaction milestone step by step |
| 30 | 2026-10-02T15:21:01Z | `13e893f6c84e668c19db596c74393ab31a13d336` | docs: mark transaction milestone verified |
| 31 | 2026-10-02T15:21:50Z | `f1f554cc0b2f22f8367a24e18a1cc2891a17ffa7` | chain: define UTXO set and undo model |
| 32 | 2026-10-02T15:21:52Z | `53e2c4a980ef36160f1e3e9226d1204655a1f2d2` | chain: implement atomic UTXO apply and undo |
| 33 | 2026-10-02T15:22:29Z | `b522e1d4116dcfab73fb6e9bd8b40dd0f04f263a` | test: cover UTXO spending double-spend and undo |
| 34 | 2026-10-02T15:22:31Z | `abf5f38b5af10660e485b5cc7b71c98894f5df01` | build: add UTXO implementation and tests |
| 35 | 2026-10-02T15:23:15Z | `ee8c6401dfb341f367df3792d7f8b74b963f3338` | docs: specify UTXO state and undo model |
| 36 | 2026-10-02T15:23:17Z | `46d5d273066914552598d4aec1d2ffa20db1c1f0` | docs: record UTXO milestone step by step |
| 37 | 2026-10-02T15:24:05Z | `1aae0bd7a98c12157f0d9cd02616973f5a539a3b` | docs: mark UTXO milestone verified |
| 38 | 2026-10-02T15:24:32Z | `c4ef74a0d9c95664ff28d9be81c43f625a9a807c` | block: define block header and Merkle interfaces |
| 39 | 2026-10-02T15:24:34Z | `d00d5dc4e35e7e7b11a4fb5621ef862324caebfe` | block: implement header hash and Merkle root |
| 40 | 2026-10-02T15:25:14Z | `b17bcc9885283e7770cbef889702d77fe0d4be08` | test: add block header and Merkle vectors |
| 41 | 2026-10-02T15:25:16Z | `01857cf6a7e0fd8e53e75e16ec1e58a6c88a34e0` | build: add block and Merkle tests |
| 42 | 2026-10-02T15:25:59Z | `ef62fb0a96a2eacfa59640deb6b5e26234a8e2f7` | docs: specify draft block and Merkle format |
| 43 | 2026-10-02T15:26:02Z | `df40eff9ad32555ccb169b15ecf1e16b4c9a169d` | docs: record block milestone step by step |
| 44 | 2026-10-02T15:26:22Z | `f4d544d0ab10c53386b43ffd20b938c9b7b3b003` | docs: mark block milestone verified |
| 45 | 2026-10-02T15:35:11Z | `7a52b57da0a493fd695a69a212b6987a841bb286` | consensus: define proof-of-work interfaces |
| 46 | 2026-10-02T15:35:13Z | `ba950aa173e6022baba66b0c29873d6215a0e53d` | consensus: implement compact target and real nonce mining loop |
| 47 | 2026-10-02T15:36:11Z | `51c0ba005c3c10278e75359e566c68c739825c6b` | consensus: require canonical compact PoW targets |
| 48 | 2026-10-02T15:36:49Z | `b0240dcea11d0a36f6ce338341435517c03c11c2` | test: add proof-of-work target and mining vectors |
| 49 | 2026-10-02T15:37:03Z | `3b6186c637a98e03d4b41a3e043694f538fab959` | build: add proof-of-work implementation and tests |
| 50 | 2026-10-02T15:37:31Z | `3a8969bd8b15fe615646d37f7234c45093cb4e82` | test: correct independently verified PoW hash vector |
| 51 | 2026-10-02T15:38:34Z | `72f604ae47c11cb9431e4081e333a72aa148bed5` | consensus: expose chain-work arithmetic |
| 52 | 2026-10-02T15:39:25Z | `4e9d7e47ea405985947e5c64ad63599b6b04fda5` | consensus: implement exact 256-bit chain-work calculation |
| 53 | 2026-10-02T15:39:45Z | `324d3729315e1e2816f2cfc46891176d86596e9b` | test: verify exact chain-work arithmetic |
| 54 | 2026-10-02T15:40:42Z | `8054eb7f3c27ac9e4b92a74be724772bbf13c5a5` | docs: specify proof-of-work and chain-work rules |
| 55 | 2026-10-02T15:40:45Z | `a24177f81304e33e64139f37c845f879cf5d46a5` | docs: record proof-of-work milestone step by step |
| 56 | 2026-10-02T15:41:02Z | `de5b75bbccec4c8823bb6502b4962de9b6d2cf3f` | docs: mark proof-of-work milestone verified |
| 57 | 2026-10-02T15:44:04Z | `139cc71188e2abedc9337ed4afa9b19f8cf0d46f` | chain: define atomic chainstate interface |
| 58 | 2026-10-02T15:44:06Z | `62a428942efe621a14df29a53e52c3ec234fb789` | chain: implement atomic block connect disconnect and cumulative work |
| 59 | 2026-10-02T15:44:40Z | `77e244f00829af5160e34e0e2603c3bf32469719` | chain: define height overflow failure |
| 60 | 2026-10-02T15:44:41Z | `c836df17fd6fee37623ee99263d33cc029c641d2` | chain: reject active-chain height overflow |
| 61 | 2026-10-02T15:45:22Z | `a6a873380fba6ec171c51656735c45b010689936` | test: cover atomic chainstate connect disconnect and rollback |
| 62 | 2026-10-02T15:45:25Z | `54fa8a40f45bc3a79d36ad1fe13f55e59f0fecce` | build: add chainstate implementation and tests |
| 63 | 2026-10-02T15:46:42Z | `3aa849c92f61ee3f6ed74ef012f7282cbf874437` | docs: specify atomic chainstate behavior |
| 64 | 2026-10-02T15:46:45Z | `32d687f8d86cb7c9c57234bff4a57cc83af445ea` | docs: record chainstate milestone step by step |
| 65 | 2026-10-02T15:47:12Z | `2c141e9bd2f109ecf1a7469b47ee5ef21070be34` | docs: mark chainstate milestone verified |
| 66 | 2026-10-02T15:52:22Z | `a153e969babc54c38c353af59d262501b5f40925` | chain: add block index and reorg state model |
| 67 | 2026-10-02T15:53:17Z | `3c642cad08120be88a7999a417d35c11372f5587` | chain: implement block index fork tracking and staged reorg |
| 68 | 2026-10-02T15:53:50Z | `7219e977908a5e14493d196ddb6ad549398fad95` | chain: handle development genesis during branch activation |
| 69 | 2026-10-02T15:55:16Z | `6ffe82fbd07d5425e2c965b09d853d6887c58f95` | test: exercise heavier fork reorg and failed reorg rollback |
| 70 | 2026-10-02T15:56:12Z | `5fda3ebea754925d75debb1574b8d59dff7a178d` | core: remove integer-promotion warning from endian decode |
| 71 | 2026-10-02T15:58:18Z | `20475ba245ec8420e78352df61ed1d08672ca410` | docs: specify fork choice and staged reorganization |
| 72 | 2026-10-02T15:58:21Z | `92ad2c786602554e07162a4555004f65479a0704` | docs: record verified fork and reorg milestone |
| 73 | 2026-10-02T16:00:40Z | `20f16b57ca0d7152bb0149c08a985802bbdf5639` | consensus: define QUINTUM monetary parameters |
| 74 | 2026-10-02T16:00:43Z | `7dacf5a598e47e4418442cf6c0ab760e36aae50a` | consensus: implement subsidy halving and coinbase reward limits |
| 75 | 2026-10-02T16:01:03Z | `8b6906a2d77dee371370eaebbb2ec9907f091d4b` | consensus: add monetary and coinbase maturity UTXO failures |
| 76 | 2026-10-02T16:01:05Z | `4ac1f2cf1505e12dae0be9a8a6583c498206d480` | consensus: enforce money range and coinbase maturity in UTXO |
| 77 | 2026-10-02T16:01:45Z | `c95fdeb00e185332880724602900f751fa755061` | consensus: add invalid coinbase reward chain failure |
| 78 | 2026-10-02T16:01:48Z | `0c68f89c036d9fef93f9b0d40858b462e80e59cd` | consensus: enforce coinbase subsidy plus fee ceiling |
| 79 | 2026-10-02T16:01:50Z | `8ed012d05215262971bc9af397b1abb5554d1317` | build: compile monetary consensus rules |
| 80 | 2026-10-02T16:03:09Z | `eafdb31f6be1d03f886da3fc09596f2cbd96601f` | test: add monetary supply subsidy and reward vectors |
| 81 | 2026-10-02T16:03:11Z | `616025a86926687b04348ff0868d3f7a0d8dd592` | build: add monetary consensus tests |
| 82 | 2026-10-02T16:03:32Z | `95081e3db727cc7f8ca8c8ae99062e95a94cf6c9` | test: enforce coinbase maturity and money range in UTXO |
| 83 | 2026-10-02T16:05:27Z | `e36a6d25ca273e6189fc222ed575155ce0839894` | test: enforce mature coinbase rewards across chainstate and reorg |
| 84 | 2026-10-02T16:06:24Z | `50a4e7112fed112f14654be2252ffd56bf1222f3` | test: pin exact scheduled QUINTUM subsidy total |
| 85 | 2026-10-02T16:07:06Z | `5b7dfa5b0ccb24b1138b883c05035f66284402b1` | docs: define draft QUINTUM monetary policy |
| 86 | 2026-10-02T16:07:26Z | `5e55ffd161d1d7ca60d8d28c03a02fb68f9b2ff6` | docs: record implemented monetary consensus candidate |
| 87 | 2026-10-02T16:08:32Z | `e67eab922caf439d6fcf89498f40d19af33a47f9` | docs: update UTXO rules for maturity and money range |
| 88 | 2026-10-02T16:08:35Z | `5b3597c0026506bb7c4263d98b17a402008a2f46` | docs: add draft monetary policy architecture decision |
| 89 | 2026-10-02T16:09:37Z | `57c6ba0334f67738b18a5b59eed5c5c502a724fd` | docs: record monetary consensus milestone step by step |
| 90 | 2026-10-02T16:11:44Z | `ded1767a07ed8326da9f074cd206518624213c28` | test: keep consensus assertions enabled in Release CI |
| 91 | 2026-10-02T16:12:20Z | `57f179d3175a926b8ab066f0acf9c3d2aa9991ff` | docs: record Windows Release assertion QA fix |
| 92 | 2026-10-02T16:13:07Z | `1d1b98363c9ca7afc2658cb268202ae34370b036` | docs: mark monetary consensus milestone verified |
| 93 | 2026-10-02T16:22:43Z | `8766b0cbc446f31d8db6ebb07ed8bfb08452bd1d` | crypto: pin libsecp256k1 v0.8.0 by release SHA256 |
| 94 | 2026-10-02T16:23:08Z | `e172517efb1f550ce9603bf9eabaf16c093c6f1b` | crypto: define secp256k1 key and signature interface |
| 95 | 2026-10-02T16:23:11Z | `5e052a1e43c4aefa70895a3e32e2e885cce9fd70` | crypto: implement secp256k1 ECDSA keys signing and verification |
| 96 | 2026-10-02T16:23:49Z | `836d84c317fb7396745795569f3075afb9fafd5e` | consensus: define transaction authorization and sighash |
| 97 | 2026-10-02T16:23:52Z | `1f9724f571e3a1f99c1c16a9c9fb0e57b4a88abe` | consensus: implement domain-separated P2PK sighash and authorization |
| 98 | 2026-10-02T16:24:15Z | `9c2494a718f0659996d9444659c7732c109abc12` | consensus: add invalid input authorization UTXO failure |
| 99 | 2026-10-02T16:24:17Z | `6dab66b578e36af46f73637807d96440cea36bc0` | consensus: require valid secp256k1 authorization for UTXO spends |
| 100 | 2026-10-02T16:25:04Z | `5b5d8b5203907b46ffd9381700747e50a81399a6` | test: add secp256k1 key signature and tamper vectors |
| 101 | 2026-10-02T16:25:06Z | `2a6cf8ae0e59fdc0febcdc4f25510f49aa8fed0c` | test: add P2PK sighash ownership and tamper tests |
| 102 | 2026-10-02T16:25:16Z | `e424fa26ec5f8d14f8948a96cb5064d651a2cb90` | build: add secp256k1 and transaction authorization tests |
| 103 | 2026-10-02T16:25:55Z | `909bbc6a3d4f5ff84e55fa376314b300a5f71b4b` | test: require signed ownership in UTXO scenarios |
| 104 | 2026-10-02T16:26:26Z | `2831632ce4329e0acdbfd69e9f0d9c07d6d1ca44` | test: sign chainstate spends with secp256k1 ownership |
| 105 | 2026-10-02T16:27:40Z | `9a73b56a2e3282d4d03aff1aa51e15a9a60077d0` | test: complete signed spend helpers for chainstate |
| 106 | 2026-10-02T16:29:11Z | `2d25956f33d5f06f4a9c433bad91deb88cfb74da` | docs: specify cryptographic ownership and P2PK sighash |
| 107 | 2026-10-02T16:29:14Z | `cb6a6d91d8c16b15ff759fc953a74a868aa2431c` | docs: record initial cryptographic ownership decision |
| 108 | 2026-10-02T16:30:15Z | `f26d87ee5bd4470499a73e42486d634e123b9101` | docs: record cryptographic UTXO authorization |
| 109 | 2026-10-02T16:30:17Z | `4681d68f7e3be75866a087d44c338601c87bd097` | docs: record implemented transaction authorization candidate |
| 110 | 2026-10-02T16:30:19Z | `9707a497ca03736e9b4bdaabb4b711f96e760c9c` | docs: record verified cryptographic ownership milestone |
| 111 | 2026-10-02T16:34:10Z | `e79c0d43b2df18ee65f66b8046592b17baf1c1b0` | consensus: define mainnet testnet and regtest chain parameters |
| 112 | 2026-10-02T16:34:12Z | `f5f83ccf132db25ef69ed42c16fe0a811fb41fc1` | consensus: add draft QUINTUM network parameter sets |
| 113 | 2026-10-02T16:34:44Z | `847de101b08c21f469de7dac0aee66f3a16ab058` | consensus: define deterministic difficulty adjustment interface |
| 114 | 2026-10-02T16:34:46Z | `ca2b21569764cab1e567ddc8003b9d141d835b7e` | consensus: implement clamped Bitcoin-style difficulty retarget |
| 115 | 2026-10-02T16:35:08Z | `e77a4ec7a6f0d14f62ea93d098a2f623802840e3` | consensus: add PoW limit-aware validation |
| 116 | 2026-10-02T16:35:11Z | `641a9a94f80763d2ec07183c87055236f64de03f` | consensus: reject PoW targets above network limit |
| 117 | 2026-10-02T16:35:41Z | `1c63367447dd5f11794c26a77af7ed32889ad4be` | chain: bind chainstate to explicit network parameters |
| 118 | 2026-10-02T16:35:56Z | `eb48c83e73c21ddbe2144d720cbcf19eedf77247` | chain: pass network PoW limits through block application |
| 119 | 2026-10-02T16:36:40Z | `8ba2fbd4fbe9bb35de93ece956fc552a41dc274a` | consensus: enforce branch-specific expected difficulty in chainstate |
| 120 | 2026-10-02T16:36:57Z | `b0a09af358e3ec569e84a37ace7e09ea3ce4a4eb` | chain: own network parameters inside chainstate |
| 121 | 2026-10-02T16:37:00Z | `745bf98fd1c587a424690f5d9822c2dbadbe01ae` | chain: make ChainParams lifetime self-contained |
| 122 | 2026-10-02T16:37:51Z | `83aa5feeab08ee4b56e62165417e1f239937577d` | consensus: harden difficulty arithmetic overflow checks |
| 123 | 2026-10-02T16:37:59Z | `5fe232603a02d8f1fdb094609ce715d437a6f62c` | build: compile ChainParams and difficulty consensus |
| 124 | 2026-10-02T16:38:10Z | `d87aa7f067fe965132ee5c22e52da49bc6e213cb` | test: bind chainstate scenarios to regtest parameters |
| 125 | 2026-10-02T16:39:21Z | `f3871df1f1d04c59bb52817303d3434de6097ca6` | test: cover ChainParams retarget and contextual difficulty enforcement |
| 126 | 2026-10-02T16:39:38Z | `e369d8255346fe47ceedceb090ff6ea4dc93b4d9` | build: add ChainParams and difficulty tests |
| 127 | 2026-10-02T16:41:34Z | `dcbc3a4e3cce17713a423f9f5ab879485c54ba5b` | test: expose retarget integration failure code |
| 128 | 2026-10-02T16:42:40Z | `63ae72a90542bfdc272a77e368b6912b730a66d8` | test: derive small retarget vector from exact observed timespan |
| 129 | 2026-10-02T16:43:08Z | `c87d046d59e076e448786b9fe36f4418ace36bd0` | test: correct 15-of-40-second retarget expectation |
| 130 | 2026-10-02T16:43:37Z | `e541d8e5be36f2472283898592c65fff5e3236b3` | chain: expose next block work requirement |
| 131 | 2026-10-02T16:43:39Z | `71aa71f117b153acc1dccf93f948a4ea44b1edc9` | chain: calculate next active-tip difficulty for miners |
| 132 | 2026-10-02T16:43:54Z | `eea6661db28b5f84a9129e9ee4eb9f7683dda4fc` | test: verify miner-facing next work calculation |
| 133 | 2026-10-02T16:45:13Z | `af93d6b73241ff53eb006e78fcd3fc3d051b75ee` | docs: specify draft Mainnet Testnet and Regtest parameters |
| 134 | 2026-10-02T16:45:15Z | `e1f216bb4f671231ba0a238dd3430fc43e37bc92` | docs: specify QUINTUM difficulty adjustment rules |
| 135 | 2026-10-02T16:45:55Z | `947d1dfcde8ff923f0c0bd8205593ccf082c7c0d` | docs: record implemented ChainParams and difficulty candidate |
| 136 | 2026-10-02T16:45:58Z | `36bc41ec2bf0ee398b0952dd1ee4cc4ca8af042a` | docs: add network difficulty architecture decision |
| 137 | 2026-10-02T16:46:52Z | `eda0bef22971d7fd2f5e09200c09b3b5ed5da59c` | docs: record verified ChainParams and difficulty milestone |
| 138 | 2026-10-02T16:53:53Z | `49f8ec7e5a737ae32f021e520cc3a310f1330508` | consensus: add timestamp and block resource parameters |
| 139 | 2026-10-02T16:53:57Z | `2b9728d30f773f53b20f37c358a6ad7e4899d04e` | consensus: set 10-minute public block spacing and resource limits |
| 140 | 2026-10-02T16:54:29Z | `51533eaaee171fe53ee7790795a03bd3450480e1` | core: expose canonical CompactSize encoded length |
| 141 | 2026-10-02T16:54:47Z | `6278e08ade09155addd52a80e69c098889efc19e` | tx: expose exact serialized transaction size |
| 142 | 2026-10-02T16:54:50Z | `65c319a3e0f18fe03756691bb908ee5f4f3af3f1` | tx: calculate exact serialized size without allocation |
| 143 | 2026-10-02T16:55:06Z | `63971779536ed9ffb1346d63fdd33635b7da4ef8` | block: expose exact serialized block size |
| 144 | 2026-10-02T16:55:08Z | `e4bcab815808c462e7d8a3fdd37d3d13e98ebb23` | block: calculate exact serialized size without allocation |
| 145 | 2026-10-02T16:55:22Z | `08ed53567f0461335bec030d8f3378dbaecf6e83` | consensus: define block resource-limit validation |
| 146 | 2026-10-02T16:55:24Z | `e24e67083356e668156edc1a99502425b044ac00` | consensus: enforce block byte transaction and script limits |
| 147 | 2026-10-02T16:55:37Z | `99d1eb6e31a340cbe175c4bd3400252359a5b4be` | consensus: define timestamp validation helpers |
| 148 | 2026-10-02T16:55:40Z | `08444e937844b929ad886030a342f9f3628278d9` | consensus: implement Median-Time helper and future-time bound |
| 149 | 2026-10-02T16:55:59Z | `d67569d016584530fb660533567e52d7ca284f0d` | chain: add timestamp and resource-limit validation results |
| 150 | 2026-10-02T16:56:39Z | `dae93bb84fcee37b0077b0d9f7d7cfbb30474a0d` | consensus: enforce MTP future-time and block limits before indexing |
| 151 | 2026-10-02T16:56:48Z | `505c4b0f33837b2dcfc1a303b2c1a02c90917414` | build: compile timestamp and block-limit consensus |
| 152 | 2026-10-02T16:57:25Z | `c06595687c896f647b9f7f1490b0fa8fa166dfbd` | test: pin 10-minute Mainnet and Testnet block spacing |
| 153 | 2026-10-02T16:58:11Z | `f95ac39b21dc83360e9a56c5c4bd69f8ea6c42dc` | test: cover MTP future-time and block resource limits |
| 154 | 2026-10-02T16:58:20Z | `b5cd821f7ac82f23a33e4d80e0ed3b702be0be42` | build: add timestamp and block-limit tests |
| 155 | 2026-10-02T16:59:04Z | `a0b77360adf2441c761dd30741fa9b1081dc6697` | docs: update public network spacing to 10 minutes |
| 156 | 2026-10-02T16:59:07Z | `743cdf9ea8070f6d0a32f8459109c4ea34aad801` | docs: align difficulty policy with 10-minute blocks |
| 157 | 2026-10-02T16:59:10Z | `f4010c5f9a91ee47b60e5f3ef629ef7913d377f7` | docs: record timestamp and block resource consensus candidate |
| 158 | 2026-10-02T16:59:12Z | `a98433332c5d47d256361a2e2a8487583bc1d108` | docs: record timestamp and resource-limit decision |
| 159 | 2026-10-02T17:00:02Z | `6ba86977fe6a7e99d245eb7907485d52a2985777` | docs: specify timestamp consensus and resource limits |
| 160 | 2026-10-02T17:00:38Z | `88d81c387c85f02d5a8381fa27f84296dfea55a5` | docs: record timestamp consensus and block-limit milestone |
| 161 | 2026-10-02T17:01:16Z | `1bac54a3f46c871ddc5cc2e8dffa8aa329e8bd03` | docs: mark timestamp and block-limit milestone verified |
| 162 | 2026-10-02T17:30:05Z | `92b3efac423ee2696d947579abb81fe0c2ec7155` | consensus: reserve permanent unspendable locking-script version |
| 163 | 2026-10-02T17:30:08Z | `654dc634486a4d4023f88ba1aebe6ea16f592540` | consensus: make reserved lock version permanently unspendable |
| 164 | 2026-10-02T17:30:26Z | `72bc6da3e7f8ba0f7a64ece998bcd6a5ef3c1a83` | consensus: add immutable genesis parameter container |
| 165 | 2026-10-02T17:30:51Z | `bb4f10fd2036d6363e2059b6973d152b23fc5b2c` | consensus: pin mined QUINTUM genesis constants |
| 166 | 2026-10-02T17:31:17Z | `d47e4d199fba75a61da16c99d81779acf5305ebc` | consensus: define deterministic genesis construction |
| 167 | 2026-10-02T17:31:20Z | `5552858faf671e8fe9707a54d3e8195f6378c909` | consensus: implement reproducible QUINTUM genesis blocks |
| 168 | 2026-10-02T17:31:42Z | `8bc40e7f478d8255344e8d25ac6233385d630548` | consensus: include utility for genesis construction |
| 169 | 2026-10-02T17:31:54Z | `8bcc6ae0c2938b6e0b0829ce046fa4ac44fb0c13` | chain: add wrong-genesis consensus failure |
| 170 | 2026-10-02T17:31:58Z | `0b27f5ee14aa99da5b97238e423c0e2bcf9828cc` | consensus: require exact configured genesis as first block |
| 171 | 2026-10-02T17:32:05Z | `df8444eb55a128b4adf987fabbf29ffbc647dfad` | build: compile deterministic genesis construction |
| 172 | 2026-10-02T17:32:38Z | `e86929d35409b1355991d687a50c2e90bb70a6da` | test: keep synthetic chainstate scenarios outside enforced genesis |
| 173 | 2026-10-02T17:32:41Z | `28921489cf819840cd9a6e7d54f409de3e54f9c9` | test: isolate synthetic timestamp chains from enforced genesis |
| 174 | 2026-10-02T17:33:20Z | `763c5d336c4cb047f4c0566df2f8151dd16f53bb` | test: pin and independently reconstruct all QUINTUM genesis blocks |
| 175 | 2026-10-02T17:33:34Z | `c2f4638522fefbd1490219dc777b888e65150a63` | build: add immutable genesis verification tests |
| 176 | 2026-10-02T17:34:39Z | `5689b8f2f59b147e9ec9c1008bf3ec5d1aa0d717` | docs: pin QUINTUM Mainnet Testnet and Regtest Genesis blocks |
| 177 | 2026-10-02T17:35:58Z | `000c7e1986c4e6971fbe12c425cb81c31a437587` | docs: record permanently unspendable Genesis subsidy |
| 178 | 2026-10-02T17:36:01Z | `2340af101281bcdf920cfeba4833903ea434fcd8` | docs: reserve unspendable script version for Genesis |
| 179 | 2026-10-02T17:36:19Z | `0360674e01b5dd3ff5db5632dbfe2f8dfaa28a77` | docs: record code-pinned QUINTUM Genesis identity |
| 180 | 2026-10-02T17:36:22Z | `e4e6e5ca62c52a47c3414c0b6b2965ece21738f2` | docs: link ChainParams to immutable Genesis identity |
| 181 | 2026-10-02T17:37:21Z | `55f2e7fed46d5936e7c70e87459d41755f563d6c` | docs: mark QUINTUM Genesis identity as pinned |
| 182 | 2026-10-02T17:37:25Z | `9de0a970dab465c9b56a7533c5c6f50f0761a8eb` | docs: record QUINTUM Genesis birth milestone |
| 183 | 2026-10-02T18:09:36Z | `300052f02fe88ce4b02681db7f4064c57ffc2269` | storage: add persistent chainstate API |
| 184 | 2026-10-02T18:09:59Z | `3a1deb78421052b5422c8275f379a413ee916480` | storage: expose chainstate internals to durable store |
| 185 | 2026-10-02T18:10:01Z | `c40dfa74373dc3232e21ecb2865f6c0c0cc6b764` | storage: record accepted block order |
| 186 | 2026-10-02T18:10:04Z | `faa9d2746ff1401430ea71cb9b10ce329485b2e1` | storage: permit verified UTXO snapshot access |
| 187 | 2026-10-02T18:13:44Z | `94f369c20708a2030f3ee1fa88031a2f92545f94` | storage: implement durable block log and chainstate snapshot |
| 188 | 2026-10-02T18:14:22Z | `e7ddc2e8b3f1b6b0a0fd685b71ad774505720aaa` | storage: tighten cross-platform file handling |
| 189 | 2026-10-02T18:15:26Z | `2410c04abe8388c32dfae6c03bc98bc8b70c50ce` | test: cover persistent chainstate restart and corruption |
| 190 | 2026-10-02T18:15:33Z | `7e637771380e0f9bb81e0906f924d6d537b8de33` | build: add persistent storage suite |
| 191 | 2026-10-02T18:18:00Z | `01a0cf5db28bed7cce4257deb5fd213c0bb6feda` | docs: document persistent blockchain storage |
| 192 | 2026-10-02T18:18:41Z | `99dbac20b22595f60deda5e972830abf29fcc2d5` | docs: record persistent storage milestone |
| 193 | 2026-10-02T18:30:13Z | `0cc8217d366c2a735b52e1d3d3ba8913c4990678` | stage14: add src/mining/block_template.hpp |
| 194 | 2026-10-02T18:30:16Z | `c090384baf0469cdbc967180466815ff3aeb8f54` | stage14: add src/mining/block_template.cpp |
| 195 | 2026-10-02T18:30:18Z | `05e7c9b88f9557d1f5aaa568b6bd8bfb974d5c85` | stage14: add src/node/node.hpp |
| 196 | 2026-10-02T18:30:20Z | `aa9ba3acb07ff699dc1a27ce258dc87b0a9337dd` | stage14: add src/node/node.cpp |
| 197 | 2026-10-02T18:30:23Z | `e843222bc1ad282aee20d43dfb135a1873598c31` | stage14: add tests/node_runtime_tests.cpp |
| 198 | 2026-10-02T18:31:04Z | `daf91b10d5e665d2ff1c7c49c18bbd756102998b` | stage14: expose public key validation |
| 199 | 2026-10-02T18:31:07Z | `73f099eb848a46f72ef44355423839c71521a150` | stage14: validate compressed miner public keys |
| 200 | 2026-10-02T18:31:09Z | `5e1fc499a8e485ef84ed9ac52fd61b57270b1aae` | build: add stage14 node runtime and mining tests |
| 201 | 2026-10-02T18:31:48Z | `7c22a5f1c3876c8089fa7689715ee85d258ee44c` | stage14: turn quintumd into persistent development node |
| 202 | 2026-10-02T18:32:54Z | `bac37b4a78eb5ba9ce821bc57ec9871f7a3048ad` | stage14: expose safe active tip timestamp |
| 203 | 2026-10-02T18:32:56Z | `a260eb3efaf34cbc12cc1b6764310be1a051d02f` | stage14: define template timestamp failures |
| 204 | 2026-10-02T18:32:59Z | `ea1507c006bbd82cedcf1a887f60e2e7b9acf5fa` | stage14: build monotonic valid block timestamps |
| 205 | 2026-10-02T18:33:02Z | `0f3544af7c57a33fb3c9c51ad1b661e67d7438b6` | stage14: include numeric limits in development node |
| 206 | 2026-10-02T18:34:49Z | `807863185c33c9f9f2e2479fee4cf281b612f962` | stage14: reject invalid miner public keys in templates |
| 207 | 2026-10-02T18:35:41Z | `a751d1df9a3ced82e589adb430af5b3e865d845d` | docs: document node runtime and mining integration |
| 208 | 2026-10-02T18:35:43Z | `0672bffaf28712c0d64271ea3144f2946932a138` | docs: update current QUINTUM milestone |
| 209 | 2026-10-02T18:35:46Z | `cf879848a20dbb07d9075a1f240c5cc7d5ae0114` | docs: record stage14 node runtime milestone |
| 210 | 2026-10-02T18:37:11Z | `659e363c3d31ba5b26c1c23b0bfd86dccfa0666b` | test: cover mature spend fees in mined block template |
| 211 | 2026-10-02T18:39:22Z | `20834bf48a164d3cd6d3a998c19cdc35e86b15c5` | docs: finalize stage14 QA status |
| 212 | 2026-10-02T19:02:48Z | `632519637e27bac6e8e0e2eb60036b82da5adf98` | stage15: add src/net/protocol.hpp |
| 213 | 2026-10-02T19:02:50Z | `4df80e7e00d7d00934730e55fd052b61f17f2543` | stage15: add src/net/protocol.cpp |
| 214 | 2026-10-02T19:04:45Z | `1f14b7adae0546d20dabca9274d63edabcc63b50` | stage15: add src/net/peer.hpp |
| 215 | 2026-10-02T19:04:47Z | `f2a8c62312f3465a0aea6f95da4e5a0fcbde0f5e` | stage15: add src/net/peer.cpp |
| 216 | 2026-10-02T19:05:24Z | `7cda740edb4bc8b29017b74bd8e2446aa45fd32e` | stage15: allow handshake layer to adopt connected sockets |
| 217 | 2026-10-02T19:06:05Z | `bb1aa20b4e5fa7730ddbf2924dadbecedeec17a4` | test: add stage15 P2P handshake and isolation suite |
| 218 | 2026-10-02T19:06:22Z | `680dff0292dc8328e275b1ce75282c465e483737` | build: add stage15 P2P networking suite |
| 219 | 2026-10-02T19:06:24Z | `aa27e6da4fe6a9f8856f00275e8bfa964ace34b8` | test: include move utility explicitly |
| 220 | 2026-10-02T19:07:19Z | `c16b1ea131d51efe699ff2b735d8f9c7fdaf3d36` | stage15: prevent peer disconnects from raising SIGPIPE |
| 221 | 2026-10-02T19:07:55Z | `43dee0faacd5b68b9cc1c25559a614fc83966ce1` | test: cover P2P disconnect reconnect and payload bounds |
| 222 | 2026-10-02T19:08:18Z | `a2d4f4596476950779342427620024386bcf71f6` | stage15: forward declare handshake result |
| 223 | 2026-10-02T19:10:47Z | `a9f67026c1efa238c2bae7aba6f516e2f151d2b8` | docs: document QUINTUM P2P foundation |
| 224 | 2026-10-02T19:10:50Z | `0c55174582d2034241dbccb6ec3ed4963241dbcc` | docs: record stage15 P2P milestone |
| 225 | 2026-10-02T19:17:38Z | `2c2f8fb7867d77ec4e35e8f69ee0c1f66fe0d721` | stage16: add src/net/address.hpp |
| 226 | 2026-10-02T19:17:41Z | `fae95eb18547157dd6da8c2dee8da132ecd95c0d` | stage16: add src/net/address.cpp |
| 227 | 2026-10-02T19:17:43Z | `f776e649ae43624030a0832e2c020f93347f858d` | stage16: add src/net/discovery.hpp |
| 228 | 2026-10-02T19:17:45Z | `09895c707e57bf3d3adabbedbe5bfeb3e2e6b16a` | stage16: add src/net/discovery.cpp |
| 229 | 2026-10-02T19:18:22Z | `212bf2a1d186f5ed7835329eb9982261cb44b19a` | stage16: extend peer sessions with addr discovery |
| 230 | 2026-10-02T19:18:25Z | `2455708cb06a43186bb4b8dbc87dc5e5770ad67e` | stage16: implement getaddr and addr exchange |
| 231 | 2026-10-02T19:18:46Z | `d2e9fce2d742205e1c512214496bb4edc845cdce` | stage16: include utility for peer store moves |
| 232 | 2026-10-02T19:18:49Z | `80717870582e7096dea3e291953e92470c63f7c3` | stage16: include utility for discovered sessions |
| 233 | 2026-10-02T19:19:02Z | `e05f4b3d104ac13393964322f5d0f0c351883c2d` | stage16: add seed bootstrap API |
| 234 | 2026-10-02T19:19:04Z | `fdb8a6e2a4d2ca35ddc5078571c470b6039ea53d` | stage16: implement hardcoded seed bootstrap |
| 235 | 2026-10-02T19:19:58Z | `892f32a4e13463cf31e02279c2197906215b49b8` | test: add stage16 peer discovery integration suite |
| 236 | 2026-10-02T19:20:08Z | `031151cd2888068c338f958889dcd122042974f3` | build: add stage16 discovery sources and tests |
| 237 | 2026-10-02T19:21:07Z | `18d8a92b80d2918996bbcd7f7347a05f179f3ba8` | stage16: preserve filesystem paths in peer store replacement |
| 238 | 2026-10-02T19:21:10Z | `67fdb08e12f474a90e4c9f89dde980cd3bc83061` | stage16: add automatic peer address learning API |
| 239 | 2026-10-02T19:21:13Z | `a6eb29e86a4fb4a05258b4af2266fb28f82be168` | stage16: persist addresses learned from peers |
| 240 | 2026-10-02T19:21:27Z | `97cba85c1b2149a53cf3a749e3082067f0a7d575` | stage16: use Unicode-safe peer store fopen on Windows |
| 241 | 2026-10-02T19:21:30Z | `fb36ff041610aff5a5b0a9e58232d07379e4b62d` | test: verify durable automatic addr learning and backoff |
| 242 | 2026-10-02T19:22:18Z | `87ef2b118eed21b89d669596502383b8a80fa353` | stage16: bound persistent peer address manager |
| 243 | 2026-10-02T19:22:21Z | `80e56009f253f570e58a7061e3349e36c3742031` | stage16: enforce addrman bounds and exact record sizing |
| 244 | 2026-10-02T19:22:48Z | `cb524b5b515a085ceb7ab81a0d1362d6a3a56792` | stage16: add discovery initialization and multi-peer retry |
| 245 | 2026-10-02T19:22:50Z | `958dea29d7faa3c16031c419b3f91d1cf242919f` | stage16: initialize addrman safely and retry outbound candidates |
| 246 | 2026-10-02T19:23:22Z | `b2fb4cc84ef0647afd86a9c4b5ccf9f1298b17a9` | test: verify outbound discovery falls back after peer failure |
| 247 | 2026-10-02T19:23:32Z | `e1149331e6b2f2fb199f714c87f622c313685c35` | test: make failed-peer fallback port deterministic |
| 248 | 2026-10-02T19:24:46Z | `33f3a2532e186c17b15109e7b51f64d589ca9664` | docs: update milestone for stage16 peer discovery |
| 249 | 2026-10-02T19:24:49Z | `c135fb8a829c19060e63b828fa05174fd6ac5b2c` | docs: document stage16 addrman and peer discovery |
| 250 | 2026-10-02T19:24:52Z | `356d02debe555bbe53997bbf653cb313b85f781f` | docs: record stage16 discovery implementation |
| 251 | 2026-10-02T19:26:32Z | `4867224ed787e72ec3455b0c12b9d803c3b1ee06` | docs: finalize stage16 QA status |
| 252 | 2026-10-02T19:39:58Z | `b6f10d9f3264c6f514d08077eb4a9372ef625d95` | stage17: expose read-only active chain lookup for sync |
| 253 | 2026-10-02T19:40:01Z | `7b54ca6519d85a20c3f6157371f2842e79ac7ef2` | stage17: implement active chain and block lookup |
| 254 | 2026-10-02T19:40:04Z | `0e11fa1241d2b0fd19e4f5eb628a76d84289acfc` | stage17: add validated network block submission API |
| 255 | 2026-10-02T19:40:07Z | `6406232693f032ae97ab4a68f485c3b04b3af6a0` | stage17: route received blocks through persistent consensus |
| 256 | 2026-10-02T19:40:20Z | `0fd750b2816a8f655938eedb5eee0ac685d23338` | stage17: expose framed peer command transport |
| 257 | 2026-10-02T19:40:22Z | `ff6c5aa41abe4617c2b77e16646a345180827e65` | stage17: implement generic framed peer commands |
| 258 | 2026-10-02T19:41:48Z | `070572ca890abf916984c8aaf062532985179e3c` | stage17: add src/net/sync.hpp |
| 259 | 2026-10-02T19:42:01Z | `ddd879e5cc32ed95b28911ee93c535b80fa5de9f` | stage17: add src/net/sync.cpp |
| 260 | 2026-10-02T19:42:53Z | `242ad0050cca9e6adbb785fc200a780d8e47f98b` | test: add stage17 headers-first block sync integration suite |
| 261 | 2026-10-02T19:43:12Z | `9ca872f459d26e8d58a06f08b9cbbd8adc7ebfbc` | build: add stage17 headers and block sync suite |
| 262 | 2026-10-02T19:44:49Z | `5e3c740fa79f46324ce5d488a3312ba39dfc5c03` | stage17: represent stalled multi-batch synchronization |
| 263 | 2026-10-02T19:44:52Z | `59a31ae201874be17f33dbd6e00eb46c1c841bd4` | stage17: synchronize chains across multiple header batches |
| 264 | 2026-10-02T19:47:41Z | `9cd164fcd43be05b6a7c9912273d32dd63c48087` | docs: update milestone for stage17 blockchain sync |
| 265 | 2026-10-02T19:47:44Z | `d6c9a26bc5bcfc6ccdd3d073fb60808630e32bd2` | docs: document headers-first block synchronization |
| 266 | 2026-10-02T19:47:47Z | `4faaaef1eeef6636f45cf2e0bbcfe396d17008a8` | docs: record stage17 synchronization implementation |
| 267 | 2026-10-02T19:50:13Z | `32ee1c192b58c5b29b2982e83bc7001a6a93150e` | docs: finalize stage17 QA status |
| 268 | 2026-10-02T20:00:03Z | `d5502b24a0ef970798c625f8ce33190dab4464e4` | stage18: add src/node/mempool.hpp |
| 269 | 2026-10-02T20:00:06Z | `e9a1dc08e0d5ad2d65603fe2f8f6794b5fe7f8d4` | stage18: add src/node/mempool.cpp |
| 270 | 2026-10-02T20:00:26Z | `325aea88ee3329a056b73f57c8497618e8b26f8c` | stage18: integrate mempool API into node runtime |
| 271 | 2026-10-02T20:00:28Z | `af39e3dad85955b98f1736d1cf8407ae5200d5c1` | stage18: accept mempool transactions and mine them |
| 272 | 2026-10-02T20:00:57Z | `7a03ef60ef98988d47afaea22abd8f5eb179c734` | stage18: add transaction inventory type |
| 273 | 2026-10-02T20:01:48Z | `4d3e695000dab6c11f8d293cbc8a594556fdc5a7` | stage18: add src/net/relay.hpp |
| 274 | 2026-10-02T20:01:50Z | `eac78bb88aaa2f528b3a034ffde77d2ea6dfc12e` | stage18: add src/net/relay.cpp |
| 275 | 2026-10-02T20:02:42Z | `f9ba9282ff0d983acd0fc8c0c452ff194028d75a` | stage18: allow empty inv for empty mempool announcements |
| 276 | 2026-10-02T20:02:44Z | `26c3b702f5064f9352a820ed0e9f888958bd193e` | stage18: add inventory broadcast API |
| 277 | 2026-10-02T20:02:48Z | `45fad5d67290130cfdc88ef1367b0bcea2c30574` | stage18: broadcast transaction and block inventory to peers |
| 278 | 2026-10-02T20:03:53Z | `78edd84305edd7236962d5129735912897a05af4` | test: add stage18 mempool and live relay integration suite |
| 279 | 2026-10-02T20:04:00Z | `3bff085cc00315d6fac810838a75317b30795bc5` | build: add stage18 mempool and relay suite |
| 280 | 2026-10-02T20:06:01Z | `c5dd6c3939d42e8bf5739a5b88dfcf15e6fa47d4` | stage18: bound mempool mining to actual block capacity |
| 281 | 2026-10-02T20:06:27Z | `97b9dc6233848d5b23adbc65d39169ebb228b81b` | stage18: expose mempool script policy rejection |
| 282 | 2026-10-02T20:06:30Z | `769ad6a1709f1915b4f065e3c1b79966319dc04a` | stage18: reject over-limit scripts before mempool admission |
| 283 | 2026-10-02T20:06:46Z | `e251b69c7fbdbf463779b3e7081d8a2a2a8a15c5` | test: reject oversized scripts from mempool |
| 284 | 2026-10-02T20:10:17Z | `4288c243fd3aaee60b08190701cf52370350ca00` | docs: update milestone for stage18 live relay |
| 285 | 2026-10-02T20:10:31Z | `3d7d1d67dc03fca223a6875290dae3c54c897c84` | docs: document mempool and live inventory relay |
| 286 | 2026-10-02T20:10:33Z | `0a32ac422ad8bfb05c100bf6e91765b85f283307` | docs: record stage18 relay implementation |
| 287 | 2026-10-02T20:11:36Z | `44df23ca1fb6ec66a9422a776e716643a1836a5a` | stage18: mine validated mempool transactions from CLI |
| 288 | 2026-10-02T20:14:18Z | `033ecdcff1763a4e40463fa3805b003fa0c07d6a` | docs: finalize stage18 QA status |
| 289 | 2026-10-02T20:31:55Z | `815170a0d6d394a46410ff80b584a93a74323a26` | stage19: expose sync message dispatcher |
| 290 | 2026-10-02T20:31:58Z | `0eab370e87cd4092fd6f2e820a743282b073d100` | stage19: refactor sync service around pre-read messages |
| 291 | 2026-10-02T20:32:01Z | `8ca8ce669c8d80671f196f55695039c4127a0207` | stage19: expose relay request dispatcher |
| 292 | 2026-10-02T20:32:04Z | `3cd7ae466826310275927f52951b02a79e34608d` | stage19: refactor relay service around pre-read messages |
| 293 | 2026-10-02T20:32:13Z | `dc05efa043d8c0a41d32b288d4a1db8ba160acfd` | stage19: expose nonblocking peer readiness check |
| 294 | 2026-10-02T20:32:16Z | `9955b36e62f808bf6e7e694380ddeb725a8fbe01` | stage19: implement peer readiness polling |
| 295 | 2026-10-02T20:34:06Z | `7aae7b8dd4f66ad9cd5fe8b532fc000fa566966a` | stage19: define long-running network runtime |
| 296 | 2026-10-02T20:35:14Z | `1940fd76ebd4b40cea46e37ff09781b104e7cce6` | stage19: track runtime peer-address statistics safely |
| 297 | 2026-10-02T20:35:26Z | `414f92c0579a88b37f7e06e838fad623fbec8eab` | stage19: configure local-address policy before addrman load |
| 298 | 2026-10-02T20:35:30Z | `e05935dd72fed7265d7c72f54d921f4309207007` | stage19: apply runtime local-peer policy to addrman |
| 299 | 2026-10-02T20:36:21Z | `c0a06cceae2d76f234a68e49f725d1030df1be37` | stage19: separate listener poll and handshake timeouts |
| 300 | 2026-10-02T20:36:25Z | `5e537cb3164bead858ca951fbdaa17d24df5fb10` | stage19: implement independent accept and I/O timeouts |
| 301 | 2026-10-02T20:37:32Z | `122bcb02cfb624930f10d3b9753cb97b4a428ca5` | stage19: implement continuous network runtime |
| 302 | 2026-10-02T20:37:48Z | `f804219000cf2905aec347c7d329e91845558fd4` | build: include continuous network runtime |
| 303 | 2026-10-02T20:39:30Z | `25f75747a63ba207534af8dfe154f942d0a47530` | test: add stage19 continuous network runtime integration |
| 304 | 2026-10-02T20:39:38Z | `434ca388464539b6deb3f6d9ee4430e0b5cecb11` | build: add stage19 network runtime integration suite |
| 305 | 2026-10-02T20:40:32Z | `eabdee23cee6ca0612a2a7ae83505f86663a3030` | stage19: run quintumd as continuous P2P node service |
| 306 | 2026-10-02T20:40:46Z | `aee945def5e3d0362ab9b53f6a9589a29dae11e6` | fix: remove duplicate main return after network runtime integration |
| 307 | 2026-10-02T20:41:37Z | `246adea264259255de5f173c8a3560e2af523f5f` | test: keep stage19 block timestamps within consensus future limit |
| 308 | 2026-10-02T20:44:10Z | `43674f87a16906005c9699cf5809a5254ef9e735` | docs: update milestone for stage19 continuous node runtime |
| 309 | 2026-10-02T20:44:13Z | `ac193b0b4651c0d3165fd231b68f0e8e734353f5` | docs: document stage19 continuous networking lifecycle |
| 310 | 2026-10-02T20:44:18Z | `06fef3fafd6b3a4d239181ff881dfc2766e839ac` | docs: record stage19 continuous network runtime |
| 311 | 2026-10-02T20:45:32Z | `ace9e467da8afe56fce1c6aedd3eb4dd9252bf74` | stage19: relay newly synchronized active tips |
| 312 | 2026-10-02T20:45:34Z | `8293f5786669402dfdc4df26e78164e845813c5b` | stage19: fail visibly on unexpected network worker exit |
| 313 | 2026-10-02T20:48:34Z | `0d49ac829d63b13cdb61dd5caf1ae5e62758fcef` | docs: finalize stage19 QA status |
| 314 | 2026-10-02T21:07:30Z | `3306a4ef78c8779a81847f2684bfbb54ba136e46` | stage20: define OS-backed secure randomness |
| 315 | 2026-10-02T21:07:33Z | `011b940d9efff6aae00ae9342052f18218d9d747` | stage20: implement OS CSPRNG and private-key generation |
| 316 | 2026-10-02T21:07:36Z | `e9e143e7410f717e964a6d6e91f6a57f5cc7cc3b` | stage20: define QUINTUM wallet address format |
| 317 | 2026-10-02T21:07:39Z | `98da2ff4127fffd70b1d3a51f06116708613158e` | stage20: implement network-specific Bech32m addresses |
| 318 | 2026-10-02T21:10:05Z | `a01d59ca938351e8dc08d907d09593ec62578c80` | stage20: define persistent wallet core |
| 319 | 2026-10-02T21:10:08Z | `e2078eea17cc47e2f1268df72f413c7ad236fc0f` | stage20: implement keys balances coin selection signing and backup |
| 320 | 2026-10-02T21:10:34Z | `751a23f08c74371ca979f25d50ab747fd59ede06` | stage20: make address codec includes explicit |
| 321 | 2026-10-02T21:10:37Z | `28cc4e925af896667fa3f7e9df9b6e622156183f` | stage20: zero sensitive wallet buffers after use |
| 322 | 2026-10-02T21:10:46Z | `7fa5b8af5987fe2b447fcfabf2d4003afb6c01f6` | build: include wallet core and OS secure randomness |
| 323 | 2026-10-02T21:11:46Z | `f0f5447109d544b5b802df36366608a5152dae96` | fix: include byte-vector serialization types in address codec |
| 324 | 2026-10-02T21:13:09Z | `ed8b90185321d62b48c856e749f320cdba892ba6` | test: add stage20 wallet core end-to-end suite |
| 325 | 2026-10-02T21:13:19Z | `80e37af5461113f2e7686502832e9b5f8e248570` | build: add stage20 wallet core suite |
| 326 | 2026-10-02T21:14:40Z | `50d6174a8487c25547986111d970166af009deb5` | stage20: integrate wallet API into network runtime |
| 327 | 2026-10-02T21:15:15Z | `04c00090f468aeb728fcead0988713807f17ed7c` | stage20: keep wallet synchronized with live node state |
| 328 | 2026-10-02T21:16:45Z | `0020b4af8fa463818145b8b3f0dc95df252449a3` | test: verify wallet bridge through live network runtime |
| 329 | 2026-10-02T21:17:34Z | `2c295c82e0de2ded4f0d559a7accfee66eac61e7` | stage20: expose wallet receive send backup and mining defaults in CLI |
| 330 | 2026-10-02T21:18:51Z | `1ea1d0f0c3357f7ae1865fca0821fdbd662afd07` | fix: take CLI wallet backup after key-changing operations |
| 331 | 2026-10-02T21:20:34Z | `51e09dc9e99e95690d1b8cabbac47a1566146e45` | stage20: add pre-generated wallet keypool safety model |
| 332 | 2026-10-02T21:21:26Z | `f16283b9366d9ff74170c414a1857f5160eec57e` | stage20: pre-generate receive and change keypools for backup safety |
| 333 | 2026-10-02T21:22:20Z | `1e9fdbdb4577bc55cdb339bad4e882ef21b9a0b6` | stage20: report recovered keypool metadata persistence failures |
| 334 | 2026-10-02T21:22:23Z | `11905a1a8594224ad02f8a966aa84254b4b057f7` | stage20: recover keypool usage from blockchain and verify keypairs |
| 335 | 2026-10-02T21:23:00Z | `34853660dbed3d85411cdee7ba93854de97d0a6f` | stage20: recover keypool usage from pending wallet outputs |
| 336 | 2026-10-02T21:23:25Z | `78cbc445bc57fb36c9c6de72efcecfbdf9521817` | test: prove old wallet backup recovers future keypool address |
| 337 | 2026-10-02T21:24:18Z | `1bbeca21d8399f8b1ad3874f4049003ba7cd9c39` | test: pin deterministic QUINTUM address vectors |
| 338 | 2026-10-02T21:25:31Z | `d3aaa87985f2fb2799999987645554431327ac51` | fix: avoid wallet save failure after successful atomic rename |
| 339 | 2026-10-02T21:25:45Z | `9b96ccb6694188554489bfff6f022892c2466bb2` | test: verify restrictive POSIX wallet file permissions |
| 340 | 2026-10-02T21:26:59Z | `261a87d73ba36cdb2686ff89389d7c2707ef17eb` | docs: document stage20 wallet core and security boundaries |
| 341 | 2026-10-02T21:28:44Z | `edb180306ed21deb27664f01ce920a85a1f75f23` | fix: harden wallet keypool size bounds |
| 342 | 2026-10-02T21:29:15Z | `be8657f49532c0e94e37d4082f02ba0497008a95` | docs: update current milestone to wallet core |
| 343 | 2026-10-02T21:29:18Z | `440ba4e55520cf7b3f967e0c3ee6de3f55d35abe` | docs: update authorization status for wallet key generation |
| 344 | 2026-10-02T21:29:22Z | `f11a7bef4da41206bef18c55d04451f597df4e33` | docs: record candidate wallet address prefixes |
| 345 | 2026-10-02T21:29:25Z | `8ea221594ccd9204061fe16177f95433010f48c7` | docs: reflect implemented candidate address encoding |
| 346 | 2026-10-02T21:29:41Z | `bf1a9a7c5ba16169ecb72df01263fc2480db3fc7` | docs: mark wallet core milestone implemented |
| 347 | 2026-10-02T21:29:44Z | `ce085534b92fe12d7a6362a6591d86c56f7c5a03` | docs: reflect implemented candidate address format |
| 348 | 2026-10-02T21:30:12Z | `92dbcc5bea5f4011a909554ddd15ed45e294cbae` | docs: record stage20 wallet core |
| 349 | 2026-10-02T21:31:23Z | `453503f7c3a9fcc684f80639ff246f1cefa32089` | security: erase malformed wallet file buffers on every exit |
| 350 | 2026-10-02T21:32:22Z | `0305b337661502e2605839d100c0fb1fc143ceb9` | stage20: surface wallet backup-required state in CLI |
| 351 | 2026-10-02T21:34:40Z | `0c5970762c3dadb3a8e94ea4b340fc820de9294a` | security: zero private keys when wallet records move or die |
| 352 | 2026-10-02T21:34:57Z | `9b7268b0cadd9eb253318462e542fe88cbf7a7af` | fix: construct secure wallet key records explicitly |
| 353 | 2026-10-02T21:35:47Z | `9f555f3c4951951d589cd6aa2438295959cfcaa5` | security: erase partial wallet reads on I/O failure |
| 354 | 2026-10-02T21:36:45Z | `127fade0eb9b7ce00e86e08cbbeb78bb92d9402c` | stage20: restore latest used receive address as current |
| 355 | 2026-10-02T21:36:48Z | `5c77c0e051eea815b14530fb602c29c0e0d649e0` | stage20: report latest wallet receive address |
| 356 | 2026-10-02T21:36:59Z | `887b5a19c36816bd2a3200298f379cbba238000b` | test: persist latest wallet receive address selection |
| 357 | 2026-10-02T21:38:02Z | `dcb4a3e0bccba8327964f2e2ce8f33120aa21997` | stage20: reject wallet transactions above consensus block size |
| 358 | 2026-10-02T21:40:03Z | `922f134d21f923f06fdb04a4904cf0a5c203bd3e` | fix: persist latest selected wallet key ordering |
| 359 | 2026-10-03T00:31:07Z | `7af8eccf343672ee534bfb735d12c216f98c7343` | docs: finalize stage20 wallet core QA status |
| 360 | 2026-10-03T01:25:07Z | `fdcd1120fe7bc85236d2ec4f6dae01d6b02fd01d` | stage21: add wallet encryption and HD crypto API |
| 361 | 2026-10-03T01:25:09Z | `c06972ad298c2c6c1cdb6408de52688426d9d9e9` | stage21: implement Argon2id AEAD and deterministic derivation |
| 362 | 2026-10-03T01:25:21Z | `30b0aa84dc59cee2d544c8a398579ce0d6e21642` | stage21: pin Monocypher 4.0.3 for wallet cryptography |
| 363 | 2026-10-03T01:25:37Z | `d7ef23502f9c086ace645874bf8728fbded71707` | stage21: extend wallet API for encrypted deterministic wallets |
| 364 | 2026-10-03T01:26:16Z | `01c12b7e59e59c548b198035bcd9600b0e3a6d6d` | stage21: prepare versioned encrypted wallet lifecycle |
| 365 | 2026-10-03T01:27:16Z | `cb4954b48fc0e6c78235a0cda42135a56b962ae0` | stage21: implement encrypted wallet.dat v2 with v1 compatibility |
| 366 | 2026-10-03T01:27:42Z | `2cc68d649235c5e022685bdfa879f4a801f05f60` | stage21: add v1 migration and deterministic seed recovery |
| 367 | 2026-10-03T01:28:02Z | `95f7470fc6fd1ed971137f17c71a25eb59ec2d9c` | stage21: never claim seed-only recovery for legacy or imported keys |
| 368 | 2026-10-03T01:28:27Z | `c70b05270aa4241a95945c8018767ecc262d08b7` | stage21: test encrypted wallet migration and deterministic recovery |
| 369 | 2026-10-03T01:30:17Z | `8076ec311851a549f11bef4fa226a4ed1ccf5929` | stage21: include byte vector definition in wallet crypto API |
| 370 | 2026-10-03T01:30:43Z | `ceb755c9042aad89c66bbc959821662d9a8fefa0` | stage21: pass wallet credentials through network runtime |
| 371 | 2026-10-03T01:30:45Z | `4a1d2ec6b8083c4da34e3ffc361278941af527ca` | stage21: unlock and migrate encrypted wallet through runtime |
| 372 | 2026-10-03T01:31:21Z | `2046e3b4bcd3ed99f88f5fd361be9d022f595ec1` | stage21: add secure passphrase-file CLI and wallet migration command |
| 373 | 2026-10-03T01:31:49Z | `9b3a2f2f1debd47c1e445ff98e6946f78ec70620` | stage21: test encrypted wallet through live network runtime |
| 374 | 2026-10-03T01:32:07Z | `8031e1d60133ac1d7dc29d0902f7b2b3df304b34` | stage21: include utility for passphrase ownership transfer |
| 375 | 2026-10-03T01:34:15Z | `6e44f9018f3552888f919fad751dc2bd1b46c7b8` | stage21: enable vetted SHA512 HMAC for BIP32 derivation |
| 376 | 2026-10-03T01:34:18Z | `2a1199eb521a8a604865b2ba95e2db1973ec00f1` | stage21: expose secp256k1 private tweak-add for BIP32 |
| 377 | 2026-10-03T01:34:20Z | `83d783fedcd72dc6281ddb17a0c4c1091312e62b` | stage21: implement BIP32 CKD private-key tweak |
| 378 | 2026-10-03T01:34:54Z | `326aa6e5f38ef7d05112633b2733431629ba8b81` | stage21: replace custom derivation with BIP32 CKDpriv |
| 379 | 2026-10-03T01:35:32Z | `0662e0108b9c82d51b394b6170c0880282a0c20d` | stage21: wipe BIP32 hardened derivation temporaries |
| 380 | 2026-10-03T01:35:46Z | `0d9ee398347778c6cdcc75183067da72f0283955` | stage21: pin independent QUINTUM BIP32 derivation vector |
| 381 | 2026-10-03T01:36:45Z | `89da47b640aa758a8e948eeb1290b174dbcfb123` | docs: document stage21 encrypted BIP32 wallet format |
| 382 | 2026-10-03T01:36:47Z | `1152a51c9e2d003e652cfbe5fab592d18154a6d2` | docs: advance milestone to wallet hardening |
| 383 | 2026-10-03T01:36:50Z | `beab3b3b76479f7d7775ef58968757350214d188` | docs: record stage21 wallet hardening roadmap progress |
| 384 | 2026-10-03T01:37:12Z | `00c793b97de559d2980895769e88d78b1ba42b70` | stage21: allow secure wipe of BIP32 child secret |
| 385 | 2026-10-03T01:40:48Z | `44c84d6b93b680d34b65e99aefddb99d70228cad` | docs: close stage21 wallet hardening |
| 386 | 2026-10-03T01:46:17Z | `6a3a034f21d9e178692a9dca1e901239d339c631` | test: define stage22 wallet history index and fee behavior |
| 387 | 2026-10-03T01:46:25Z | `a11f6c22ccf5e7bac29a0c8e33996e3bb6eb4b7e` | test: register stage22 wallet regression suite |
| 388 | 2026-10-03T01:48:09Z | `cc44b01b2cdd0d4395e9c029d05fce7584084961` | stage22: add wallet fee policy API |
| 389 | 2026-10-03T01:48:11Z | `38d1bcfb63ca3f567b7e9259b0ba8601bb5bce95` | stage22: implement size and mempool fee policy |
| 390 | 2026-10-03T01:48:23Z | `23e62a70d1cb0c9198a22dcd22004aa4a2dcf739` | stage22: expose immutable mempool fee samples |
| 391 | 2026-10-03T01:48:26Z | `a5b85e0881c04e82855fa0576363664ca4f32bff` | stage22: expose immutable mempool entries |
| 392 | 2026-10-03T01:48:28Z | `c3c9809e27ec08b50ae0234fd9ac7e938fdb3d52` | stage22: build wallet fee policy |
| 393 | 2026-10-03T01:49:31Z | `00092181b4f4874f6ca9a99dc93a763d43b5fb08` | stage22: define persistent wallet history and index API |
| 394 | 2026-10-03T01:50:04Z | `9788598bc87c9843fe36c1c6328e48b7fb69c9e7` | stage22: prepare bounded persistent wallet state storage |
| 395 | 2026-10-03T01:51:07Z | `7fcb47da6de9c084577a555a156afeb2479c601a` | stage22: add durable wallet index and confirmed history store |
| 396 | 2026-10-03T01:51:23Z | `1c16fd3145056f4fd8f67b75ad95f2c7e5dd2d68` | stage22: build persistent wallet index |
| 397 | 2026-10-03T01:51:27Z | `a721c0f8132e952b04b3f21bb9abe4719a681758` | stage22: load and invalidate wallet index at lifecycle boundaries |
| 398 | 2026-10-03T01:52:14Z | `4feca89cafec73a86cd310eb7e8294a61f2d971b` | stage22: replace full wallet rescans with persistent incremental index |
| 399 | 2026-10-03T01:54:07Z | `36c48171c79e6e26609519e742212b417fb91767` | test: require runtime wallet history and fee policy bridge |
| 400 | 2026-10-03T01:55:40Z | `4955bbd7498c1afba2cc462a08b8fb239c2a1085` | stage22: expose wallet history and fee rate from runtime |
| 401 | 2026-10-03T01:55:43Z | `a74529d92eacb6e039368f4041a6aee2a903e57c` | stage22: bridge history and fee estimator through runtime |
| 402 | 2026-10-03T01:56:52Z | `7d44d97feae96c32ece10d3fadfc7d1b56ea4262` | test: persist inactive wallet transaction history across restart |
| 403 | 2026-10-03T01:58:30Z | `7c7896023890aebe2a809994dcfc435f9e582343` | stage22: represent persisted inactive wallet transactions |
| 404 | 2026-10-03T01:59:03Z | `1f2981c6bc3a58680d9053c86c2a5e5e3d0b1773` | stage22: persist confirmed and inactive transaction history |
| 405 | 2026-10-03T01:59:23Z | `ad0c05c04beec145c34f7a435ea9d7bff1c2c793` | stage22: retain inactive history when mempool entries disappear |
| 406 | 2026-10-03T02:00:06Z | `f18b0a1505979f3c46e3cc3cf0fc29c121638ac0` | test: verify wallet index rebuild on heavier-chain reorg |
| 407 | 2026-10-03T02:02:03Z | `a21ad6192c41341dfea5265d8a9d52b5716350a9` | test: diagnose persisted wallet history sync failure |
| 408 | 2026-10-03T02:03:10Z | `f56d258d6e2f5292647b28576ce9fb038c6b742d` | stage22: calculate confirmations only for confirmed history |
| 409 | 2026-10-03T02:03:47Z | `e0d087cb8249c06468efc79bf694be61d756b4ab` | test: cover crash after historical key import before wallet rescan |
| 410 | 2026-10-03T02:05:34Z | `e41d4fe246b6db4a4d7eb240b6e9ff32cdb67b2d` | stage22: bind wallet index cache to complete public key set |
| 411 | 2026-10-03T02:07:01Z | `55909e46a1664c9a91b92edc67754110afd15681` | test: remove stage22 diagnostic output after root-cause fix |
| 412 | 2026-10-03T02:08:11Z | `e6dbaec354c3e7f53f8175af882e5cb4a276fcfa` | docs: document stage22 wallet history index and fee policy |
| 413 | 2026-10-03T02:08:26Z | `82bebcc034bddc29292b66f1eca39b7aaf007272` | docs: advance current milestone to stage22 wallet indexing |
| 414 | 2026-10-03T02:08:30Z | `58d3f67a25649d3fc275d9134e4535960abd23a3` | docs: record stage22 wallet indexing roadmap progress |
| 415 | 2026-10-03T02:51:58Z | `ac03a5fc0ada648468988e5f2efb79dc92405511` | test: require immediate rescan after private key import |
| 416 | 2026-10-03T02:53:53Z | `e77be5a4b3cd40ab44eff9906e852c28400330d1` | test: retain reorged wallet history as inactive |
| 417 | 2026-10-03T02:55:49Z | `716e6fd25387607f3867c504079af42cc053ad9f` | stage22: retain reorged confirmed history as inactive |
| 418 | 2026-10-03T02:59:51Z | `f507872dea2f0da08dae76db22f599b74e57fe06` | docs: clarify wallet index privacy and reorg history semantics |
| 419 | 2026-10-03T02:59:54Z | `4e7ae6dce738652eceeca29b132202666fe26b54` | docs: close stage22 wallet index and history |
| 420 | 2026-10-03T04:06:27Z | `e892d53ec485f8cda3dd933fbfe27e457b82d76e` | test: define stage23 24-word recovery mnemonic behavior |
| 421 | 2026-10-03T04:06:34Z | `b19c9a6ff6ee1510fd3dc0a9ea348a931406ec57` | test: register stage23 recovery suite |
| 422 | 2026-10-03T04:08:20Z | `0f1dbf69318e6c8587504260fa77f82e66d004ec` | stage23: add pinned BIP39 English word list |
| 423 | 2026-10-03T04:08:23Z | `fe428d3d35d081068deed8dc245cc6f195f67542` | stage23: define recovery mnemonic API |
| 424 | 2026-10-03T04:08:25Z | `25871e3018b2d338a59c4073ab054dbafb7a5eba` | stage23: implement 24-word recovery mnemonic codec |
| 425 | 2026-10-03T04:08:39Z | `3d2af87d4e7bb6c1f09976ed25917f1ab5ef354c` | stage23: use secure erase helper in mnemonic codec |
| 426 | 2026-10-03T04:08:41Z | `3b8b6faaa084db29e5c1304935a4427aad66ba8d` | build: compile recovery mnemonic codec |
| 427 | 2026-10-03T04:10:04Z | `c7803674761d9d0ca4d46925549d3378b020040c` | test: include secure erase helper in recovery suite |
| 428 | 2026-10-03T04:12:36Z | `167ba842629b0bdf59058392d9b6b75b8e981d69` | test: correct unknown mnemonic word vector length |
| 429 | 2026-10-03T04:17:39Z | `95e14855b97210fb501405d886a5fd3b6965ae9b` | test: define mnemonic wallet recovery and gap rescan |
| 430 | 2026-10-03T04:20:17Z | `47ea9d10abd272d7c0509ce0bf0b1fd55736e030` | stage23: define mnemonic recovery wallet API |
| 431 | 2026-10-03T04:20:20Z | `1b1eb35c6ecff97b212e37f6f420aa838d0d64f9` | stage23: implement gap-aware mnemonic recovery |
| 432 | 2026-10-03T04:24:34Z | `d583e18d940ab56433cbfe1b8290790f371c4182` | test: require atomic mnemonic recovery commit |
| 433 | 2026-10-03T04:27:20Z | `992642fb872851c41e373b4ea57349792c740001` | stage23: commit recovered wallet only after successful index sync |
| 434 | 2026-10-03T04:31:56Z | `dc38eb9807caa8c2e48d7292c4673bd656d2df1a` | test: require explicit runtime mnemonic bridge |
| 435 | 2026-10-03T04:34:37Z | `58a8f7924b69d935d61bddb4eb957365bc964975` | stage23: expose explicit runtime mnemonic bridge |
| 436 | 2026-10-03T04:34:39Z | `9ba7700f03a13dfb4dc445b49b7bf3673b65756f` | stage23: expose explicit runtime mnemonic bridge |
| 437 | 2026-10-03T04:38:50Z | `3c427654514498a5c0ef9d0dcb65c519db5c6056` | docs: document stage23 mnemonic recovery semantics |
| 438 | 2026-10-03T04:38:52Z | `053435bc019414862319cf39a75f6ce4d50137d7` | docs: advance milestone to stage23 recovery |
| 439 | 2026-10-03T05:02:49Z | `44df6d2e1176a87b6744eef43cf7a605920d7edc` | test: make unknown-word case unambiguous |
| 440 | 2026-10-03T05:06:08Z | `75da97cd062324904e9457a4d1c4dcc27c1f3801` | docs: close stage23 recovery milestone |
| 441 | 2026-10-03T06:26:15Z | `be934247ce1985a0280f70b000f22c2a786e2302` | test: define stage24 auto fee and relay policy |
| 442 | 2026-10-03T06:26:22Z | `c495482e7ca17767d9057d5ac2f8102b5bc140ea` | test: register stage24 fee policy suite |
| 443 | 2026-10-03T06:27:57Z | `0d20733eb0691347b3414df73dfc1beecf9b0bef` | stage24: define shared fee policy math |
| 444 | 2026-10-03T06:27:59Z | `96ef610dc822a1cf013893932b533e560361d961` | stage24: implement shared fee policy math |
| 445 | 2026-10-03T06:28:17Z | `ca10320f2f8fae897d4f22076bd352a20d3dd230` | build: compile shared fee policy |
| 446 | 2026-10-03T06:28:27Z | `2bd7ce5fdda82b2dc928a2399b0f7517d4e015fb` | stage24: share fee math with node policy |
| 447 | 2026-10-03T06:28:29Z | `bf3bcbf5abb270ebeb116ddba26d857252c2ab89` | stage24: floor fee recommendation at relay policy |
| 448 | 2026-10-03T06:28:44Z | `fd8a083508472718d545d74235ea385d056690a3` | stage24: define mempool relay fee policy |
| 449 | 2026-10-03T06:29:19Z | `a43e0e8478201567f4c28d598cdc552124b7eea5` | stage24: enforce minimum relay fee in mempool |
| 450 | 2026-10-03T06:30:06Z | `fd3cfc20fab7b46cdbe7990eef13ac9d057095ab` | stage24: expose P2PK transaction size estimate |
| 451 | 2026-10-03T06:30:08Z | `6b89bc91d0832b949dcb9e1c94bc9fb058c59f08` | stage24: estimate signed P2PK transaction size |
| 452 | 2026-10-03T06:30:37Z | `e8745ab6f7b9a3ee236594f8f4582f3786f1b319` | stage24: define wallet auto fee quote and send API |
| 453 | 2026-10-03T06:31:21Z | `0ce2e483bef6c4ce7e06f540b398775f8c9c59d3` | stage24: implement automatic wallet fee quote and creation |
| 454 | 2026-10-03T06:32:03Z | `2ec1910225700dd5ad767e50e636586c6cd54dd7` | fix: include Amount definition in fee policy |
| 455 | 2026-10-03T06:33:50Z | `7eb5e22db08b43bb6bcb6972a06c30026b12e150` | test: update explicit wallet fees for relay floor |
| 456 | 2026-10-03T06:34:18Z | `3359a3ae04b362575d4132cc9e1b8c4caafd63a2` | test: define runtime auto fee quote and send bridge |
| 457 | 2026-10-03T06:36:06Z | `d594083a0a7dcc7763437ed184fe366ddbfe3037` | stage24: expose runtime fee quote and auto send API |
| 458 | 2026-10-03T06:36:18Z | `8d64fce3e49013b22fb0d4b8abdb8c7d3fcd55f2` | stage24: implement runtime fee quote and auto send bridge |
| 459 | 2026-10-03T06:37:56Z | `72e7a4dab5a7e7e72585e18ff5e83eb2307f88fb` | stage24: make automatic fee the default send policy |
| 460 | 2026-10-03T06:40:06Z | `93f2d535b8c643d59c66cc167bf921fddfd10ada` | test: prove relay fee remains non-consensus policy |
| 461 | 2026-10-03T06:44:55Z | `fe1a747b7f8fc15729288abc2fccfdd6017ddad7` | docs: advance milestone to stage24 fee policy |
| 462 | 2026-10-03T06:44:57Z | `133562972e3ae109bc4de25e0b092d1fc38bacff` | docs: document stage24 automatic fee semantics |
| 463 | 2026-10-03T06:45:00Z | `99e0de811390223a24581af59e88b86271119c53` | docs: record stage23 and stage24 wallet milestones |
| 464 | 2026-10-03T06:45:02Z | `8696fff233e3602d41989f62fce7540bb505b7d4` | docs: close stage24 automatic fee milestone |
| 465 | 2026-10-03T07:18:10Z | `196252a446ac87be7980e6b4cc1bfe457c50d730` | test: define stage25 desktop wallet API |
| 466 | 2026-10-03T07:18:21Z | `72bd819ce247b109447e3ec22b345bc3c7119c83` | test: register stage25 desktop wallet suite |
| 467 | 2026-10-03T07:19:06Z | `7a239fda051254214c21aec01bca7217d9fed794` | stage25: define persistent wallet metadata API |
| 468 | 2026-10-03T07:20:10Z | `7a1dc94f21aeef034be6e99023618b2fb442801b` | stage25: implement durable wallet metadata store |
| 469 | 2026-10-03T07:20:20Z | `d58be8405ef00c637de2542cc52bf3308b089937` | build: compile stage25 wallet metadata |
| 470 | 2026-10-03T07:20:36Z | `3f5ee840458e597b308bee1baf62ba7753b73098` | stage25: load metadata with wallet startup |
| 471 | 2026-10-03T07:21:31Z | `0a027a6eebcde0968518f96a041991996ece0b76` | stage25: define desktop snapshot and guarded send preview API |
| 472 | 2026-10-03T07:22:27Z | `09b4327e36aa4ae34b4da9c192c1f8ff75dcaeba` | stage25: implement guarded preview-confirm and desktop snapshot |
| 473 | 2026-10-03T07:24:46Z | `7f66ec964f1c07f060ef4c4436fa5c05ebc8d162` | stage25: harden metadata rollback and address canonicalization |
| 474 | 2026-10-03T07:25:28Z | `2d69ca053a6c8ea1dffd8e5327f73fbf7e6a18bf` | test: cover canonical address-book identity |
| 475 | 2026-10-03T07:25:31Z | `4932749d6e42d46b55797024f2b9ddd3341044df` | stage25: resolve preview labels by canonical address |
| 476 | 2026-10-03T07:26:47Z | `98b61901e6d4c95c492b00ffe12ab7fc796cf1c8` | stage25: distinguish wrong-wallet metadata |
| 477 | 2026-10-03T07:26:51Z | `b9af681aa61ef1655ed3c1362ee29cab2831e85b` | stage25: bind metadata to wallet identity |
| 478 | 2026-10-03T07:27:15Z | `431b8db80c2c1e1907ee60019832cb2a19d493c5` | test: bind wallet metadata to owning wallet |
| 479 | 2026-10-03T07:28:34Z | `9356c3d4713ea9031af643e96f67c1e66f7c2139` | fix: use stable deterministic anchor for metadata identity |
| 480 | 2026-10-03T07:28:50Z | `b958827eb642f674cf437f2144f833592280e6ac` | test: keep metadata identity stable across key reservation |
| 481 | 2026-10-03T07:29:54Z | `9ae30c121ca2be65f0ea38a402e888022113453f` | stage25: retain stable metadata wallet anchor |
| 482 | 2026-10-03T07:29:57Z | `14d9e4bff2226cb5498a2f8bdba9e19a9f630381` | stage25: verify metadata anchor against owned keys |
| 483 | 2026-10-03T07:30:01Z | `03ff32ee371063f825421dab2edb8c7b7b6f2204` | stage25: reset metadata anchor on fresh recovery |
| 484 | 2026-10-03T07:33:56Z | `71fcdd615e7556438742947c977d84d5e7cabb58` | docs: advance milestone to stage25 desktop wallet API |
| 485 | 2026-10-03T07:34:25Z | `2ed634cdf38858edbf2795151b76074bfa84e591` | docs: document stage25 metadata and preview-confirm safety |
| 486 | 2026-10-03T07:34:55Z | `9869809a6a26ffa7530924c5847809b53d41ba87` | docs: record stage25 desktop API milestone |
| 487 | 2026-10-03T07:35:19Z | `37f1b525cdf6f072edd518d03737e986b7d58945` | docs: close stage25 desktop wallet API |
| 488 | 2026-10-03T08:02:09Z | `825fcd3859f0c3aa7249809c6e764cc0cf7b0086` | stage26: define Qt desktop main window |
| 489 | 2026-10-03T08:03:17Z | `6cf9ae730d4a8ba98e2b6022f7577f8948b81d09` | stage26: implement Qt wallet overview send receive transactions |
| 490 | 2026-10-03T08:03:55Z | `30e59b6aa1207f3fc697d35b1afeff042bc7b284` | stage26: add Qt desktop application entrypoint |
| 491 | 2026-10-03T08:04:16Z | `0f69506eb9f511848145ac6ac66f377402de07e1` | stage26: add optional Qt 6 desktop executable target |
| 492 | 2026-10-03T08:05:05Z | `4bc60f3727b6c474394a70c8e857388fe5ac79ed` | ci: build Qt 6 desktop wallet on Linux and Windows |
| 493 | 2026-10-03T08:06:42Z | `a0da0dcae9160a754b367c427cc23805123cc2a9` | ci: install Linux OpenGL development dependency for Qt |
| 494 | 2026-10-03T08:08:15Z | `5f2c2fac0987245e7c6d64af5d0dbc95864d1250` | fix: use valid Qt standard confirmation button |
| 495 | 2026-10-03T08:09:10Z | `496cb6f066540e2dbdeacc19a0e733bc3a44067f` | ci: smoke-test Qt desktop executable |
| 496 | 2026-10-03T08:10:57Z | `d40b5e71e3b7473d117fa727523cba1a0761fb36` | ci: fix per-platform Qt smoke steps |
| 497 | 2026-10-03T08:12:04Z | `cc6926315fb8c7f6a946eba635c0455285ec05a5` | stage26: add real headless desktop smoke mode |
| 498 | 2026-10-03T08:12:16Z | `87377442df4c00e9cef31ca6769805461fc08b98` | ci: smoke-test live Qt wallet runtime |
| 499 | 2026-10-03T08:18:27Z | `608aaa3c961594d50972bcbfbcdafc2fbde740d6` | docs: close stage26 Qt desktop wallet foundation |
| 500 | 2026-10-03T08:29:36Z | `0ce455988031b18b557afd0aa2009dff17a8ec78` | stage27: extend runtime for recovery sync and wallet mining |
| 501 | 2026-10-03T08:30:30Z | `8e727a9dfdd5f6bbf6f440c8e2feee9a154eff95` | stage27: implement runtime recovery sync status and wallet mining |
| 502 | 2026-10-03T08:30:58Z | `d8b11a650b98899be8713349ed6a391b9d6b3120` | fix: preserve outbound peer sync height |
| 503 | 2026-10-03T08:31:36Z | `76b62eb0bfc63f515972a914e00f4cd2d09b5368` | test: define stage27 recovery mining and sync runtime behavior |
| 504 | 2026-10-03T08:31:48Z | `b3637e10ce11e6d197eafbb3c1b762fdf7b5b310` | test: register stage27 desktop operations suite |
| 505 | 2026-10-03T08:32:16Z | `46502af14c63015880549783ea25165246c57716` | fix: derive peer height from accepted active chain |
| 506 | 2026-10-03T08:33:28Z | `b73dc7b94a513ff9261b3260ef047e3f686e2e2d` | stage27: add create or 24-word recovery startup flow |
| 507 | 2026-10-03T08:34:05Z | `7e64e92d9de4b2780bf714607eb72a258b953f11` | stage27: define operational desktop wallet pages |
| 508 | 2026-10-03T08:35:58Z | `1948ecb098df1d43e1d5cad15fee05883e99b639` | stage27: implement recovery address book mining settings and sync UI |
| 509 | 2026-10-03T08:37:54Z | `fb2ed2e93004df6e04e1832b0679e87ef8d11ad5` | test: verify peer-height desktop synchronization status |
| 510 | 2026-10-03T08:39:19Z | `5e335cbdaca973ed0aebd26ddd7443783226127d` | security: erase revealed recovery words from desktop memory |
| 511 | 2026-10-03T08:39:30Z | `8ae4560e3f5162a8c5e382ff636b3eb8730186a3` | fix: erase mutable recovery phrase without const cast |
| 512 | 2026-10-03T08:39:44Z | `f5285dd7db987e7d2e4e17f80ef27ada59715817` | security: erase desktop setup secrets after runtime handoff |
| 513 | 2026-10-03T08:40:10Z | `229423149a3cc08968aab9851e2df17e5de62dd3` | stage27: fix offline sync progress and mining refresh load |
| 514 | 2026-10-03T08:42:30Z | `4f07c3e31bdc2eecadf60c1bf32b7af448d14ff0` | stage27: atomically rebind metadata after seed recovery |
| 515 | 2026-10-03T08:42:46Z | `11d8a0b71ab916a8340ade4824167846825c3886` | test: recover safely over orphaned wallet metadata |
| 516 | 2026-10-03T08:44:12Z | `95219c2f517becc9af1ddf8ea854517807733a63` | docs: close stage27 operational desktop wallet |
| 517 | 2026-10-03T08:47:06Z | `d71c8184a2f61e0b2c06660d7b6dacbf5c3a3ac1` | stage27: expose wallet passphrase verification |
| 518 | 2026-10-03T08:47:21Z | `a5622f21d27c044ec78ee3f5baacbb5d7eb97b0b` | stage27: verify wallet password before seed reveal |
| 519 | 2026-10-03T08:47:31Z | `26b64b64d3c350ef995a8c7420db8c56dc099be2` | stage27: bridge wallet passphrase verification to desktop |
| 520 | 2026-10-03T08:47:40Z | `c33b817bbf9d75028ca474ff70caddadb2897f53` | stage27: implement desktop wallet password verification bridge |
| 521 | 2026-10-03T08:47:58Z | `60c36c6217af8d6461df55971b020a356835d105` | security: require wallet password before revealing seed phrase |
| 522 | 2026-10-03T08:48:09Z | `772f8f5cab1419b54a47787a6e484eb6abffa882` | test: verify seed reveal password gate |
| 523 | 2026-10-03T08:56:57Z | `3fc6d93a3b0e9fcaabf98ec5beee0745ff9ff29e` | docs: close stage27 operational desktop wallet |
| 524 | 2026-10-03T10:12:58Z | `e438d1f04eaeea374c8260cd8166a61881970f06` | stage28: encrypt wallet metadata with authenticated v2 format |
| 525 | 2026-10-03T10:13:33Z | `0999f3a856b8ea56a32a7b4dd69f6d64f0f9de5c` | test: define stage28 encrypted metadata hardening |
| 526 | 2026-10-03T10:13:43Z | `5fa9c4e4cec98b1108886aec6a6c8135ecb1bf32` | test: register stage28 release hardening suite |
| 527 | 2026-10-03T10:14:22Z | `459acd5018b6f0ea2320769aa836321c118381ef` | stage28: define cross-platform wallet file permissions |
| 528 | 2026-10-03T10:14:25Z | `f2e1b366123e3b5a75514e46bd7d69cc87bc4bfa` | stage28: enforce current-user wallet file ACLs |
| 529 | 2026-10-03T10:14:44Z | `51c65c97361c04f9e90eeff3dba382f88f1a14cb` | build: compile Windows wallet file security helper |
| 530 | 2026-10-03T10:14:47Z | `f23c758e7469d11f04e301c9e4f5880f740c43fd` | stage28: restrict wallet files before writing secrets |
| 531 | 2026-10-03T10:14:52Z | `6f69825175780644e086c44fc85752e4cc2debe2` | stage28: restrict metadata files before writing labels |
| 532 | 2026-10-03T10:16:28Z | `65afe50f66104d87b5d90b6cbb3004682fa88802` | stage28: define complete wallet backup bundle API |
| 533 | 2026-10-03T10:17:07Z | `af271baea972c66f62d1e327ab7c68b5eb2bff1e` | stage28: implement complete wallet backup and restore bundle |
| 534 | 2026-10-03T10:17:31Z | `d5082f8808d4a78e0a17ec49cd61ef242d3db86b` | test: verify full wallet backup bundle roundtrip and guards |
| 535 | 2026-10-03T10:18:13Z | `d076a3d2dc1fa5e36c7e93b3d1f6a9afd08970a8` | fix: use cross-platform file permission helper everywhere |
| 536 | 2026-10-03T10:18:51Z | `0e7abeadf17bc7f4a31b8179f5206d9c1cdb848d` | stage28: expose complete backup bundle runtime API |
| 537 | 2026-10-03T10:18:55Z | `321666cef26eef70098674ba787ef5cd106360fc` | stage28: implement backup bundle runtime bridge |
| 538 | 2026-10-03T10:19:54Z | `d8f9f1ab9a7eaca1c119c1e309bcfa40d2d61289` | stage28: add desktop restore from complete wallet backup |
| 539 | 2026-10-03T10:20:16Z | `061acb0ef03d20db1943c9a84e00a50f76dd6265` | stage28: rename desktop backup action for complete bundle |
| 540 | 2026-10-03T10:20:18Z | `9d6a4e1dbd3e1a4c84d702a8c99d9bfcf8cb430a` | stage28: use complete wallet backup bundle in Settings |
| 541 | 2026-10-03T10:21:47Z | `5acda07542b75dfc29dfcaf50b41299e76af81cc` | stage28: add safe-upgrade Windows installer definition |
| 542 | 2026-10-03T10:23:03Z | `f900f1937d2703ce3732bdfa70a7f36b65f20fa0` | ci: package and smoke-test Windows installer |
| 543 | 2026-10-03T10:24:17Z | `e2f17822efac5ba9183a694310c1abb56a42c2c7` | stage28: migrate metadata atomically when encrypting legacy wallet |
| 544 | 2026-10-03T10:24:31Z | `a453123950a99e2fcff178729780e75b6a1f3096` | test: require immediate metadata encryption during wallet migration |
| 545 | 2026-10-03T10:25:23Z | `c84d32f4000d1a8cbd05562dfaa5b58ac715e5b3` | test: verify Windows wallet files use current-user protected ACL |
| 546 | 2026-10-03T10:27:41Z | `f7be5694235604ca8e924def2246cbd2debedc78` | fix: use numeric Windows product version in installer |
| 547 | 2026-10-03T10:28:50Z | `5df861ee8002258213a2f7172ae8e4506e61f761` | test: add persistent desktop mode for installer update QA |
| 548 | 2026-10-03T10:29:02Z | `e5c2225be0d23bd464fb5cb1556382ea1e062caa` | ci: verify installer closes running wallet during update |
| 549 | 2026-10-03T10:29:34Z | `38290214e11b8921abb4d5e95759cabd2fe73cd1` | ci: publish SHA256 checksums with Windows package |
| 550 | 2026-10-03T10:36:03Z | `d82a3b49686c021076073b725013b5f2e0d0bda8` | docs: close stage28 release hardening and Windows distribution |
| 551 | 2026-10-03T17:26:02Z | `ae05cb46c7985a863ed58dba97959424aa046cda` | net: add first public QUINTUM testnet seed |
| 552 | 2026-10-03T17:26:05Z | `490e0b395202a26db8f709493c63bec26b67587a` | test: pin QUINTUM testnet seed endpoint |
| 553 | 2026-10-03T17:44:49Z | `3cbdd3d93c1c53cd362394b4ada611b8fd0e6beb` | qt: default desktop wallet to public testnet |
| 554 | 2026-10-03T17:44:52Z | `874967b257a260f215c89bc2666fc8fbf5bb6bba` | docs: document testnet as desktop default |
| 555 | 2026-10-03T18:11:03Z | `33abe5915344fd19adcb40a046a0f4ea2dc7c7e2` | docs: record live public testnet P2P verification |
| 556 | 2026-10-03T18:11:05Z | `f9d035eab2da8bc9bed2a01dd52d8881b41f989a` | docs: record live Windows testnet verification |
| 557 | 2026-10-03T18:11:08Z | `9e4c14d94320d05c395e0e7b5a02fe8017c35840` | docs: record live runtime persistence verification |
| 558 | 2026-10-03T18:11:45Z | `85f6850504cf19945cf9699f7b47aef5d56dbeba` | docs: record Stage 29 public testnet end-to-end verification |
| 559 | 2026-10-03T18:12:01Z | `ac36d42a5d96be1e172f28d8d6322b39e05b6a77` | docs: mark Stage 29 public testnet deployment verified |
| 560 | 2026-10-03T18:12:03Z | `27d1513d96ad0472d240ffa9eb1313865fdc6e0a` | docs: remove obsolete Stage 28 backup note |
| 561 | 2026-10-03T18:14:44Z | `88a3c36c19269395878cc68553970df779a646fa` | runtime: expose peer sync state in desktop snapshot |
| 562 | 2026-10-03T18:14:52Z | `9d710674dcc1da6e321bd31c4c891454583e6984` | test: cover desktop peer height sync status |
| 563 | 2026-10-03T18:39:08Z | `44557c15456fd2b88baa0f9ebecb3813f6b5f860` | qt: report exact wallet startup failure safely |
| 564 | 2026-10-03T18:42:35Z | `54f0dae720a43623f3fabd22a832847264f056ed` | net: allow safe desktop listener fallback |
| 565 | 2026-10-03T18:42:38Z | `3378c07e7d0ce838c7ec9e8514fca31ccfbda2f6` | net: retry default listener on ephemeral port |
| 566 | 2026-10-03T18:42:40Z | `bc6123987885f0db8c85a97fdae7f3d8366f47d0` | test: cover occupied desktop P2P port fallback |
| 567 | 2026-10-03T18:43:01Z | `7a74aabfdf2e9b650071005e73490c7416099302` | net: surface worker startup failure |
| 568 | 2026-10-03T18:43:04Z | `356f83919d55d6f5e0a73ed5626eec4a6ae97906` | net: fail cleanly if worker thread cannot start |
| 569 | 2026-10-03T18:43:39Z | `6b06dfca169f394df3874165f925805d429350b8` | qt: keep wallet visible on startup failures |
| 570 | 2026-10-03T18:48:49Z | `aa9d90a229da7cd2e1e8966f088296cd3d5f9091` | net: require exclusive Windows listener ownership |
| 571 | 2026-10-03T18:50:23Z | `97c49f294f9d32c74285fad2e541b5481225ee6a` | docs: record live automatic peer reconnect |
| 572 | 2026-10-03T18:50:26Z | `c2d16a7676c38770463dc432ce96113c34a8a657` | docs: record live reconnect recovery on Windows |
| 573 | 2026-10-03T19:01:00Z | `ead859062f639ed470fdf81bf71dac9d2a1baf1a` | qt: harden Windows Server startup and trace stages |
| 574 | 2026-10-03T19:36:25Z | `3b53602b64c1a9e397dbed8c59fb5a29e6b75848` | docs: record repeated live block relay to height 7 |
| 575 | 2026-10-03T19:43:40Z | `c3396b22d1f25fed788e63330b62381be40f30e6` | mining: support nonce range continuation |
| 576 | 2026-10-03T19:43:42Z | `0bc969451b3ebe16180352134d1b00899e8ea6c0` | mining: begin PoW from supplied nonce cursor |
| 577 | 2026-10-03T19:43:44Z | `3130685c3539d4db7c67ca575348d436dc0ff254` | mining: keep desktop nonce cursor |
| 578 | 2026-10-03T19:43:47Z | `1d1b4ce7756417a0694b79783f8236ee88c1cc37` | mining: stop repeating nonce ranges in desktop miner |
| 579 | 2026-10-03T19:44:03Z | `685e3ae517268a130d05d93b4c858f227b4fa9e1` | test: verify PoW nonce range continuation |
| 580 | 2026-10-03T20:09:09Z | `7aceaeaa65617c74d87e1f7940538c74e25ac650` | net: refresh peer height from echoed active-block inventory |
| 581 | 2026-10-03T20:09:13Z | `44505f2a0e84f7d7b56912d5540b31ca4070a436` | test: refresh peer height after local block relay |
| 582 | 2026-10-03T20:44:47Z | `ae69cf1a610506be9f5f743da6a318c73b296738` | qt: add QUINTUM brand and navigation assets |
| 583 | 2026-10-03T20:45:03Z | `9db57e908c64a672aef7ecb946dbdbb7f2dfd4b0` | qt: expand modern desktop wallet view model |
| 584 | 2026-10-03T20:45:36Z | `915339935e4ac61d9f9826f353ab9ffbb8a26322` | qt: add receive address labeling control |
| 585 | 2026-10-03T20:48:27Z | `d2469ed285bb043911a89db6f83121fd3af046ac` | qt: implement reference QUINTUM Core desktop interface |
| 586 | 2026-10-03T20:48:38Z | `41271636cea9c4a1b263f6fa049eff791a09e9e0` | build: bundle QUINTUM Qt branding resources |
| 587 | 2026-10-03T20:48:46Z | `fb5c53388e6f8f3b9cef8224f5d76d38e7322617` | qt: complete modern UI compile includes |
| 588 | 2026-10-03T21:10:41Z | `78c03539438c0c5b3cc4be3f53d3bbb9a8a079db` | net: make initial chain sync bidirectional |
| 589 | 2026-10-03T21:17:43Z | `946d8d7b3be37aee89feaae211da143285225ac1` | net: preserve equal-height fork discovery |
| 590 | 2026-10-04T05:36:34Z | `ee006ca8fa126d8aab9625a78d5fc20774023e6e` | docs: record overnight height 139 maturity and reconnect test |
| 591 | 2026-10-04T06:09:32Z | `c5e29dcf45375d227872dd1a21c9127b214b1c8d` | net: harden headers sync continuation |
