"""Build-only opt-in ESPHome 2026.10.1 SX126x in-flight FIFO telemetry patch.

This modifies a private copy of official source, never the production driver.
It provides *measurement* of GET_RXBUFFERSTATUS before RX_DONE, not yet a
progressive FIFO read: chip behavior must be verified on Heltec hardware.
"""
from pathlib import Path
import sys
from check_sx126x_rx_hook import check

HEADER_NEEDLE = "  void register_listener(SX126xListener *listener) { this->listeners_.push_back(listener); }"
LOOP_NEEDLE = """  if (!this->dio1_pin_->digital_read()) {
    return;
  }
"""
HEADER_ADD = """  // Experimental, read-only observer: radio ownership stays in SX126x.
  using RxProgressObserver = void (*)(uint8_t fifo_count, uint8_t fifo_start);
  void set_rx_progress_observer(RxProgressObserver observer) { this->rx_progress_observer_ = observer; }
"""
MEMBER_NEEDLE = "  std::vector<SX126xListener *> listeners_;"
MEMBER_ADD = "  RxProgressObserver rx_progress_observer_{nullptr};\n"
LOOP_ADD = """  // Diagnostic only. GetRxBufferStatus may be meaningful only after RX_DONE;
  // this must be established on hardware before any progressive FIFO read.
  if (this->rx_progress_observer_ != nullptr &&
      this->modulation_ != PACKET_TYPE_LORA) {
    uint8_t fifo_status[2]{};
    this->read_opcode_(RADIO_GET_RXBUFFERSTATUS, fifo_status, 2);
    this->rx_progress_observer_(fifo_status[0], fifo_status[1]);
  }
"""

def patch(header: str, source: str) -> tuple[str, str]:
    check(source)
    for needle in (HEADER_NEEDLE, MEMBER_NEEDLE):
        if header.count(needle) != 1:
            raise AssertionError(f"unexpected upstream header: {needle}")
    if source.count(LOOP_NEEDLE) != 1:
        raise AssertionError("unexpected upstream loop()")
    header = header.replace(HEADER_NEEDLE, HEADER_ADD + HEADER_NEEDLE)
    header = "#define SX126X_EXPERIMENTAL_PROGRESS_PROBE 1\n" + header
    header = header.replace(MEMBER_NEEDLE, MEMBER_NEEDLE + "\n" + MEMBER_ADD.rstrip())
    source = source.replace(LOOP_NEEDLE, LOOP_ADD + LOOP_NEEDLE)
    return header, source

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_sx126x_progress_probe.py path/to/installed/sx126x")
    base = Path(sys.argv[1])
    h, cpp = patch((base/"sx126x.h").read_text(), (base/"sx126x.cpp").read_text())
    (base/"sx126x.h").write_text(h)
    (base/"sx126x.cpp").write_text(cpp)
    print("Experimental SX126x progress observer inserted (not a receiver)")
