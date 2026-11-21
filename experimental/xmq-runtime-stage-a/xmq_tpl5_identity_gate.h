#pragma once
// HOST-ONLY routing identity gate for direct TPL5 frame candidates.
// This is exact address/meter-type matching, NOT cryptographic identity
// authentication; mode-5 2F2F remains only a weak framing check.
// No stored meter secrets, legacy sensor state or HA publication.
#include "xmq_tpl5_direct_header.h"
#include "xmq_telegram_capture.h" // manufacturer_code_to_flag
#include <array>
#include <cstdint>
#include <string>

namespace xmq_runtime {

struct ExpectedTpl5Identity {
  std::array<std::uint8_t, 2> mfct_le{};
  std::array<std::uint8_t, 6> address_a{};
};

struct Tpl5IdentityCandidate {
  bool accepted{false};
  Mvt inner_mvt{};
};

// For CI=0x72 use the inner TPL identity; for CI=0x7A use the DLL
// identity. Do not silently fall back from inner to outer on mismatch.
// Both manufacturer and all six A-field bytes must match the subscriber.
inline Tpl5IdentityCandidate inspect_tpl5_routing_identity(
    const ParsedTpl5 &parsed, const ExpectedTpl5Identity &expected) noexcept {
  Tpl5IdentityCandidate candidate{};
  if (!parsed.valid || parsed.fields.mfct_le != expected.mfct_le ||
      parsed.fields.address_a != expected.address_a)
    return candidate;
  try {
    const unsigned code = static_cast<unsigned>(expected.mfct_le[0]) |
                          (static_cast<unsigned>(expected.mfct_le[1]) << 8);
    const auto manufacturer = manufacturer_code_to_flag(code);
    if (manufacturer.empty()) return candidate;
    candidate.inner_mvt = {
        manufacturer, expected.address_a[4], expected.address_a[5]};
    candidate.accepted = true;
  } catch (...) {
    candidate = Tpl5IdentityCandidate{};
  }
  return candidate;
}

}  // namespace xmq_runtime
