#!/usr/bin/env python3
"""Guard actual streaming radio/HA RSSI wiring, not just an isolated formula.

A stream may finish before RxDone. Do not silently substitute the GFSK
payload average, LoRa RSSI field or instantaneous noise for RssiSync.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[3]
source=(root/"components/sx126x/sx126x.cpp").read_text(encoding="utf-8")
header=(root/"components/sx126x/gfsk_rssi.h").read_text(encoding="utf-8")
receiver=(root/"components/wmbus_native_sx126x/native_receiver_probe.cpp").read_text(encoding="utf-8")
stream=source.split("void SX126x::t1_stream_loop_()",1)[1].split("void SX126x::loop()",1)[0]
assert '#include "gfsk_rssi.h"' in source
assert "this->read_opcode_(RADIO_GET_PACKETSTATUS, packet_status, 3)" in stream
assert "gfsk_rssi::parse(packet_status)" in stream
assert "const float rssi_dbm = measurement.sync_dbm;" in stream
assert "this->t1_stream_reset_();" in stream
assert "this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true, rssi_dbm)" in stream
assert stream.index("gfsk_rssi::parse(packet_status)") < stream.index("this->t1_stream_reset_();",stream.index("  if (this->t1_stream_target_ &&"))
assert "measurement.average_dbm" in stream and "measurement.average_raw" in stream
assert "RssiSync" in header and "RssiAvg" in header
assert "dbm(status[1])" in header and "dbm(status[2])" in header
assert "rssi_dbm" in receiver and "round_for_sensor(rssi_dbm" in receiver
assert "self->publish_secure_batch2_(" in receiver
assert "self->publish_plain_batch2_(" in receiver
assert "rssi_sensors_" in receiver
print("PASS live GFSK RSSI source contract: SX126x gets sync sample before RX reset, "
      "avg is diagnostic only, same per-meter HA path retained.")
