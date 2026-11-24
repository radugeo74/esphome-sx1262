#include "../../components/wmbus_radio/phy_crc.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
using esphome::wmbus_radio::phy::crc16;
using esphome::wmbus_radio::phy::strip_format_a;
using esphome::wmbus_radio::phy::strip_format_b;
static std::vector<uint8_t> hex(const std::string &s) {
  assert(s.size() % 2 == 0);
  std::vector<uint8_t> b;
  for (size_t i = 0; i < s.size(); i += 2)
    b.push_back(static_cast<uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return b;
}
int main() {
  // Authentic T1/A from Heltec log (80): four validated CRC blocks.
  auto raw = hex("324468506758252469806D91A0113E35060080170000A70673060000B2DD00000000000000000000000000000000FFFF00000000000000000670A1");
  const auto good = hex("32446850675825246980A0113E35060080170000A7067306000000000000000000000000000000000000000000000000000006");
  assert(strip_format_a(raw) && raw == good);
  // Every CRC block, and the final short block, must be validated.
  const auto original = hex("324468506758252469806D91A0113E35060080170000A70673060000B2DD00000000000000000000000000000000FFFF00000000000000000670A1");
  for (size_t offset : {size_t(0), size_t(12), size_t(30), size_t(48)}) {
    auto corrupted = original;
    corrupted[offset] ^= 0x01;
    assert(!strip_format_a(corrupted));
  }
  auto truncated = original;
  truncated.pop_back();
  assert(!strip_format_a(truncated));
  auto wrong_length = original;
  wrong_length[0] ^= 1;
  assert(!strip_format_a(wrong_length));
  // Format B, one CRC block and two CRC blocks.
  for (size_t n : {size_t(30), size_t(130)}) {
    std::vector<uint8_t> body(n, 0x23);
    const size_t split = n <= 126 ? n : size_t(126);
    std::vector<uint8_t> on_air(body.begin(), body.begin() + split);
    auto add_crc = [&on_air](const uint8_t *data, size_t len) {
      uint16_t crc = crc16(data, len);
      on_air.push_back(uint8_t(crc >> 8));
      on_air.push_back(uint8_t(crc));
    };
    add_crc(body.data(), split);
    if (split < n) {
      on_air.insert(on_air.end(), body.begin() + split, body.end());
      add_crc(body.data() + split, n - split);
    }
    on_air[0] = static_cast<uint8_t>(on_air.size() - 1);
    // L-byte is in the CRC-protected first block, recalculate CRC.
    uint16_t first = crc16(on_air.data(), split);
    on_air[split] = uint8_t(first >> 8);
    on_air[split + 1] = uint8_t(first);
    auto result = on_air;
    assert(strip_format_b(result));
    assert(result.size() == n);
    assert(result[0] == n-1);
    auto corrupt = on_air;
    corrupt[14] ^= 0x20;
    assert(!strip_format_b(corrupt));
  }
  std::cout << "PHY_CRC_INDEPENDENT_REAL_T1_AND_FORMAT_B_OK\n";
}
