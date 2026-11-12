#pragma once

// Hardware-independent SX1276 FSK FIFO decisions, so the 64-byte FIFO and
// timeout policy can be covered by host-side tests without an RF module.
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace sx127x {

struct WmbusFifoGuard {
  static constexpr uint8_t IRQ1_REGISTER = 0x3E;
  static constexpr uint8_t IRQ2_REGISTER = 0x3F;
  static constexpr uint8_t IRQ1_TIMEOUT = 0x04;
  static constexpr uint8_t FIFO_FULL = 0x80;
  static constexpr uint8_t FIFO_EMPTY = 0x40;
  static constexpr uint8_t FIFO_LEVEL = 0x20;
  static constexpr uint8_t FIFO_OVERRUN = 0x10;

  // With FIFO threshold 15, FifoLevel guarantees >=16 received bytes.
  static constexpr uint8_t FIFO_THRESHOLD = 15;
  static constexpr size_t FIFO_BURST = FIFO_THRESHOLD + 1;
  // 384 raw bytes at 100 kbps take <31ms; allow task/ESP-IDF scheduling slack.
  static constexpr int64_t FRAME_DEADLINE_US = 120000;
  static constexpr uint32_t RX_WATCHDOG_INTERVAL_MS = 1000;
  static constexpr uint32_t IDLE_REARM_MS = 60000;
};

enum class WmbusFifoAction : uint8_t { WAIT, BURST, TAIL, OVERRUN };
enum class WmbusRxHealth : uint8_t { OK, FIFO_OVERRUN, RX_TIMEOUT, NOT_RX };

inline WmbusFifoAction wmbus_fifo_next_action(uint8_t irq2, size_t pending) {
  if ((irq2 & WmbusFifoGuard::FIFO_OVERRUN) != 0)
    return WmbusFifoAction::OVERRUN;
  if (pending >= WmbusFifoGuard::FIFO_BURST)
    return (irq2 & WmbusFifoGuard::FIFO_LEVEL) &&
                   !(irq2 & WmbusFifoGuard::FIFO_EMPTY)
               ? WmbusFifoAction::BURST : WmbusFifoAction::WAIT;
  return (irq2 & WmbusFifoGuard::FIFO_EMPTY)
             ? WmbusFifoAction::WAIT : WmbusFifoAction::TAIL;
}

inline WmbusRxHealth wmbus_rx_health(uint8_t mode, uint8_t irq1, uint8_t irq2) {
  if (irq2 & WmbusFifoGuard::FIFO_OVERRUN)
    return WmbusRxHealth::FIFO_OVERRUN;
  if (irq1 & WmbusFifoGuard::IRQ1_TIMEOUT)
    return WmbusRxHealth::RX_TIMEOUT;
  // SX127x RegOpMode: RX=5, RX_FS=4. Ignore non-mode flag bits.
  if ((mode & 0x07) != 0x05 && (mode & 0x07) != 0x04)
    return WmbusRxHealth::NOT_RX;
  return WmbusRxHealth::OK;
}

} // namespace sx127x
} // namespace esphome
