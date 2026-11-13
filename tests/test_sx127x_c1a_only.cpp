// Isolated SX1276 C1/A PHY test: hardware consumed 0x54 CD before FIFO.
#include "components/sx127x/wmbus_t1_length.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
using namespace esphome::sx127x;

int main() {
  // 0x36 = 54-byte C1/A L, emitted by real C1/A meters in earlier captures.
  const std::array<uint8_t, 3> prefix{{0x36, 0x44, 0x68}};
  auto candidates = wmbus_raw_candidates(0xCD, true, prefix.data(), prefix.size(), 384);
  assert(candidates.t1 == 0);        // Hardware sync=54CD excludes T1 / C1-B.
  assert(candidates.c1 == 63);      // C1/A raw bytes after 54 CD, incl DLL CRC.
  assert(candidates.c1 + 2 == 65);  // Shared decoder needs full 54 CD + raw body.

  std::array<uint8_t, 384> fifo_frame{};
  fifo_frame[0] = 0x54;
  fifo_frame[1] = 0xCD;
  for (size_t i = 0; i < prefix.size(); ++i)
    fifo_frame[2 + i] = prefix[i];
  assert(fifo_frame[0] == 0x54 && fifo_frame[1] == 0xCD);
  assert(fifo_frame[2] == 0x36);  // FIFO starts at L, not CD.
  // Invalid L must never be queued or checked as a real C1 frame.
  std::array<uint8_t, 3> invalid{{0x09, 0x44, 0x68}};
  auto rejected = wmbus_raw_candidates(0xCD, true, invalid.data(), invalid.size(), 384);
  assert(rejected.t1 == 0 && rejected.c1 == 0);
  std::puts("PASS C1/A-only: 54CD hardware sync, FIFO begins L, CRC body rebuilt");
}
