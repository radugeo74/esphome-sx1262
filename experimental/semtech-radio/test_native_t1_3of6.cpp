#include "../../components/wmbus_native_sx126x/t1_3of6.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::t1;
static std::vector<uint8_t> encode(const std::vector<uint8_t>& decoded) {
  const uint8_t symbols[] = {
      0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
      0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29
  };
  std::vector<uint8_t> bits;
  for (auto b : decoded) {
    for (auto nib : {static_cast<uint8_t>(b >> 4), static_cast<uint8_t>(b & 15)}) {
      const auto s = symbols[nib];
      for (int i=5;i>=0;--i) bits.push_back((s >> i) & 1U);
    }
  }
  std::vector<uint8_t> coded((bits.size()+7)/8, 0);
  for (size_t i=0;i<bits.size();++i) coded[i/8] |= bits[i] << (7-(i%8));
  return coded;
}
int main() {
  // Synthetic T1 data used only to exercise reversible 3-of-6 coding.
  const std::vector<uint8_t> plain = {
    0x2F,0x44,0x68,0x50,0x01,0x00,0x00,0x31,0x95,0x62,0xA2,
    0x06,0x3E,0x35,0x7C,0x05,0x10,0x16,0x00,0x00,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,0x05,0x05,0x0B,0x05,
    0x10,0x12,0x0A,0x07,0x11,0x15,0x12,0x0F,0x14,
    0x14,0x12,0x14,0x12,0x10,0x13,0x0F,0x10
  };
  auto coded = encode(plain);
  std::vector<uint8_t> output;
  assert(decode(coded.data(), coded.size(), plain.size(), output) == DecodeStatus::Ok);
  assert(output == plain);
  assert(decode(coded.data(), coded.size()-1, plain.size(), output) == DecodeStatus::Truncated);
  assert(output.empty());
  coded[0] = 0;
  assert(decode(coded.data(), coded.size(), plain.size(), output) == DecodeStatus::BadSymbol);
  assert(output.empty());
  assert(decode(nullptr,0,0,output) == DecodeStatus::Empty);
  return 0;
}
