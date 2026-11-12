#include "components/sx127x/wmbus_t1_length.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
constexpr uint8_t SYMBOL[16] = {
    0b010110, 0b001101, 0b001110, 0b001011,
    0b011100, 0b011001, 0b011010, 0b010011,
    0b101100, 0b100101, 0b100110, 0b100011,
    0b110100, 0b110001, 0b110010, 0b101001,
};

std::array<uint8_t,3> t1_encode_prefix(uint8_t l, uint8_t c) {
  uint32_t bits = (static_cast<uint32_t>(SYMBOL[l>>4]) << 18) |
                  (static_cast<uint32_t>(SYMBOL[l&15]) << 12) |
                  (static_cast<uint32_t>(SYMBOL[c>>4]) << 6) |
                  SYMBOL[c&15];
  return {static_cast<uint8_t>(bits>>16), static_cast<uint8_t>(bits>>8),
          static_cast<uint8_t>(bits)};
}
} // namespace

int main() {
  for (uint8_t i=0; i<16; ++i)
    assert(esphome::sx127x::t1_symbol_to_nibble(SYMBOL[i]) == i);
  assert(esphome::sx127x::t1_symbol_to_nibble(0) == -1);
  const auto a = t1_encode_prefix(0x2F,0x44);
  const auto b = t1_encode_prefix(0x32,0x44);
  assert(esphome::sx127x::t1_expected_raw_bytes(a.data(),3,384) == 84);
  assert(esphome::sx127x::t1_expected_raw_bytes(b.data(),3,384) == 89);
  assert(esphome::sx127x::t1_expected_raw_bytes(a.data(),3,64) == 0);
  assert(esphome::sx127x::t1_expected_raw_bytes(a.data(),2,384) == 0);
  const auto short_frame = t1_encode_prefix(0x09,0x44);
  assert(esphome::sx127x::t1_expected_raw_bytes(short_frame.data(),3,384) == 0);
  const std::array<uint8_t,3> invalid{{0,0,0}};
  assert(esphome::sx127x::t1_expected_raw_bytes(invalid.data(),3,384) == 0);
  const auto too_long = t1_encode_prefix(0xFF,0x44);
  assert(esphome::sx127x::t1_expected_raw_bytes(too_long.data(),3,384) == 0);
  // C1/A carries CRC bytes after the L-length, C1/B counts CRC in L.
  assert(esphome::sx127x::c1_expected_raw_bytes(0x36, false, 382) == 63);
  assert(esphome::sx127x::c1_expected_raw_bytes(0x36, true, 382) == 55);
  assert(esphome::sx127x::c1_expected_raw_bytes(0x2E, false, 382) == 55);
  assert(esphome::sx127x::c1_expected_raw_bytes(0x2E, true, 382) == 47);
  assert(esphome::sx127x::c1_expected_raw_bytes(9, false, 382) == 0);
  assert(esphome::sx127x::c1_expected_raw_bytes(0x36, false, 62) == 0);
  assert(esphome::sx127x::c1_expected_raw_bytes(0x36, true, 54) == 0);

  // The same 54 3D can introduce T1 3-of-6 or a C1/B clear payload.
  const auto t1_only = esphome::sx127x::wmbus_raw_candidates(
      0x3D, false, a.data(), a.size(), 384);
  assert(t1_only.t1 == 84 && t1_only.c1 == 0);
  const auto ambiguous = esphome::sx127x::wmbus_raw_candidates(
      0x3D, true, a.data(), a.size(), 384);
  assert(ambiguous.t1 == 84 && ambiguous.c1 != 0);
  const std::array<uint8_t, 3> c1_prefix{{0x36, 0x44, 0x68}};
  const auto cb = esphome::sx127x::wmbus_raw_candidates(
      0x3D, true, c1_prefix.data(), 3, 384);
  assert(cb.c1 == 55);
  const auto ca = esphome::sx127x::wmbus_raw_candidates(
      0xCD, true, c1_prefix.data(), 3, 384);
  assert(ca.c1 == 63 && ca.t1 == 0);
  const auto no_c1 = esphome::sx127x::wmbus_raw_candidates(
      0xCD, false, c1_prefix.data(), 3, 384);
  assert(no_c1.c1 == 0 && no_c1.t1 == 0);
  const auto unsupported = esphome::sx127x::wmbus_raw_candidates(
      0x12, true, c1_prefix.data(), 3, 384);
  assert(unsupported.c1 == 0 && unsupported.t1 == 0);

  std::puts("PASS SX127x T1/C1-A/C1-B frame sizes and ambiguous 54 3D");
}
