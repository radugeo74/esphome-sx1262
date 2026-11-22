#pragma once
#include <string>
// Lab-only, read-only post-decode hook. This small POD API avoids linking
// the legacy meter source against XMQ parser internals.
// No radio callbacks, meter mutations, HA sensors or sensitive payload logs.
struct Telegram;
namespace esphome {
namespace xmq_shadow_probe {
struct LegacyNumeric {
  float value{0.0f};
  bool present{false};
  bool changed{false};
  bool matched_dv{false};
};
struct LegacyReadings {
  LegacyNumeric target_m3;
  LegacyNumeric total_m3;
  LegacyNumeric current_hca;
  LegacyNumeric previous_hca;
  LegacyNumeric temp_room_c;
  LegacyNumeric temp_radiator_c;
  std::string current_date;
  std::string previous_date;
  std::string target_date;
  bool current_date_changed{false};
  bool previous_date_changed{false};
  bool current_date_matched_dv{false};
  bool previous_date_matched_dv{false};
  bool target_date_changed{false};
  bool target_date_matched_dv{false};
};
bool wants_legacy_readings(const void *source_meter) noexcept;
void observe_legacy_telegram(const void *source_meter,
                             const ::Telegram &telegram, bool decoded,
                             const char *driver,
                             const LegacyReadings &old_values) noexcept;
}  // namespace xmq_shadow_probe
}  // namespace esphome
