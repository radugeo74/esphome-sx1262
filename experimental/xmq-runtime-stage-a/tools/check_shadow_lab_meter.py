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
assert "const bool probe_requested = xmq_shadow_probe::wants_legacy_readings();" in observer
assert "if (probe_requested) snapshot(before);" in observer
assert "if (decoded && probe_requested)" in observer
assert "after.changed = after.present" in observer
assert "telegram->dv_entries" in observer
assert "entry.second.second.hasFieldInfo(field)" in observer
assert "value.matched_dv = matched_dv(" in observer
assert "legacy.current_date_matched_dv" in observer
assert "capture(\"target_m3\"" in observer
assert "capture(\"current_hca\"" in observer
assert "xmq_shadow_probe::observe_legacy_telegram(" in observer
assert "frame->mark_as_handled()" not in observer
assert "set_radio(" not in observer
clone = clone[:start] + '''  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());
''' + clone[end:]
assert clone==base, "unexpected changes to meter handling outside lab hook"
probe=(lab.parent/"xmq_shadow_probe/xmq_shadow_probe.cpp").read_text()
assert "if (!wants_readings()) return;" in probe
assert "diagnostic_.inspect(" in probe
assert "source_mkradio4()" in probe
assert "source_fhkvdataiii()" in probe
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
# The opt-in observer may inspect a successfully decoded Telegram, but must
# never become another receiver, invoke legacy decoding, or publish sensors.
# This guard is source-level contract checking, not an RF/runtime test.
for forbidden in (
    "add_frame_handler(", "add_meter_frame_handler(",
    "handleTelegram(", "mark_as_handled(", "publish_state(",
    "sensor->publish_state(", "sensor::Sensor", "set_radio(",
):
    assert forbidden not in probe, "shadow probe gained an unsafe sink: " + forbidden
assert "if (!wants_readings()) return;" in probe
assert "if (!enabled_)" in probe
assert "active_probe = this;" in probe
assert "capture_verified_payload(" in probe
assert "diagnostic_.inspect(" in probe
header=(lab.parent/"xmq_shadow_probe/xmq_shadow_probe.h").read_text()
assert "bool enabled_{false};" in header
assert "if (!enabled_ || !registry_) return false;" in header
assert "return inspected_ < kMaxObservations;" in header
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
assert "return budget_.may_observe();" in header
assert "if (!budget_.consume()) return;" in probe
assert "if (!enabled_ || !registry_) return false;" in header
assert "hourly observation budget renewed" in header
print("SHADOW_HOURLY_BUDGET_CONTRACT_OK: 128 observations/hour, uint32 rollover-safe renewal, no sensor writes")

assert "set_enabled(bool enabled)" in header
enabled_yaml=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-enabled-lab.yaml").read_text()
assert enabled_yaml.count("enabled: true") == 1
assert "!include heltec-xmq-shadow-build.yaml" in enabled_yaml
for unsafe in ("wifi:", "api:", "ota:", "mqtt:", "!secret", "password:", "key:"):
    assert unsafe not in enabled_yaml, "unsafe enabled shadow lab: " + unsafe
print("SHADOW_READ_ONLY_CONTRACT_OK: post-decode capture only, no radio ownership, sensor writes or network; bounded opt-in")
yaml=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-build.yaml").read_text()
assert "enabled: false" in yaml
assert "components: [wmbus_meter, xmq_shadow_probe]" in yaml
print("SHADOW_LAB_METER_OK: guarded legacy field snapshot + read-only XMQ comparison; dispatch unchanged")
