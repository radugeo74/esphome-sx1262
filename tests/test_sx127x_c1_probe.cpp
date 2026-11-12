// SX1276 pre-CRC receive diagnostics: first FIFO byte after hardware 0x54.
#include "components/sx127x/wmbus_t1_length.h"
#include "components/sx127x/wmbus_fifo_guard.h"
#include <array>
#include <cassert>
#include <cstdio>

using esphome::sx127x::WmbusSync54Marker;
using esphome::sx127x::classify_wmbus_sync54_marker;
using esphome::sx127x::wmbus_raw_candidates;
using esphome::sx127x::WmbusFifoGuard;

static_assert(classify_wmbus_sync54_marker(0x3D) == WmbusSync54Marker::T1_OR_C1B);
static_assert(classify_wmbus_sync54_marker(0xCD) == WmbusSync54Marker::C1A);
static_assert(classify_wmbus_sync54_marker(0x54) == WmbusSync54Marker::OTHER);
static_assert(classify_wmbus_sync54_marker(0x00) == WmbusSync54Marker::OTHER);
static_assert(WmbusFifoGuard::IRQ1_REGISTER == 0x3E);
static_assert(WmbusFifoGuard::IRQ1_PREAMBLE_DETECTED == 0x02);
static_assert(WmbusFifoGuard::IRQ1_SYNC_ADDRESS_MATCH == 0x01);

int main() {
  const std::array<uint8_t, 3> c1_header{{0x36, 0x44, 0x68}};
  const auto a = wmbus_raw_candidates(0xCD, true, c1_header.data(), c1_header.size(), 384);
  assert(a.t1 == 0 && a.c1 == 63); // C1/A L=0x36 plus CRC blocks.
  const auto b = wmbus_raw_candidates(0x3D, true, c1_header.data(), c1_header.size(), 384);
  assert(b.c1 == 55); // Possible C1/B (also may be T1).
  const auto off = wmbus_raw_candidates(0xCD, false, c1_header.data(), c1_header.size(), 384);
  assert(off.t1 == 0 && off.c1 == 0);
  const std::array<uint8_t, 3> invalid_l{{0x09, 0x44, 0x68}};
  const auto bad = wmbus_raw_candidates(0xCD, true, invalid_l.data(), invalid_l.size(), 384);
  assert(bad.t1 == 0 && bad.c1 == 0);
  const auto other = wmbus_raw_candidates(0x18, true, c1_header.data(), c1_header.size(), 384);
  assert(other.t1 == 0 && other.c1 == 0);
  std::puts("PASS SX1276: C1/A marker classification, L gate and IRQ1 flags");
}
