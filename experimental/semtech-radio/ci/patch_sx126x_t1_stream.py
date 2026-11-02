"""Opt-in T1 streaming patch for *official* ESPHome 2026.10.1 sx126x.

Only SX126x owns SPI, IRQ, FIFO and RX re-arm. Mode disabled by default.
Derived receive algorithm: SzczepanLeon/esphome-components wmbus_radio SX1262.
"""
from pathlib import Path
import sys
from check_sx126x_rx_hook import check

HEADER_METHOD = "  void register_listener(SX126xListener *listener) { this->listeners_.push_back(listener); }"
HEADER_MEMBER = "  std::vector<SX126xListener *> listeners_;"
LOOP_ANCHOR = "void SX126x::loop() {"
IRQ_ANCHOR = "  uint16_t irq = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR;"
CPP_INCLUDE = '#include "sx126x.h"'

PUBLIC_API = """  // Opt-in T1 mode. Existing ESPHome packet reception stays unchanged otherwise.
  using T1StreamCallback = void (*)(void *context, const uint8_t *bytes, size_t size, bool start, bool end);
  void set_t1_stream_callback(T1StreamCallback callback, void *context) {
    this->t1_stream_callback_ = callback;
    this->t1_stream_context_ = context;
  }
"""
PRIVATE_API = """  T1StreamCallback t1_stream_callback_{nullptr};
  void *t1_stream_context_{nullptr};
  bool t1_stream_receiving_{false};
  size_t t1_stream_received_{0};
  size_t t1_stream_target_{0};
  uint32_t t1_stream_started_{0};
  void t1_stream_loop_();
  void t1_stream_reset_();
"""

CPP_STREAM = """
// Format A T1 3-of-6 frame, single-owner FIFO stream. No meter publication here.
void SX126x::t1_stream_reset_() {
  this->t1_stream_receiving_ = false;
  this->t1_stream_received_ = 0;
  this->t1_stream_target_ = 0;
  uint8_t value = 0xFF;
  this->write_register_(0x06BB, &value, 1);  // GFSK payload length restored.
  uint8_t irq_bytes[2] = {0xFF, 0xFF};
  this->write_opcode_(RADIO_CLR_IRQSTATUS, irq_bytes, 2);
  this->set_mode_rx();
}

void SX126x::t1_stream_loop_() {
  if (!this->t1_stream_receiving_) {
    if (!this->dio1_pin_->digital_read()) return;
    uint8_t irq_bytes[2] = {};
    this->read_opcode_(RADIO_GET_IRQSTATUS, irq_bytes, 2);
    uint16_t flags = (static_cast<uint16_t>(irq_bytes[0]) << 8) | irq_bytes[1];
    this->write_opcode_(RADIO_CLR_IRQSTATUS, irq_bytes, 2);
    if (!(flags & IRQ_SYNCWORD_VALID)) return;
    this->t1_stream_receiving_ = true;
    this->t1_stream_received_ = 0;
    this->t1_stream_target_ = 0;
    this->t1_stream_started_ = millis();
    this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, true, false);
  }
  // An interrupted stream must never emit a packet. RX re-arm is radio-owned.
  if (millis() - this->t1_stream_started_ > 60) {
    this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true);
    this->t1_stream_reset_();
    return;
  }
  uint8_t write_ptr = 0;
  this->read_register_(0x0803, &write_ptr, 1); // RX_ADDR_PTR
  uint8_t payload = static_cast<uint8_t>(write_ptr - 1);
  this->write_register_(0x06BB, &payload, 1);
  const uint8_t read_ptr = static_cast<uint8_t>(this->t1_stream_received_);
  size_t available = static_cast<uint8_t>(write_ptr - read_ptr);
  if (this->t1_stream_target_ != 0)
    available = std::min(available, this->t1_stream_target_ - this->t1_stream_received_);
  else
    available = std::min<size_t>(available, 2 - this->t1_stream_received_);
  if (available == 0) return;
  std::vector<uint8_t> fragment(available);
  this->read_fifo_(read_ptr, fragment);
  this->t1_stream_received_ += available;
  this->t1_stream_callback_(this->t1_stream_context_, fragment.data(), fragment.size(), false, false);
  if (this->t1_stream_target_ == 0 && this->t1_stream_received_ >= 2) {
    // Determine the L-field from the first two encoded bytes via validated
    // 3-of-6 symbols. Receiver callback collects full frame separately.
    static constexpr int8_t inverse[64] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,3,-1,1,2,-1,
      -1,-1,-1,7,-1,-1,0,-1,-1,5,6,-1,4,-1,-1,-1,
      -1,-1,-1,11,-1,9,10,-1,-1,15,-1,-1,8,-1,-1,-1,
      -1,13,14,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    // The first two bytes are held in the radio-owned prefix buffer.
    const uint16_t first = (static_cast<uint16_t>(this->t1_stream_prefix_[0]) << 8) |
                           this->t1_stream_prefix_[1];
    const int8_t hi = inverse[(first >> 10) & 0x3F];
    const int8_t lo = inverse[(first >> 4) & 0x3F];
    if (hi < 0 || lo < 0) {
      this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true);
      this->t1_stream_reset_();
      return;
    }
    const size_t l = static_cast<size_t>((hi << 4) | lo);
    if (l < 10) {
      this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true);
      this->t1_stream_reset_();
      return;
    }
    const size_t crc_blocks = 1 + (l - 9 + 15) / 16;
    const size_t decoded = l + 1 + 2 * crc_blocks;
    if (decoded > 255) {
      this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true);
      this->t1_stream_reset_();
      return;
    }
    this->t1_stream_target_ = (decoded * 12 + 7) / 8;
  }
  if (this->t1_stream_target_ && this->t1_stream_received_ == this->t1_stream_target_) {
    this->t1_stream_callback_(this->t1_stream_context_, nullptr, 0, false, true);
    this->t1_stream_reset_();
  }
}
"""

