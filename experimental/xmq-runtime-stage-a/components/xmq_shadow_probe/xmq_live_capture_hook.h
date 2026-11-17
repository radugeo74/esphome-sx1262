#pragma once
// Lab-only, read-only post-decode hook. This small POD API avoids linking
// the legacy meter source against XMQ parser internals.
// No radio callbacks, meter mutations, HA sensors or sensitive payload logs.
struct Telegram;
namespace esphome {
namespace xmq_shadow_probe {
struct LegacyNumeric {
  float value{0.0f};
  bool present{false};
};
struct LegacyReadings {
  LegacyNumeric target_m3;
  LegacyNumeric total_m3;
  LegacyNumeric current_hca;
  LegacyNumeric previous_hca;
  LegacyNumeric temp_room_c;
  LegacyNumeric temp_radiator_c;
};
bool wants_legacy_readings() noexcept;
void observe_legacy_telegram(const ::Telegram &telegram, bool decoded,
                             const char *driver,
                             const LegacyReadings &old_values) noexcept;
}  // namespace xmq_shadow_probe
}  // namespace esphome
