#!/usr/bin/env python3
"""Synthetic tests for Stage 1: one existing mkradio4, all others untouched."""
from copy import deepcopy
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from stage1_mkradio4_release_gate import verify_stage1_config, release_stage1


def fixture():
    original = {
        "esphome": {"name": "ci-synthetic-heltec"},
        "esp32": {"board": "heltec_wifi_lora_32_V3"},
        "sx126x": {"frequency": 868950000},
        "wmbus_radio": {"native_stream_mode": "T1_C1"},
        "wmbus_meter": (
            [{"id": "ci_heat_" + str(i), "type": "fhkvdataiii",
              "mode": ["T1"], "key": "ci-placeholder"} for i in range(7)]
            + [{"id": "ci_water_" + str(i), "type": "mkradio4",
                "mode": ["T1", "C1"], "key": "ci-placeholder"} for i in range(5)]
        ),
        "sensor": [{"name": "ci_sensor", "field": "target_m3"}],
        "external_components": [{
            "source": {"type": "git", "ref": "a" * 40},
            "components": ["sx126x", "wmbus_common", "wmbus_radio",
                           "wmbus_meter"],
        }],
    }
    candidate = deepcopy(original)
    candidate["external_components"] = [{
        "source": {"type": "git", "ref": "a" * 40},
        "components": ["sx126x", "wmbus_common", "wmbus_radio"],
    }, {
        "source": {"type": "git", "ref": "b" * 40},
        "components": ["wmbus_meter", "xmq_shadow_probe"],
    }]
    candidate["xmq_shadow_probe"] = {
        "id": "ci_shadow_probe", "enabled": True, "target_meter": "ci_water_0"}
    return original, candidate


def expect_block(old, new, reason):
    try:
        verify_stage1_config(old, new)
    except ValueError:
        return
    raise AssertionError("unsafe Stage 1 candidate passed: " + reason)


def main():
    old, new = fixture()
    assert verify_stage1_config(old, new)
    for label, mutation in [
        ("probe disabled", lambda c: c["xmq_shadow_probe"].update(enabled=False)),
        ("probe missing target", lambda c: c["xmq_shadow_probe"].pop("target_meter")),
        ("probe all meters", lambda c: c["xmq_shadow_probe"].update(target_meter="")),
        ("probe unknown meter", lambda c: c["xmq_shadow_probe"].update(target_meter="nonexistent")),
        ("probe heat meter", lambda c: c["xmq_shadow_probe"].update(target_meter="ci_heat_0")),
        ("probe unsupported water", lambda c: c["wmbus_meter"][7].update(type="mkradio4a")),
        ("probe extra setting", lambda c: c["xmq_shadow_probe"].update(debug=True)),
        ("legacy secret changed", lambda c: c["wmbus_meter"][0].update(key="changed")),
        ("legacy ID changed", lambda c: c["wmbus_meter"][0].update(id="changed")),
        ("legacy topology changed", lambda c: c["wmbus_meter"].pop()),
        ("network altered", lambda c: c["esphome"].update(name="different")),
        ("RF changed", lambda c: c["sx126x"].update(frequency=868000000)),
        ("mode downgraded", lambda c: c["wmbus_radio"].update(native_stream_mode="T1")),
        ("unpinned Git", lambda c: c["external_components"][1]["source"].update(ref="experimental/xmq-runtime-stage-a")),
        ("component missing", lambda c: c["external_components"][1].update(components=["wmbus_meter"])),
    ]:
        variant = deepcopy(new)
        mutation(variant)
        expect_block(old, variant, label)
    different = deepcopy(new)
    different["wmbus_meter"][8]["id"] = "ci_water_0"
    expect_block(old, different, "duplicate meter component ID")

    with TemporaryDirectory(prefix="xmq-stage1-ci-") as tmp:
        p = Path(tmp)
        ref = p / "reference.yaml"
        can = p / "candidate.yaml"
        oldbin = p / "old.bin"
        newbin = p / "new.bin"
        manifest = p / "manifest.json"
        events = []
        def resolved(path, fixture):
            events.append(("read", path.name))
            return old if path == ref else new
        def images(before, after, output):
            events.append(("verify", before.name, after.name, output.name))
        with (
            patch("stage1_mkradio4_release_gate.validate_and_read", side_effect=resolved),
            patch("stage1_mkradio4_release_gate.verify", side_effect=images),
        ):
            release_stage1(ref, can, oldbin, newbin, manifest,
                           resolved_fixtures=True)
        assert events == [
            ("read", "reference.yaml"), ("read", "candidate.yaml"),
            ("verify", "old.bin", "new.bin", "manifest.json"),
        ], events
        assert not manifest.exists()
    print("XMQ_STAGE1_RELEASE_GATE_OK: selected one mkradio4, reject unsafe diffs, no flash")


if __name__ == "__main__":
    main()
