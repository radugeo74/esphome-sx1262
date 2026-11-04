#!/usr/bin/env python3
"""Guard the T1 verified-event contract separately from sensor publication.

The actual T1 receive path must notify YAML on_verified for every CRC-verified
T1 DLL frame; the configured-meter/transport gate must still guard XMQ sensors.
No real meter IDs or telegrams are used by this test.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[3]
src = (root / "components/wmbus_native_sx126x/native_receiver_probe.cpp").read_text()
cfg = (root / "components/wmbus_native_sx126x/__init__.py").read_text()
hdr = (root / "components/wmbus_native_sx126x/native_receiver_probe.h").read_text()
lab = (root / "experimental/semtech-radio/heltec-sx126x-combined-lab.yaml").read_text()
begin = src.index("if (state == t1::RxState::Verified)")
end = src.index("} else {\n      ++self->t1_stats_.rejected", begin)
verified = src[begin:end]
callback = "self->fire_verified_(frame.header.meter_id, rssi_dbm, frame.bytes);"
assert verified.count(callback) == 1, "The T1 verified callback must fire exactly once"
assert verified.index(callback) < verified.index("const auto admission=")
assert verified.index(callback) < verified.index("if (kind == registry::Kind::Configured &&")
assert verified.index("const auto admission=") < verified.index("if (kind == registry::Kind::Configured &&")
assert "t1_unconfigured_hex" not in (src + cfg + hdr + lab)
assert "on_verified" in cfg, "Existing YAML diagnostic trigger must remain available"
print("PASS: all CRC-verified T1 events reach on_verified; sensor gate unchanged")
