#!/usr/bin/env python3
"""Protect pre-CRC C1/A stage ordering and existing SPI/ISR behavior."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "components/sx127x/sx127x.cpp").read_text()
header = (root / "components/sx127x/sx127x.h").read_text()
task = source.split("void SX127x::wmbus_task_()", 1)[1].split("void SX127x::dump_config()", 1)[0]
stats = source.split("void SX127x::wmbus_log_statistics_()", 1)[1].split(
    "void IRAM_ATTR SX127x::wmbus_fifo_intr_", 1)[0]
assert "this->wmbus_read_fifo_(&marker, 1, deadline_us)" in task
marker = task.index("switch (classify_wmbus_sync54_marker(marker))")
header_read = task.index("this->wmbus_read_fifo_(body.data(), 3, deadline_us)")
candidates = task.index("wmbus_raw_candidates(")
queued = task.index("xQueueSend(this->wmbus_rx_queue_, &frame, 0)")
assert marker < header_read < candidates < queued
for field in ("fifo_marker_cd", "fifo_marker_3d", "fifo_marker_other",
              "fifo_marker_unreadable", "c1a_bad_prefix", "c1a_body_complete",
              "c1a_queued", "c1a_queue_dropped", "c1a_read_aborted",
              "irq1_preamble_at_fifo", "irq1_sync_at_fifo"):
    assert f"std::atomic<uint32_t> {field}" in header, field
    assert f"wmbus_stats_.{field}" in task, field
assert task.count("this->read_register_(WmbusFifoGuard::IRQ1_REGISTER)") == 1
assert "SX127x SYNC54 probe" in stats and "SX127x C1A stages" in stats
assert "s.c1a_verified.load()" in stats
isr = source.split("void IRAM_ATTR SX127x::wmbus_fifo_intr_", 1)[1].split(
    "void SX127x::wmbus_task_entry_", 1)[0]
assert "read_register_(" not in isr and "ESP_LOG" not in isr
print("PASS: C1/A pre-CRC stages, reused IRQ1 read, no added SPI/ISR logging")
