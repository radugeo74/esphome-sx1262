#!/usr/bin/env python3
"""Fail-closed checks for the isolated post-decode meter clone.

No modifications to the active components/ are permitted. This verifies the
clone matches the current active meter except for a narrowly scoped hook.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[3]
lab=root/"experimental/xmq-runtime-stage-a/components/wmbus_meter"
original=root/"components/wmbus_meter"
clone=(lab/"wmbus_meter.cpp").read_text()
base=(original/"wmbus_meter.cpp").read_text()
assert (lab/"wmbus_meter.h").read_bytes()==(original/"wmbus_meter.h").read_bytes()
for name in ("__init__.py","automation.h","base_sensor.cpp","base_sensor.h",
             "base_sensor.py","sensor/__init__.py","sensor/sensor.cpp",
             "sensor/sensor.h","text_sensor/__init__.py",
             "text_sensor/text_sensor.cpp","text_sensor/text_sensor.h"):
    assert (lab/name).read_bytes()==(original/name).read_bytes(),name

include='#include "esphome/components/xmq_shadow_probe/xmq_live_capture_hook.h"\n'
assert clone.count(include)==1
clone=clone.replace(include,"")
start = clone.index("  // Baseline values are observed BEFORE legacy decoding.")
end = clone.index("\n  if (id_match) {", start)
observer = clone[start:end]
assert observer.count("this->meter->handleTelegram(") == 1
assert "const bool probe_requested = xmq_shadow_probe::wants_legacy_readings(this);" in observer
assert "if (probe_requested) snapshot(before);" in observer
assert "if (decoded && probe_requested)" in observer
assert "after.changed = after.present" in observer
assert "telegram->dv_entries" in observer
assert "entry.second.second.hasFieldInfo(field)" in observer
assert "value.matched_dv = matched_dv(" in observer
assert "legacy.current_date_matched_dv" in observer
assert "capture(\"target_m3\"" in observer
assert "capture(\"current_hca\"" in observer
assert 'shadow_driver == "mkradio4a"' in observer
assert 'readings.target_date = date("target");' in observer
assert 'matched_dv("target", Quantity::PointInTime);' in observer
assert "legacy.target_date_changed" in observer
assert "xmq_shadow_probe::observe_legacy_telegram(" in observer
assert "this, *telegram, decoded, shadow_driver.c_str(), legacy" in observer
assert "target_meter: lab_water" in (root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-enabled-lab.yaml").read_text()
assert 'cv.Optional(CONF_TARGET_METER): cv.use_id(Meter)' in (lab.parent/"xmq_shadow_probe/__init__.py").read_text()
assert "wants_legacy_readings(this)" in observer
# The callback is outside the field-snapshot branch. A successfully
# decoded mkradio4a can therefore contribute metadata even though the
# numerical XMQ shadow currently supports only mkradio4/fhkvdataiii.
assert observer.index("xmq_shadow_probe::observe_legacy_telegram(") > observer.index(
    "if (decoded && probe_requested)"
)
assert observer.index("xmq_shadow_probe::observe_legacy_telegram(") > observer.index(
    "this->meter->handleTelegram("
)
assert "frame->mark_as_handled()" not in observer
assert "set_radio(" not in observer
clone = clone[:start] + '''  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());
''' + clone[end:]
assert clone==base, "unexpected changes to meter handling outside lab hook"
probe=(lab.parent/"xmq_shadow_probe/xmq_shadow_probe.cpp").read_text()
assert "if (!wants_readings(source_meter)) return;" in probe
assert "shadow_memory_allows_decode(heap, largest_block)" in probe
assert "heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)" in probe
assert "++low_memory_skipped_" in probe
assert probe.index("shadow_memory_allows_decode(heap, largest_block)") < probe.index(
    "diagnostic_.inspect("
)
assert "diagnostic_.inspect(" in probe
assert "source_mkradio4()" in probe
assert "stage1_mkradio4 ? &stage1_output : nullptr" in probe
assert "XMQ_STAGE1_MKRADIO4_RESULT target_m3=%.3f total_m3=%.3f" in probe
assert 'disposition == xmq_runtime::ShadowDisposition::Difference' in probe
assert '"match" : "different"' in probe
assert "source=xmq_primary parity=%s" in probe
assert "provenance=not_proven" in probe
assert "old_values.target_m3.matched_dv" in probe  # for mkradio4a only
assert "telegram.tpl_ci != 0xA2" in probe
assert "mkradio4 is proprietary CI=0xA2" in probe
assert probe.count("publish_state(") == 2
assert "stage2_target_->publish_state(target->second.value);" in probe
assert "stage2_total_->publish_state(total->second.value);" in probe
assert "disposition == xmq_runtime::ShadowDisposition::Match &&" in probe
assert "stage2_target_ != nullptr && stage2_total_ != nullptr" in probe
assert "XMQ_STAGE2_MKRADIO4_TEST_PUBLISHED" in probe
assert "source_fhkvdataiii()" in probe
assert "source_mkradio4a()" in probe
assert 'name == "mkradio4a"' in probe
assert "old_values.target_date_matched_dv" in probe
assert 'old.dates.emplace("target_date", old_values.target_date)' in probe
assert 'std::vector<std::string>{"target_date"}' in probe
# Incomplete or stale reference must abort before any XMQ comparison.
assert probe.index("!old_values.target_m3.matched_dv") < probe.index(
    "diagnostic_.inspect("
)
assert "old.numeric.empty()" in probe
assert "provenance=not_proven" in probe
assert "XMQ shadow hourly observation budget exhausted" in probe
assert "if (inspected_ == kMaxObservations)" in probe
assert "static_cast<unsigned>(kMaxObservations)" in probe
assert "changed_numeric" in probe
assert "matched_numeric_dv" in probe
assert "unmatched_numeric_dv" in probe
assert "matched_date_dv" in probe
assert "observe_legacy_telegram" in probe
assert "ci_census_.observe_telegram(telegram, decoded);" in probe
assert probe.index("ci_census_.observe_telegram(telegram, decoded);") < probe.index(
    'if (name == "mkradio4")'
)
assert "XMQ CI/TPL census scoped=legacy_shadow_callback" in probe
# The opt-in observer may inspect a successfully decoded Telegram, but must
# never become another receiver, invoke legacy decoding, or publish sensors.
# This guard is source-level contract checking, not an RF/runtime test.
for forbidden in (
    "add_frame_handler(", "add_meter_frame_handler(",
    "handleTelegram(", "mark_as_handled(",
    "sensor->publish_state(", "set_radio(",
):
    assert forbidden not in probe, "shadow probe gained an unsafe sink: " + forbidden
assert "if (!wants_readings(source_meter)) return;" in probe
assert "if (!enabled_)" in probe
assert "active_probe = this;" in probe
assert "capture_verified_payload(" in probe
assert "diagnostic_.inspect(" in probe
header=(lab.parent/"xmq_shadow_probe/xmq_shadow_probe.h").read_text()
assert '#include "xmq_shadow_memory_gate.h"' in header
assert "low_memory_skipped_ = 0;" in header
assert "min_largest_free_block_ = UINT32_MAX;" in header
assert "bool enabled_{false};" in header
assert "if (!enabled_ || !registry_) return false;" in header
assert "kMaxObservations = kShadowWindowMaxObservations" in header
# Budget must renew at most once per hour, without unbounded observations
# or permanent silence after 128 successful meter telegrams.
budget=(lab.parent/"xmq_shadow_probe/xmq_shadow_budget.h").read_text()
assert "kShadowWindowMillis = 3600000UL" in budget
assert "kShadowWindowMaxObservations = 128" in budget
assert "static_cast<uint32_t>(now_ms - started_ms) >= kShadowWindowMillis" in budget
assert "class ShadowObservationBudget" in budget
assert "bool consume() noexcept" in budget
assert "bool renew_if_elapsed(uint32_t now_ms) noexcept" in budget
assert "used_ < kShadowWindowMaxObservations" in budget
assert '#include "xmq_shadow_budget.h"' in header
assert "budget_.renew_if_elapsed(now)" in header
assert "ShadowObservationBudget budget_{};" in header
assert "inspected_ = 0;" in header
assert "diagnostic_ = xmq_runtime::ShadowDiagnostic(kMaxObservations, 0.051);" in header
assert "diagnostic_.set_enabled(true);" in header
assert "if (!budget_.may_observe()) return false;" in header
assert "if (!memory_headroom_available())" in header
assert (
    header.index("if (!memory_headroom_available())") <
    header.index("return true;", header.index("if (!memory_headroom_available())"))
)
assert "++low_memory_skipped_;" in header
assert "bool memory_headroom_available() noexcept;" in header
assert "bool ShadowProbe::memory_headroom_available() noexcept" in probe
assert "XMQ shadow disabled at setup: insufficient heap headroom" in probe
assert (
    probe.index("bool ShadowProbe::memory_headroom_available() noexcept") <
    probe.index("void ShadowProbe::observe(")
)
assert "shadow_memory_allows_decode(free_heap, largest_block)" in probe
assert "if (!budget_.consume()) return;" in probe
assert "if (!enabled_ || !registry_) return false;" in header
assert "hourly observation budget renewed" in header
print("SHADOW_HOURLY_BUDGET_CONTRACT_OK: 128 observations/hour, uint32 rollover-safe renewal, no sensor writes")

assert "set_enabled(bool enabled)" in header
assert "set_target_meter(wmbus_meter::Meter *meter)" in header
assert "target_meter_ != nullptr && source_meter != target_meter_" in header
assert "bool wants_legacy_readings(const void *source_meter)" in (lab.parent/"xmq_shadow_probe/xmq_live_capture_hook.h").read_text()
enabled_yaml=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-enabled-lab.yaml").read_text()
assert enabled_yaml.count("enabled: true") == 1
assert "!include heltec-xmq-shadow-build.yaml" in enabled_yaml
for unsafe in ("wifi:", "api:", "ota:", "mqtt:", "!secret", "password:", "key:"):
    assert unsafe not in enabled_yaml, "unsafe enabled shadow lab: " + unsafe
print("SHADOW_READ_ONLY_CONTRACT_OK: post-decode capture only, no radio ownership, sensor writes or network; bounded opt-in")
yaml=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-build.yaml").read_text()
assert "enabled: false" in yaml
assert "components: [wmbus_meter, xmq_shadow_probe]" in yaml
assert "id: lab_water_mkradio4a_census" in yaml
assert "type: mkradio4a" in yaml
assert "meter_id: 0x12345679" in yaml
assert "key: !secret" not in yaml
print("SHADOW_LAB_METER_OK: guarded legacy field snapshot + read-only XMQ comparison; dispatch unchanged")
# Actual SX1262 install receives T1 and C1; test this configuration
# separately from the old T1-only synthetic baseline, never as an OTA image.
t1c1=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-t1-c1-build.yaml").read_text()
assert "packages:" not in t1c1, "wmbus_meter list package merge duplicates IDs"
assert "native_stream_mode: T1_C1" in t1c1
assert t1c1.count("mode: [T1, C1]") == 2
assert t1c1.count("  - id: lab_water\n") == 1
assert t1c1.count("  - id: lab_water_mkradio4a_census\n") == 1
# Build fixture may be standalone to avoid ESPHome list concatenation, but
# must remain a mechanically checked derivative of the legacy-safe baseline.
baseline = yaml
expected = (baseline.replace(
    "esphome:\n  name: sx126x-xmq-shadow-lab",
    "esphome:\n  name: sx126x-xmq-shadow-t1c1-lab"
).replace(
    "  enabled: false  # opt-in capture-only; still disabled in build fixture",
    "  enabled: true  # only for synthetic offline CI, NEVER FLASH\n  target_meter: lab_water  # select only synthetic mkradio4; never log meter IDs"
).replace(
    "  native_stream_mode: T1\n",
    "  native_stream_mode: T1_C1\n"
).replace("    mode: [T1]\n", "    mode: [T1, C1]\n"))
assert t1c1.split("# Derived from heltec-xmq-shadow-build.yaml, with exact drift checked in CI.\n", 1)[1] == expected, (
    "standalone T1_C1 fixture drifted from source baseline"
)
assert "type: mkradio4a" in t1c1
assert "type: mkradio4" in t1c1
for unsafe in ("wifi:", "api:", "ota:", "mqtt:", "!secret", "password:", "key:"):
    assert unsafe not in t1c1, "unsafe T1_C1 build fixture: " + unsafe
print("SHADOW_T1_C1_LAB_CONTRACT_OK: two synthetic T1+C1 meters, no network, no keys, NO FLASH")
stage2=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-stage2-sensors-build.yaml").read_text()
assert "!include heltec-xmq-shadow-t1-c1-build.yaml" in stage2
assert "target_meter: lab_water" in stage2
assert stage2.count("test_target_m3:") == 1 and stage2.count("test_total_m3:") == 1
assert stage2.count("internal: true") == 2
for unsafe in ("wifi:", "api:", "ota:", "mqtt:", "!secret", "password:", "key:"):
    assert unsafe not in stage2, "unsafe Stage 2 build fixture: "+unsafe
schema=(lab.parent/"xmq_shadow_probe/__init__.py").read_text()
assert "has_target != has_total" in schema
assert 'if has_target and (not config[CONF_ENABLED] or CONF_TARGET_METER not in config)' in schema
assert "await sensor.new_sensor(config[CONF_TEST_TARGET])" in schema
assert "await sensor.new_sensor(config[CONF_TEST_TOTAL])" in schema
assert "set_stage2_sensors(target, total)" in schema
print("XMQ_STAGE2_SENSOR_CONTRACT_OK: paired opt-in XMQ-only test sensors, gated parity, no legacy writes")


