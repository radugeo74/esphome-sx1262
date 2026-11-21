#pragma once
// Pure, dependency-free time-window predicate for the opt-in XMQ observer.
// uint32_t subtraction is deliberately wrap-safe for windows < 2^31 ms.
// No radio, sensor, AES, credential or heap ownership.
#include <cstdint>

namespace esphome {
namespace xmq_shadow_probe {

static constexpr uint32_t kShadowWindowMillis = 3600000UL;
static constexpr uint32_t kShadowWindowMaxObservations = 128;

inline bool shadow_window_elapsed(uint32_t now_ms, uint32_t started_ms) noexcept {
  return static_cast<uint32_t>(now_ms - started_ms) >= kShadowWindowMillis;
}

// Constant-space admission control shared by the real firmware and host test.
// Queries do not consume a slot: the probe asks before AND after legacy decode.
// Only an actual observation consumes one slot, preventing double counting.
class ShadowObservationBudget {
 public:
  bool renew_if_elapsed(uint32_t now_ms) noexcept {
    if (!shadow_window_elapsed(now_ms, window_started_ms_)) return false;
    window_started_ms_ = now_ms;
    used_ = 0;
    return true;
  }
  bool may_observe() const noexcept { return used_ < kShadowWindowMaxObservations; }
  bool consume() noexcept {
    if (!may_observe()) return false;
    ++used_;
    return true;
  }
  bool exhausted() const noexcept { return !may_observe(); }
  uint32_t used() const noexcept { return used_; }
  uint32_t window_start() const noexcept { return window_started_ms_; }

 private:
  uint32_t window_started_ms_{0};
  uint32_t used_{0};
};

}  // namespace xmq_shadow_probe
}  // namespace esphome
