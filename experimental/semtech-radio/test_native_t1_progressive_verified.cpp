#include "../../components/wmbus_native_sx126x/t1_progressive_verified.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::t1;
int main() {
  ProgressiveVerified receiver;
  const uint8_t invalid_symbol[] = {0, 0, 0};
  assert(receiver.feed(invalid_symbol, sizeof(invalid_symbol)) == VerifiedFeedStatus::Rejected);
  assert(receiver.result().status != esphome::wmbus_native_sx126x::verified::Status::Ok);
  receiver.reset();
  assert(receiver.feed(nullptr, 0) == VerifiedFeedStatus::NeedMore);
  // 3-of-6 encoded first byte L=11: 0x16 0x23, packed over 12 bits.
  // A full syntactically shaped but CRC-invalid packet may never be VERIFIED.
  constexpr uint8_t symbols[16] = {0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
                                   0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29};
  std::vector<uint8_t> decoded(16, 0x44); decoded[0] = 11;
  std::vector<uint8_t> bits((decoded.size()*12+7)/8,0);
  size_t offset=0;
  for (uint8_t byte: decoded) {
    for (uint8_t nibble: {static_cast<uint8_t>(byte>>4),static_cast<uint8_t>(byte&15)}) {
      for(int bit=5;bit>=0;--bit,++offset)
        bits[offset/8] |= static_cast<uint8_t>(((symbols[nibble]>>bit)&1U)<<(7-offset%8));
    }
  }
  for (size_t i=0;i<bits.size();++i) {
    const auto result=receiver.feed(&bits[i],1);
    assert(result == (i+1 == bits.size() ? VerifiedFeedStatus::Rejected : VerifiedFeedStatus::NeedMore));
  }
  assert(receiver.result().status != esphome::wmbus_native_sx126x::verified::Status::Ok);
  // Positive T1 Format A: valid 10-byte initial block + CRC, then
  // 2-byte second block + CRC. Encode with 3-of-6 to exercise the full bridge.
  std::vector<uint8_t> valid_header = {
    0x0B, 0x44, 0x68, 0x50, 0x01, 0x00, 0x00, 0x31, 0x95, 0x37,
    0x72, 0x00
  };
  const auto crc_first = wmbus_pure::crc16_en13757(valid_header.data(), 10);
  const auto crc_second = wmbus_pure::crc16_en13757(valid_header.data() + 10, 2);
  std::vector<uint8_t> with_crc(valid_header.begin(), valid_header.begin() + 10);
  with_crc.push_back(static_cast<uint8_t>(crc_first >> 8));
  with_crc.push_back(static_cast<uint8_t>(crc_first));
  with_crc.insert(with_crc.end(), valid_header.begin() + 10, valid_header.end());
  with_crc.push_back(static_cast<uint8_t>(crc_second >> 8));
  with_crc.push_back(static_cast<uint8_t>(crc_second));
  assert(with_crc.size() == 16);
  std::vector<uint8_t> encoded_valid((with_crc.size()*12+7)/8, 0);
  offset = 0;
  for (uint8_t byte: with_crc) {
    for (uint8_t nibble: {static_cast<uint8_t>(byte>>4),static_cast<uint8_t>(byte&15)}) {
      for(int bit=5;bit>=0;--bit,++offset)
        encoded_valid[offset/8] |= static_cast<uint8_t>(((symbols[nibble]>>bit)&1U)<<(7-offset%8));
    }
  }
  receiver.reset();
  for (size_t i=0;i<encoded_valid.size();++i) {
    const auto result=receiver.feed(&encoded_valid[i],1);
    assert(result == (i+1 == encoded_valid.size() ? VerifiedFeedStatus::Verified : VerifiedFeedStatus::NeedMore));
  }
  assert(receiver.result().status == esphome::wmbus_native_sx126x::verified::Status::Ok);
  assert(receiver.result().frame.header.meter_id == 31000001u);
  assert(receiver.result().frame.bytes == valid_header);
  assert(receiver.feed(encoded_valid.data(), 1) == VerifiedFeedStatus::Rejected);
  receiver.reset();
  assert(receiver.feed(encoded_valid.data(), encoded_valid.size()) == VerifiedFeedStatus::Verified);
  // Same symbols but damaged payload: complete packet must fail CRC.
  encoded_valid[5] ^= 1;
  receiver.reset();
  assert(receiver.feed(encoded_valid.data(), encoded_valid.size()) == VerifiedFeedStatus::Rejected);
  assert(receiver.result().status != esphome::wmbus_native_sx126x::verified::Status::Ok);
  assert(receiver.feed(encoded_valid.data(), 1) == VerifiedFeedStatus::Rejected);
  return 0;
}
