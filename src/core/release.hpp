#pragma once

namespace quintum::release {

// Pre-launch safety gate. Mainnet consensus vectors remain testable, but the
// public NetworkRuntime must not start a live mainnet P2P node until the
// audited launch release deliberately flips this constant.
inline constexpr bool kMainnetEnabled{false};

} // namespace quintum::release
