#include "xmq_shadow_probe.h"
#include "esphome/components/wmbus_common/wmbus.h"
#include "esphome/components/wmbus_meter/wmbus_meter.h"
#include "xmq_telegram_capture.h"
#include "xmq_driver_sources.h"
#include "esp_heap_caps.h"
#include <cmath>

namespace esphome {
namespace xmq_shadow_probe {
static const char *const TAG = "xmq_shadow_probe";
static ShadowProbe *active_probe = nullptr;

bool ShadowProbe::memory_headroom_available() noexcept {
  const auto free_heap = static_cast<size_t>(
      heap_caps_get_free_size(MALLOC_CAP_8BIT));
  const auto largest_block = static_cast<size_t>(
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  return xmq_runtime::shadow_memory_allows_decode(free_heap, largest_block);
}

bool wants_legacy_readings(const void *source_meter) noexcept {
  return active_probe != nullptr && active_probe->wants_readings(source_meter);
}

void observe_legacy_telegram(const void *source_meter,
                             const ::Telegram &telegram, bool decoded,
                             const char *driver,
                             const LegacyReadings &old_values) noexcept {
  if (active_probe != nullptr)
    active_probe->observe(source_meter, telegram, decoded, driver, old_values);
}

void ShadowProbe::observe(const void *source_meter,
                          const ::Telegram &telegram, bool decoded,
                          const char *driver,
                          const LegacyReadings &old_values) noexcept {
  if (!wants_readings(source_meter)) return;
  try {
    if (!budget_.consume()) return;
    ++inspected_;
    // Observe only legacy post-decode callback metadata. The callback is
    // scoped to the shadow-integrated legacy drivers, NOT all radio frames.
    // No IDs, ACC, manufacturer, ciphertext or sensor values are retained.
    ci_census_.observe_telegram(telegram, decoded);
    // Emit one bounded aggregate per full observation window, never raw
    // frames or per-meter metadata. Fields are grouped by CI and TPL mode.
    if (inspected_ == kMaxObservations) {
      ESP_LOGI(TAG,
               "XMQ CI/TPL census scoped=legacy_shadow_callback "
               "A0=[%u,%u,%u,%u] A2=[%u,%u,%u,%u] "
               "72=[%u,%u,%u,%u] 7A=[%u,%u,%u,%u] "
               "other=[%u,%u,%u,%u] rejected=%u "
               "mode_order=none,5,7,other privacy=aggregate",
               ci_census_.accepted[0][0], ci_census_.accepted[0][1],
               ci_census_.accepted[0][2], ci_census_.accepted[0][3],
               ci_census_.accepted[1][0], ci_census_.accepted[1][1],
               ci_census_.accepted[1][2], ci_census_.accepted[1][3],
               ci_census_.accepted[2][0], ci_census_.accepted[2][1],
               ci_census_.accepted[2][2], ci_census_.accepted[2][3],
               ci_census_.accepted[3][0], ci_census_.accepted[3][1],
               ci_census_.accepted[3][2], ci_census_.accepted[3][3],
               ci_census_.accepted[4][0], ci_census_.accepted[4][1],
               ci_census_.accepted[4][2], ci_census_.accepted[4][3],
               ci_census_.rejected);
      ESP_LOGI(TAG,
               "XMQ shadow hourly observation budget exhausted (max %u, read-only)",
               static_cast<unsigned>(kMaxObservations));
    }
    const auto heap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    const auto largest_block = static_cast<uint32_t>(
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (heap < min_free_heap_) min_free_heap_ = heap;
    if (largest_block < min_largest_free_block_)
      min_largest_free_block_ = largest_block;
    // Never run the optional XMQ decoder if heap or allocation continuity
    // is below the conservative read-only shadow floor. Legacy already
    // handled the frame; this return never changes its result or sensors.
    if (!xmq_runtime::shadow_memory_allows_decode(heap, largest_block)) {
      ++low_memory_skipped_;
      return;
    }
    xmq_runtime::ShadowReference old;
    unsigned changed_numeric = 0;
    unsigned unchanged_numeric = 0;
    unsigned changed_dates = 0;
    unsigned unchanged_dates = 0;
    unsigned matched_numeric_dv = 0;
    unsigned unmatched_numeric_dv = 0;
    unsigned matched_date_dv = 0;
    unsigned unmatched_date_dv = 0;
    const std::string name = driver == nullptr ? "" : driver;
    auto put = [&](const char *key, const LegacyNumeric &value) {
      if (!value.present) return;
      old.numeric.emplace(key, static_cast<double>(value.value));
      if (value.changed) ++changed_numeric;
      else ++unchanged_numeric;
      if (value.matched_dv) ++matched_numeric_dv;
      else ++unmatched_numeric_dv;
    };
    if (name == "mkradio4") {
      // mkradio4 is proprietary CI=0xA2; legacy processContent()
      // directly calls setNumericValue without DV FieldInfo matching.
      // Only demand finite legacy references here; the separately
      // captured and decoded XMQ payload is the per-frame evidence.
      // Same-frame legacy setter provenance remains NOT proven.
      if (target_meter_ != nullptr &&
          (!decoded || telegram.tpl_ci != 0xA2 ||
           !old_values.target_m3.present ||
           !old_values.total_m3.present)) {
        ++no_reference_;
        return;
      }
      put("target_m3", old_values.target_m3);
      put("total_m3", old_values.total_m3);
    } else if (name == "mkradio4a") {
      // Require BOTH original legacy fields and their per-frame DV match.
      // An unchanged cached target value is not evidence of this telegram.
      if (!old_values.target_m3.present || !old_values.target_m3.matched_dv ||
          old_values.target_date.empty() ||
          !old_values.target_date_matched_dv) {
        ++no_reference_;
        return;
      }
      put("target_m3", old_values.target_m3);
      old.dates.emplace("target_date", old_values.target_date);
      if (old_values.target_date_changed) ++changed_dates;
      else ++unchanged_dates;
      ++matched_date_dv;
    } else if (name == "fhkvdataiii") {
      put("current_hca", old_values.current_hca);
      put("previous_hca", old_values.previous_hca);
      put("temp_room_c", old_values.temp_room_c);
      put("temp_radiator_c", old_values.temp_radiator_c);
      if (!old_values.current_date.empty()) {
        old.dates.emplace("current_date", old_values.current_date);
        if (old_values.current_date_changed) ++changed_dates;
        else ++unchanged_dates;
        if (old_values.current_date_matched_dv) ++matched_date_dv;
        else ++unmatched_date_dv;
      }
      if (!old_values.previous_date.empty()) {
        old.dates.emplace("previous_date", old_values.previous_date);
        if (old_values.previous_date_changed) ++changed_dates;
        else ++unchanged_dates;
        if (old_values.previous_date_matched_dv) ++matched_date_dv;
        else ++unmatched_date_dv;
      }
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
    // Reuse the existing XMQ projection; do not parse twice or write sensors.
    xmq_runtime::PrimaryReadiness xmq_fields;
    xmq_runtime::XmqFrameResult stage1_output;
    const bool stage1_mkradio4 =
        target_meter_ != nullptr && name == "mkradio4";
    const auto disposition = diagnostic_.inspect(
        *registry_, snapshot.inner_mvt, snapshot.encoding,
        snapshot.payload, old, heap, true, name,
        name == "mkradio4" ?
            std::vector<std::string>{"target_m3", "total_m3"} :
        name == "mkradio4a" ?
            std::vector<std::string>{"target_m3"} :
            std::vector<std::string>{"current_hca", "previous_hca",
                                     "temp_room_c", "temp_radiator_c"},
        &xmq_fields,
        name == "fhkvdataiii" ?
            std::vector<std::string>{"current_date", "previous_date"} :
        name == "mkradio4a" ?
            std::vector<std::string>{"target_date"} :
            std::vector<std::string>{},
        stage1_mkradio4 ? &stage1_output : nullptr);
    // Values come from XMQ's independent one-pass projection of the
    // verified legacy-decrypted application bytes, never from old values.
    // Reading them does NOT prove independent RF/AES or field setter provenance.
    if (stage1_mkradio4 && snapshot.encoding ==
            xmq_runtime::ApplicationEncoding::OriginalIxml &&
        xmq_fields.ready &&
        (disposition == xmq_runtime::ShadowDisposition::Match ||
         disposition == xmq_runtime::ShadowDisposition::Difference)) {
      const auto target = stage1_output.numeric.find("target_m3");
      const auto total = stage1_output.numeric.find("total_m3");
      if (target != stage1_output.numeric.end() &&
          total != stage1_output.numeric.end()) {
        // Only paired opt-in XMQ sensors are writable. Require complete
        // two-field readiness AND parity match before publishing either;
        // mismatch, invalid CI, low heap or exceptions publish NOTHING.
        // The values come from XMQ projection, not the legacy snapshot.
        if (stage2_target_ != nullptr && stage2_total_ != nullptr &&
            disposition == xmq_runtime::ShadowDisposition::Match &&
            std::isfinite(target->second.value) &&
            std::isfinite(total->second.value) &&
            target->second.value >= 0.0 && total->second.value >= 0.0) {
          stage2_target_->publish_state(target->second.value);
          stage2_total_->publish_state(total->second.value);
          ESP_LOGI(TAG,
                   "XMQ_STAGE2_MKRADIO4_TEST_PUBLISHED source=xmq_primary "
                   "parity=match fields=2 test_sensors=2 legacy_writes=0");
        }
        // Stage 3 default stays legacy. Opt-in applies only to the
        // selected existing meter, only after full XMQ parity match.
        // Meter's callback-scoped getter clears this after publication.
        if (xmq_primary_ && target_meter_ != nullptr &&
            source_meter == target_meter_ &&
            disposition == xmq_runtime::ShadowDisposition::Match &&
            std::isfinite(target->second.value) &&
            std::isfinite(total->second.value) &&
            target->second.value >= 0.0 && total->second.value >= 0.0) {
          target_meter_->set_xmq_frame_values(
              static_cast<float>(target->second.value),
              static_cast<float>(total->second.value));
          ESP_LOGI(TAG,
                   "XMQ_STAGE3_PRIMARY_ARMED source=xmq_primary "
                   "fields=2 parity=match selected_meter_only=1 "
                   "fallback=legacy");
        }
        ESP_LOGI(TAG,
                 "XMQ_STAGE1_MKRADIO4_RESULT target_m3=%.3f total_m3=%.3f "
                 "source=xmq_primary parity=%s "
                 "transport=legacy_verified provenance=not_proven "
                 "test_sensors=%u legacy_writes=0",
                 target->second.value, total->second.value,
                 disposition == xmq_runtime::ShadowDisposition::Match ?
                     "match" : "different",
                 static_cast<unsigned>(stage2_target_ != nullptr &&
                                       stage2_total_ != nullptr));
      }
    }
    const auto &stats = diagnostic_.counters();
    // Log only aggregates, never IDs, raw payloads, keys or old/new values.
    ESP_LOGI(TAG,
             "Live XMQ shadow: driver=%s disposition=%u compared=%u mismatches=%u "
             "matched=%u different=%u unsupported=%u calendar_refs=%u observed=%u captured=%u "
             "rejected=%u changed_numeric=%u unchanged_numeric=%u "
             "changed_dates=%u unchanged_dates=%u "
             "matched_numeric_dv=%u unmatched_numeric_dv=%u "
             "matched_date_dv=%u unmatched_date_dv=%u "
             "provenance=not_proven xmq_fields_ready=%u "
             "xmq_fields_present=%u xmq_fields_required=%u "
             "primary_transport=legacy_verified min_free_heap=%u "
             "min_largest_block=%u low_memory_skipped=%u",
             name.c_str(), static_cast<unsigned>(disposition),
             static_cast<unsigned>(stats.last_compared),
             static_cast<unsigned>(stats.last_mismatches),
             stats.matched, stats.different, stats.errors + unsupported_,
             static_cast<unsigned>(old.dates.size()),
             inspected_, captured_, rejected_, changed_numeric, unchanged_numeric,
             changed_dates, unchanged_dates, matched_numeric_dv,
             unmatched_numeric_dv, matched_date_dv, unmatched_date_dv,
             static_cast<unsigned>(xmq_fields.ready),
             static_cast<unsigned>(xmq_fields.available),
             static_cast<unsigned>(xmq_fields.required), min_free_heap_,
             min_largest_free_block_, low_memory_skipped_);
  } catch (const std::exception &) {
    ++unsupported_;
    ESP_LOGW(TAG, "Live XMQ shadow unsupported/error; legacy decoder unaffected");
  }
}

void ShadowProbe::setup() {
  if (!enabled_) {
    ESP_LOGI(TAG, "XMQ probe disabled; existing meter decoder untouched");
    return;
  }
  // Avoid allocating a second driver registry at boot when headroom
  // is already below the threshold. Legacy meter setup continues.
  if (!memory_headroom_available()) {
    ESP_LOGW(TAG, "XMQ shadow disabled at setup: insufficient heap headroom");
    return;
  }
  try {
    registry_.reset(new xmq_runtime::DriverRegistry({
        xmq_runtime::load_driver(xmq_runtime::source_mkradio4()),
        xmq_runtime::load_driver(xmq_runtime::source_mkradio4a()),
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
  ESP_LOGCONFIG(TAG, "Experimental XMQ: %s, decoder=%s, paired test entities=%s",
                enabled_ ? "enabled" : "disabled",
                xmq_primary_ ? "xmq" : "legacy",
                stage2_target_ && stage2_total_ ? "yes" : "no");
}

}  // namespace xmq_shadow_probe
}  // namespace esphome
