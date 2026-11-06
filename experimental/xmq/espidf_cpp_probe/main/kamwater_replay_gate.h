#pragma once
// HOST/PROBE ONLY: replay admission for a SINGLE configured Kamwater meter.
// PREPARE -> synchronous delivery confirmation -> COMMIT. Preparing a sensor
// plan never marks a CCM counter consumed. No ESPHome live receiver uses this.
// This is not a persistence, concurrency, or exactly-once delivery system.
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
  Ok, IdentityChanged, DecodeRejected, InvalidMapping, DuplicateOrStale,
  NotPrepared, DeliveryFailed
};
struct Prepared {
  sensor_mapping::Plan plan{};
  Meter identity{};
  uint32_t counter{0};
  bool ready{false};
};
inline bool same_meter(const Meter &a, const Meter &b) {
  return a.manufacturer==b.manufacturer && a.id==b.id &&
         a.version==b.version && a.medium==b.medium;
}
// Authenticates the original short TPL and stages every field in memory.
// This function MUST NOT advance any replay state. Clear out on all errors.
inline Status prepare(const uint8_t *frame, size_t length,
                      const Meter &configured, const uint8_t key[16],
                      const State &state, Prepared &out) {
  out=Prepared{};
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
  out.plan=std::move(staged);
  out.identity=configured;
  out.counter=reading.authenticated_counter;
  out.ready=true;
  return Status::Ok;
}
inline void discard(Prepared &pending) { pending=Prepared{}; }
// Call ONLY when a synchronous downstream sink has confirmed completion.
// This does not prove HA delivery, make multiple entity writes atomic,
// or survive restart. Identity and counter are rechecked at commit time.
inline Status confirm_delivery(State &state, Prepared &pending) {
  if (!pending.ready) return Status::NotPrepared;
  const Meter identity=pending.identity;
  const uint32_t counter=pending.counter;
  discard(pending);
  if (state.initialized && !same_meter(state.identity,identity))
    return Status::IdentityChanged;
  if (state.initialized && counter<=state.highest_counter)
    return Status::DuplicateOrStale;
  state.identity=identity;
  state.highest_counter=counter;
  state.initialized=true;
  return Status::Ok;
}
// A synchronous simulated or real sink decides whether it completed.
// Rejected/duplicate frames never reach the sink; a failed sink does not
// consume the counter. This cannot roll back partial sink side effects.
template<typename Sink>
inline Status deliver_once(const uint8_t *frame, size_t length,
                           const Meter &configured, const uint8_t key[16],
                           State &state, Sink &&sink) {
  Prepared pending{};
  const Status result=prepare(frame,length,configured,key,state,pending);
  if (result!=Status::Ok) return result;
  if (!sink(pending.plan)) {
    discard(pending);
    return Status::DeliveryFailed;
  }
  return confirm_delivery(state,pending);
}
}}} // namespace xmq_portable::kamwater::replay_gate
