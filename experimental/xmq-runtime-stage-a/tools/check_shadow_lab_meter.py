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
change='''  const bool decoded = this->meter->handleTelegram(
      about, frame->data(), false, &adresses, &id_match, telegram.get());

  // Lab-only observer, AFTER the existing AES/parser/field update path.
  // No changes to old handled semantics, sensor output, or radio ownership.
  xmq_shadow_probe::observe_legacy_telegram(*telegram, decoded);

'''
assert clone.count(change)==1
clone=clone.replace(change,'''  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());

''')
assert clone==base, "unexpected changes to meter handling outside lab hook"
probe=(lab.parent/"xmq_shadow_probe/xmq_shadow_probe.cpp").read_text()
assert "if (!enabled_ || inspected_ >= kMaxObservations) return;" in probe
assert "observe_legacy_telegram" in probe
yaml=(root/"experimental/xmq-runtime-stage-a/heltec-xmq-shadow-build.yaml").read_text()
assert "enabled: false" in yaml
assert "components: [wmbus_meter, xmq_shadow_probe]" in yaml
print("SHADOW_LAB_METER_OK: only post-decode observer changed; legacy dispatch unchanged")
