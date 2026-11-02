#include "../../components/wmbus_native_sx126x/t1_packet_framing.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::t1;
static std::vector<uint8_t> encode(const std::vector<uint8_t> &bytes) {
  constexpr uint8_t sym[16]={0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
                              0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29};
  std::vector<uint8_t> out((bytes.size()*12+7)/8,0);
  size_t offset=0;
  for(auto byte:bytes) {
    for(auto nib: {static_cast<uint8_t>(byte>>4),static_cast<uint8_t>(byte&15)}) {
      for(int bit=5;bit>=0;--bit,++offset)
        out[offset/8]|=static_cast<uint8_t>(((sym[nib]>>bit)&1U)<<(7-offset%8));
    }
  }
  return out;
}
int main(){
  // L=0x0B: 12 data bytes (including L) + two CRC blocks (4 bytes).
  std::vector<uint8_t> dll(16,0x44);dll[0]=0x0B;
  auto coded=encode(dll);
  std::vector<uint8_t> output;
  size_t expected = 0;
  assert(expected_encoded_size(coded.data(), 1, expected)==FrameStatus::Incomplete);
  assert(expected == 0);
  assert(expected_encoded_size(coded.data(), 2, expected)==FrameStatus::Ok);
  assert(expected == coded.size());
  assert(expected_encoded_size(coded.data(), coded.size(), expected)==FrameStatus::Ok);
  assert(expected == coded.size());
  assert(frame_from_post_sync(coded.data(),coded.size(),output)==FrameStatus::Ok);
  assert(output==dll);
  coded.push_back(0xA5); // trailing radio bytes must never propagate
  assert(frame_from_post_sync(coded.data(),coded.size(),output)==FrameStatus::Ok);
  assert(output==dll);
  assert(frame_from_post_sync(coded.data(),2,output)==FrameStatus::Incomplete);
  assert(output.empty());
  coded[0]=0; // invalid initial 3-of-6 symbol
  expected = 123;
  assert(expected_encoded_size(coded.data(), coded.size(), expected)==FrameStatus::BadSymbol);
  assert(expected == 0);
  assert(frame_from_post_sync(coded.data(),coded.size(),output)==FrameStatus::BadSymbol);
  assert(output.empty());
  // Check every possible L-field, including block CRC boundaries and
  // the maximum 255-byte decoded DLL guard.
  for (unsigned l = 0; l <= 255; ++l) {
    const size_t remaining = l >= 9 ? l - 9 : 0;
    const size_t crc_blocks = 1 + (remaining + 15) / 16;
    const size_t decoded_length = l + 1 + 2 * crc_blocks;
    std::vector<uint8_t> trial(decoded_length, 0x44);
    trial[0] = static_cast<uint8_t>(l);
    const auto encoded = encode(trial);
    size_t needed = 999;
    const auto result = expected_encoded_size(encoded.data(), encoded.size(), needed);
    if (l < 10) {
      assert(result == FrameStatus::HeaderInvalid);
      assert(needed == 0);
    } else if (decoded_length > 255) {
      assert(result == FrameStatus::TooLong);
      assert(needed == 0);
    } else {
      assert(result == FrameStatus::Ok);
      assert(needed == encoded.size());
      std::vector<uint8_t> decoded;
      assert(frame_from_post_sync(encoded.data(), encoded.size(), decoded) == FrameStatus::Ok);
      assert(decoded == trial);
      assert(frame_from_post_sync(encoded.data(), encoded.size() - 1, decoded) == FrameStatus::Incomplete);
      assert(decoded.empty());
    }
  }
  dll[0]=0x08;
  coded=encode(dll);
  assert(frame_from_post_sync(coded.data(),coded.size(),output)==FrameStatus::HeaderInvalid);
  return 0;
}
