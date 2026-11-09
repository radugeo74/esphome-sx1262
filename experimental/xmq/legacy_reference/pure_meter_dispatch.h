#pragma once
// Bounded ID-based routing for configured meters. Independent of wmbusmeters.
// A handler registered with an unsupported ID stays on the legacy fallback.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "pure_meter_match.h"

namespace wmbus_pure {
inline bool route_to_configured_meter(const Header &header, const char *id) {
  const auto match = match_meter_id(header, id);
  return match != MatchStatus::DifferentId;
}
inline bool configured_id_is_exact(const char *id) {
  Header dummy{};
  return match_meter_id(dummy, id) != MatchStatus::InvalidConfiguredId;
}
}  // namespace wmbus_pure
