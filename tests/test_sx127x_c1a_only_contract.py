#!/usr/bin/env python3
"""C1/A-only must not consume a phantom marker or change T1+C1 behavior."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cpp = (root / "components/sx127x/sx127x.cpp").read_text()
hdr = (root / "components/sx127x/sx127x.h").read_text()
schema = (root / "components/sx127x/__init__.py").read_text()
combined_fixture = (root / "tests/lilygo-t3s3-sx1276-ci.yaml").read_text()
isolated_fixture = (root / "tests/lilygo-t3s3-sx1276-c1a-only-ci.yaml").read_text()

assert 'wmbus_c1a_only: true' in isolated_fixture
assert 'sync_value: [0x54, 0xCD]' in isolated_fixture
assert 'native_stream_mode: T1_C1' in isolated_fixture
assert '\nsensor:' not in isolated_fixture
assert 'wmbus_c1a_only:' not in combined_fixture
assert 'sync_value: [0x54]' in combined_fixture
assert 'CONF_WMBUS_C1A_ONLY' in schema
assert 'config[CONF_SYNC_VALUE] != [0x54, 0xCD]' in schema
assert 'cv.Optional(CONF_WMBUS_C1A_ONLY, default=False)' in schema
assert 'set_wmbus_c1a_only' in hdr
assert 'c1a_hw_sync_fifo' in hdr
assert 'this->wmbus_c1a_only_\n        ? this->wmbus_c1_mode_' in cpp
assert 'this->sync_value_[1] == 0xCD' in cpp
assert 'HW sync 54 CD' in cpp
task = cpp.split("void SX127x::wmbus_task_()")[1].split("void SX127x::dump_config()")[0]
assert "uint8_t marker = this->wmbus_c1a_only_ ? 0xCD : 0x3D;" in task
assert "if (this->wmbus_c1a_only_) {" in task
assert "++this->wmbus_stats_.c1a_hw_sync_fifo;" in task
assert "else if (this->wmbus_c1_mode_) {" in task
markerread = task.index("status = this->wmbus_read_fifo_(&marker, 1, deadline_us)")
direct = task.index("if (this->wmbus_c1a_only_) {")
headerread = task.index("status = this->wmbus_read_fifo_(body.data(), 3, deadline_us)")
assert direct < markerread < headerread
assert "frame.bytes[0] = 0x54;" in task
assert "frame.bytes[1] = marker;" in task
assert "wmbus_raw_candidates(" in task
assert task.count("this->read_register_(WmbusFifoGuard::IRQ1_REGISTER)") == 1
assert "hw_sync54CD_fifo=%u" in cpp
assert "c1a_hw_sync_fifo.load()" in cpp
print("PASS: opt-in 54CD sync, no phantom CD read, old combined path preserved")