# The generated code must preserve the first two bytes from FIFO, even across polling iterations.
CPP_STREAM = CPP_STREAM.replace(
    "  this->t1_stream_received_ += available;",
    """  if (this->t1_stream_received_ < 2) {
    for (size_t i = 0; i < available && this->t1_stream_received_ + i < 2; ++i)
      this->t1_stream_prefix_[this->t1_stream_received_ + i] = fragment[i];
  }
  this->t1_stream_received_ += available;"""
)
PRIVATE_API += "  uint8_t t1_stream_prefix_[2]{};\n"

def patch(header: str, cpp: str) -> tuple[str,str]:
    check(cpp)
    for token in (HEADER_METHOD, HEADER_MEMBER):
        if header.count(token) != 1:
            raise AssertionError("Unexpected official header: " + token)
    if cpp.count(LOOP_ANCHOR) != 1 or cpp.count(IRQ_ANCHOR) != 1:
        raise AssertionError("Unexpected official SX126x IRQ loop")
    header = header.replace(HEADER_METHOD, PUBLIC_API + HEADER_METHOD)
    header = "#define SX126X_EXPERIMENTAL_T1_STREAM 1\n" + header
    header = header.replace(HEADER_MEMBER, HEADER_MEMBER + "\n" + PRIVATE_API)
    cpp = cpp.replace(CPP_INCLUDE, CPP_INCLUDE + "\n#include <algorithm>")
    cpp = cpp.replace(LOOP_ANCHOR, CPP_STREAM + "\n" + LOOP_ANCHOR +
       "\n  if (this->t1_stream_callback_ != nullptr) {\n"
       "    this->t1_stream_loop_();\n    return;\n  }")
    cpp = cpp.replace(IRQ_ANCHOR,
       "  uint16_t irq = this->t1_stream_callback_ != nullptr ? IRQ_SYNCWORD_VALID : "
       "(IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR);")
    return header, cpp

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_sx126x_t1_stream.py path/to/esphome/components/sx126x")
    base = Path(sys.argv[1])
    h,c = patch((base/"sx126x.h").read_text(), (base/"sx126x.cpp").read_text())
    (base/"sx126x.h").write_text(h)
    (base/"sx126x.cpp").write_text(c)
    print("Installed opt-in T1 stream patch into isolated ESPHome source")
