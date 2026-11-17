#include "xmq_shadow_probe.h"
#include "esphome/components/wmbus_common/wmbus.h"
#include "xmq_telegram_capture.h"
#include "xmq_driver_sources.h"
#include "esp_heap_caps.h"

namespace esphome {
namespace xmq_shadow_probe {
static const char *const TAG = "xmq_shadow_probe";
static ShadowProbe *active_probe = nullptr;

bool wants_legacy_readings() noexcept {
  return active_probe != nullptr && active_probe->wants_readings();
}

void observe_legacy_telegram(const ::Telegram &telegram, bool decoded,
                             const char *driver,
                             const LegacyReadings &old_values) noexcept {
  if (active_probe != nullptr)
    active_probe->observe(telegram, decoded, driver, old_values);
}

void ShadowProbe::observe(const ::Telegram &telegram, bool decoded,
                          const char *driver,
                          const LegacyReadings &old_values) noexcept {
  if (!wants_readings()) return;
  ++inspected_;
  const auto heap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
  if (heap < min_free_heap_) min_free_heap_ = heap;
  xmq_runtime::ShadowReference old;
  const std::string name = driver == nullptr ? "" : driver;
  auto put = [&old](const char *key, const LegacyNumeric &value) {
    if (value.present) old.numeric.emplace(key, static_cast<double>(value.value));
  };
  if (name == "mkradio4") {
    put("target_m3", old_values.target_m3);
    put("total_m3", old_values.total_m3);
  } else if (name == "fhkvdataiii") {
    put("current_hca", old_values.current_hca);
    put("previous_hca", old_values.previous_hca);
    put("temp_room_c", old_values.temp_room_c);
    put("temp_radiator_c", old_values.temp_radiator_c);
  } else {
    ++unsupported_;
    return;
  }
  if (old.numeric.empty()) {
    ++no_reference_;
    return;
  }
  xmq_runtime::VerifiedApplication snapshot;
  bool captured = false;
  if (xmq_runtime::encoding_matches_ci(
          xmq_runtime::ApplicationEncoding::StandardDv, telegram.tpl_ci)) {
    captured = xmq_runtime::capture_verified_payload(
        telegram, decoded, xmq_runtime::ApplicationEncoding::StandardDv,
        snapshot);
  } else if (xmq_runtime::encoding_matches_ci(
                 xmq_runtime::ApplicationEncoding::OriginalIxml, telegram.tpl_ci)) {
    captured = xmq_runtime::capture_verified_payload(
        telegram, decoded, xmq_runtime::ApplicationEncoding::OriginalIxml,
        snapshot);
  }
  if (!captured) {
    ++rejected_;
    return;
  }
  ++captured_;
  const auto disposition = diagnostic_.inspect(
      *registry_, snapshot.inner_mvt, snapshot.encoding,
      snapshot.payload, old, heap, true, name);
  const auto &stats = diagnostic_.counters();
  // Log only aggregates, never IDs, raw payloads, keys or old/new values.
  ESP_LOGI(TAG,
           "Live XMQ shadow: driver=%s disposition=%u compared=%u mismatches=%u "
           "matched=%u different=%u unsupported=%u observed=%u captured=%u "
           "rejected=%u min_free_heap=%u",
           name.c_str(), static_cast<unsigned>(disposition),
           static_cast<unsigned>(stats.last_compared),
           static_cast<unsigned>(stats.last_mismatches),
           stats.matched, stats.different, stats.errors + unsupported_,
           inspected_, captured_, rejected_, min_free_heap_);
}

void ShadowProbe::setup() {
  if (!enabled_) {
    ESP_LOGI(TAG, "XMQ probe disabled; existing meter decoder untouched");
    return;
  }
  try {
    registry_.reset(new xmq_runtime::DriverRegistry({
        xmq_runtime::load_driver(xmq_runtime::source_mkradio4()),
        xmq_runtime::load_driver(xmq_runtime::source_fhkvdataiii())}));
    diagnostic_.set_enabled(true);
  } catch (const std::exception &) {
    ESP_LOGE(TAG, "XMQ shadow driver init failed; legacy decoder remains active");
    registry_.reset();
    return;
  }
  active_probe = this;

  // Lab self-test only: synthetic clear payload, never a live radio frame.
  // Instantiate the real generic engine and shadow comparator in the
  // FULL Heltec ESPHome image to prove that C++ symbols actually link.
  xmq_runtime::Driver stub;
  stub.name = "xmq_build_only";
  stub.detect.push_back({"TCH", 0x95, 0x37});
  xmq_runtime::DriverRegistry registry({stub});
  xmq_runtime::ShadowDiagnostic diagnostic(1);
  diagnostic.set_enabled(true);
  xmq_runtime::ShadowReference reference;
  const auto result = diagnostic.inspect(
      registry, {"TCH", 0x95, 0x37},
      xmq_runtime::ApplicationEncoding::StandardDv,
      {0x43, 0x15, 0xA1, 0x00, 0x00}, reference);
  ESP_LOGI(TAG, "Synthetic-only XMQ linkage check result=%u",
           static_cast<unsigned>(result));
}

void ShadowProbe::dump_config() {
  ESP_LOGCONFIG(TAG, "Experimental XMQ lab shadow: %s (capture-only, NO sensor changes)",
                enabled_ ? "live bounded numeric shadow (no sensor writes)" : "disabled");
}

}  // namespace xmq_shadow_probe
}  // namespace esphome
