// Bounded standalone regression of the actual native-path 3-of-6 decoder.
// It detects the final-symbol out-of-bounds read seen for 3-byte buffers.
#include "components/wmbus_radio/decode3of6.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
constexpr std::array<uint8_t, 16> SYMBOLS = {
    0x16, 0x0d, 0x0e, 0x0b, 0x1c, 0x19, 0x1a, 0x13,
    0x2c, 0x25, 0x26, 0x23, 0x34, 0x31, 0x32, 0x29,
};
std::vector<uint8_t> encode(const std::vector<uint8_t> &plain) {
  const size_t bits = plain.size() * 12u;
  std::vector<uint8_t> out((bits + 7u) / 8u, 0);
  size_t offset = 0;
  for (uint8_t byte : plain) {
    for (int shift : {4, 0}) {
      const uint8_t symbol = SYMBOLS[(byte >> shift) & 15u];
      for (int bit = 5; bit >= 0; --bit) {
        const size_t absolute = offset++;
        out[absolute / 8u] |= static_cast<uint8_t>(
            ((symbol >> bit) & 1u) << (7u - absolute % 8u));
      }
    }
  }
  return out;
}
}

int main() {
  using esphome::wmbus_radio::decode3of6;
  using esphome::wmbus_radio::encoded_size;

  for (size_t size = 1; size <= 255; ++size) {
    std::vector<uint8_t> original(size);
    for (size_t j = 0; j < size; ++j)
      original[j] = static_cast<uint8_t>((j * 73u + size * 11u) & 0xFFu);
    auto radio = encode(original);
    assert(radio.size() == encoded_size(original.size()));
    const auto got = decode3of6(radio);
    assert(got.has_value());
    assert(*got == original);
  }

  // An invalid first symbol may never produce plausible meter data.
  auto invalid = encode({0x2e, 0x44, 0x68, 0x50});
  invalid[0] = 0; // starts with invalid 3-of-6 code 0b000000
  assert(!decode3of6(invalid).has_value());

  // Short exact-byte T1 examples exercise the boundary without a guard byte.
  for (const auto &plain : std::vector<std::vector<uint8_t>>{
           {0x12}, {0x12, 0x34}, {0x12, 0x34, 0x56},
           {0x2e, 0x44, 0x68, 0x50}}) {
    auto bytes = encode(plain);
    assert(*decode3of6(bytes) == plain);
  }
  return 0;
}
