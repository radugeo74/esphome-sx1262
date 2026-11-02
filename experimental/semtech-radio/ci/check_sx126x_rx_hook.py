"""Guard the exact upstream ESPHome 2026.10.1 SX126x RX insertion point.

This is a prerequisite for a single-owner progressive patch, not that patch.
Never silently rebase an RX FIFO hook onto a different implementation.
"""
from pathlib import Path
import sys

def check(source: str) -> None:
    required = (
        "void SX126x::loop()",
        "if ((status & IRQ_RX_DONE) == IRQ_RX_DONE)",
        "this->read_opcode_(RADIO_GET_RXBUFFERSTATUS, buf, 2);",
        "this->packet_.resize(buf[0]);",
        "this->read_fifo_(buf[1], this->packet_);",
        "this->call_listeners_(this->packet_",
        "uint16_t irq = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR;",
    )
    for needle in required:
        if needle not in source:
            raise AssertionError(f"ESPHome SX126x RX code changed: {needle}")
    idx=[source.index(x) for x in required[1:6]]
    if idx != sorted(idx):
        raise AssertionError("ESPHome SX126x RX path order changed")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_sx126x_rx_hook.py path/to/sx126x.cpp")
    check(Path(sys.argv[1]).read_text(encoding="utf-8"))
    print("Official SX126x RX hook anchor verified; no patch applied")
