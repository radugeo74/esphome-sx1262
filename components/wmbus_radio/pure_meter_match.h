#pragma once
// Independent BCD meter identity matching. No legacy meter driver dependencies.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "pure_frame_header.h"

namespace wmbus_pure {
enum class MatchStatus : uint8_t { Match, DifferentId, InvalidConfiguredId };
inline MatchStatus match_meter_id(const Header &header, const char *configured_id) {
  if (!configured_id || std::strlen(configured_id) != 8)
    return MatchStatus::InvalidConfiguredId;
  uint32_t expected = 0;
  for (size_t i = 0; i < 8; ++i) {
    const char c = configured_id[i];
    if (c < '0' || c > '9') return MatchStatus::InvalidConfiguredId;
    expected = expected * 10 + static_cast<uint32_t>(c - '0');
  }
  return expected == header.meter_id ? MatchStatus::Match : MatchStatus::DifferentId;
}
}  // namespace wmbus_pure
