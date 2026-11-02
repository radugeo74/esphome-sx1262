#pragma once
// Pure registry gate: only verified DLL frames may be classified.
// Does not dispatch or publish; one place for subsequent XMQ registration.
#include <string>
#include <vector>
#include "native_verified_frame.h"
#include "pure_meter_match.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace registry {
enum class Kind : uint8_t { Invalid, Configured, Discovered };
inline Kind classify(const verified::Result &result, const std::vector<std::string> &ids) {
  if (result.status != verified::Status::Ok) return Kind::Invalid;
  for (const auto &id : ids) {
    if (wmbus_pure::match_meter_id(result.frame.header, id.c_str()) ==
        wmbus_pure::MatchStatus::Match) return Kind::Configured;
  }
  return Kind::Discovered;
}
}  // namespace registry
}  // namespace wmbus_native_sx126x
}  // namespace esphome
