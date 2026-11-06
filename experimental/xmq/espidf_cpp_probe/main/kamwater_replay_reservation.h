#pragma once
// ESP-IDF / HOST PROBE ONLY. Never included by the live SX126x receiver.
// Opt-in write-AHEAD reservation policy for authenticated CCM counters.
// A durable upper bound is committed BEFORE a sink sees measurements;
// restored state rejects EVERY counter <= that bound, including unseen
// counter values. This trades availability after reboot for fewer writes.
// No NVS setup, auto-provisioning, key reset, or HA publishing here.
#include <cstddef>
#include <cstdint>
#include <limits>
#include "kamwater_replay_checkpoint.h"

namespace xmq_portable { namespace kamwater { namespace reservation {
using Meter=checkpoint::Meter;
enum class Status : uint8_t {
  Ok, NotReady, InvalidWindow, Busy, DecodeRejected, DuplicateOrStale,
  StorageFailed, DeliveryFailed
};
constexpr uint8_t kMaxWindow=8; // bounded experimental choice, NOT a default
struct Session {
  replay_gate::State accepted{};
  Meter configured{};
  uint32_t epoch{0};
  uint32_t reserved_through{0};
  uint8_t window{0};
  bool ready{false};
  bool busy{false};
};
// Begin ONLY with a checkpoint already accepted by checkpoint::restore
// (or nvs_probe::load), never from a fabricated empty initial state.
// On restart the durable reservation becomes the in-RAM high watermark;
// this intentionally skips any unobserved reserved counter values.
inline Status begin(const checkpoint::Session &restored,
                    uint8_t window,Session &out) {
  out=Session{};
  if(window==0 || window>kMaxWindow) return Status::InvalidWindow;
  if(!restored.ready || !restored.state.initialized ||
     restored.epoch==0 || !replay_gate::same_meter(
         restored.configured,restored.state.identity))
    return Status::NotReady;
  out.accepted=restored.state;
  out.configured=restored.configured;
  out.epoch=restored.epoch;
  out.reserved_through=restored.state.highest_counter;
  out.window=window;
  out.ready=true;
  return Status::Ok;
}
inline uint32_t upper_bound(uint32_t counter,uint8_t window) {
  const uint64_t value=static_cast<uint64_t>(counter)+window-1u;
  const uint64_t maximum=std::numeric_limits<uint32_t>::max();
  return static_cast<uint32_t>(value<maximum?value:maximum);
}
// A bounded, single-threaded synchronous operation.
// The Store callback MUST atomically durably persist the new watermark
// (e.g. an explicitly initialized real NVS adapter), or return false.
// Any ambiguous write failure latches the session OFF. No sink is called
// until the durable reservation is confirmed; unlike the earlier
// store-after-delivery prototype, a crash cannot expose a delivered
// but unreserved counter provided the durable-store contract holds.
// On retry after a failed sink (without reboot), the existing reserved
// range may be reused. A power loss may skip up to window-1 counter
// values that have never been delivered. This is not HA exactly-once.
template<typename Store,typename Sink>
inline Status deliver_once(Session &session,const uint8_t *frame,size_t length,
                           const uint8_t key[16],Store &&store,Sink &&sink) {
  if(!session.ready) return Status::NotReady;
  if(session.busy) return Status::Busy;
  session.busy=true;
  replay_gate::Prepared pending{};
  const auto accepted=replay_gate::prepare(
      frame,length,session.configured,key,session.accepted,pending);
  if(accepted!=replay_gate::Status::Ok) {
    session.busy=false;
    return accepted==replay_gate::Status::DuplicateOrStale
        ? Status::DuplicateOrStale : Status::DecodeRejected;
  }
  if(pending.counter>session.reserved_through) {
    const uint32_t bound=upper_bound(pending.counter,session.window);
    checkpoint::State reservation_state{true,session.configured,bound};
    checkpoint::Bytes encoded{};
    if(!checkpoint::encode(reservation_state,session.epoch,encoded) ||
       !store(encoded.data(),encoded.size())) {
      replay_gate::discard(pending);
      session.busy=false;
      session.ready=false; // write may have been durable despite error
      return Status::StorageFailed;
    }
    session.reserved_through=bound;
  }
  // Callback is synchronous, and MUST be idempotent for retry.
  if(!sink(pending.plan)) {
    replay_gate::discard(pending);
    session.busy=false;
    return Status::DeliveryFailed;
  }
  session.accepted.highest_counter=pending.counter;
  replay_gate::discard(pending);
  session.busy=false;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::reservation
