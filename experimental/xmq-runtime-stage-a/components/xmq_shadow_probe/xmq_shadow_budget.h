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

}  // namespace xmq_shadow_probe
}  // namespace esphome
