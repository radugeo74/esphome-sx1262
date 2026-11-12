#include "components/sx127x/wmbus_fifo_guard.h"
#include <cassert>
#include <cstdio>

using esphome::sx127x::WmbusFifoAction;
using esphome::sx127x::WmbusFifoGuard;
using esphome::sx127x::WmbusRxHealth;
using esphome::sx127x::wmbus_fifo_next_action;
using esphome::sx127x::wmbus_rx_health;

int main() {
  using G = WmbusFifoGuard;
  using A = WmbusFifoAction;
  using H = WmbusRxHealth;
  static_assert(G::FIFO_BURST == 16, "Unexpected burst/threshold size");
  static_assert(G::FRAME_DEADLINE_US > 384 * 80, "Deadline too short for 384 raw bytes");

  // Do not read a 16-byte block until FifoLevel confirms its availability.
  assert(wmbus_fifo_next_action(G::FIFO_LEVEL, 16) == A::BURST);
  assert(wmbus_fifo_next_action(G::FIFO_LEVEL, 255) == A::BURST);
  assert(wmbus_fifo_next_action(G::FIFO_EMPTY, 16) == A::WAIT);
  assert(wmbus_fifo_next_action(0, 16) == A::WAIT);
  assert(wmbus_fifo_next_action(G::FIFO_LEVEL | G::FIFO_EMPTY, 16) == A::WAIT);

  // Short tails read only actually available bytes; never an empty FIFO.
  assert(wmbus_fifo_next_action(0, 15) == A::TAIL);
  assert(wmbus_fifo_next_action(0, 1) == A::TAIL);
  assert(wmbus_fifo_next_action(G::FIFO_EMPTY, 1) == A::WAIT);
  assert(wmbus_fifo_next_action(G::FIFO_EMPTY, 15) == A::WAIT);

  // Overrun always aborts, even with FifoLevel or a partial tail.
  assert(wmbus_fifo_next_action(G::FIFO_OVERRUN | G::FIFO_LEVEL, 64) == A::OVERRUN);
  assert(wmbus_fifo_next_action(G::FIFO_OVERRUN, 1) == A::OVERRUN);
  assert(wmbus_rx_health(0x05, 0, G::FIFO_EMPTY) == H::OK);
  assert(wmbus_rx_health(0x04, 0, G::FIFO_EMPTY) == H::OK);
  assert(wmbus_rx_health(0x01, 0, G::FIFO_EMPTY) == H::NOT_RX);
  assert(wmbus_rx_health(0x05, G::IRQ1_TIMEOUT, G::FIFO_EMPTY) == H::RX_TIMEOUT);
  assert(wmbus_rx_health(0x05, 0, G::FIFO_OVERRUN) == H::FIFO_OVERRUN);
  std::puts("PASS: SX127x FIFO burst/tail gate, overrun and RX watchdog");
}
