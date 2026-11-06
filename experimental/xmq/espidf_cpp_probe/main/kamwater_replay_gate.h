#pragma once
// HOST/PROBE ONLY: one counter-state instance per configured meter.
// Monotonic CCM message-counter admission AFTER successful authenticated
// DLL decode AND complete in-memory sensor-map staging. Not hooked to
// native RF dispatch, Home Assistant, or persistent storage.
// A reboot with a fresh state does NOT provide cross-reboot replay security.
#include <cstddef>
#include <cstdint>
#include <utility>
#include "kamwater_sensor_mapping.h"

namespace xmq_portable { namespace kamwater { namespace replay_gate {
using Meter=oms_ccm_idf_probe::ingress::ConfiguredMeter;
struct State {
  bool initialized{false};
  Meter identity{};
  uint32_t highest_counter{0};
};
enum class Status : uint8_t {
  Ok, IdentityChanged, DecodeRejected, InvalidMapping, DuplicateOrStale
};
inline bool same_meter(const Meter &a, const Meter &b) {
  return a.manufacturer==b.manufacturer && a.id==b.id &&
         a.version==b.version && a.medium==b.medium;
}
inline Status stage_once(const uint8_t *frame, size_t length,
                         const Meter &configured, const uint8_t key[16],
                         State &state, sensor_mapping::Plan &out) {
  out=sensor_mapping::Plan{};
  // A shared state object must not accidentally admit another meter.
  if (state.initialized && !same_meter(state.identity,configured))
    return Status::IdentityChanged;
  configured_fields::Reading reading{};
  if (configured_fields::decode_format_b(frame,length,configured,key,reading)
      !=configured_fields::Status::Ok)
    return Status::DecodeRejected;
  if (state.initialized &&
      reading.authenticated_counter<=state.highest_counter)
    return Status::DuplicateOrStale;
  sensor_mapping::Plan staged{};
  if (sensor_mapping::stage(reading,staged)!=sensor_mapping::Status::Ok)
    return Status::InvalidMapping;
  // Commit ONLY after authenticating all bytes and staging every field.
  // No partial or stale measurements survive a rejected telegram.
  state.identity=configured;
  state.highest_counter=reading.authenticated_counter;
  state.initialized=true;
  out=std::move(staged);
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::replay_gate
