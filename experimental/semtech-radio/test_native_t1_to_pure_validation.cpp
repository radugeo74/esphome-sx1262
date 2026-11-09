// Native T1 3-of-6 -> complete pure format B DLL CRC -> manufacturer/id.
// This verifies the conversion contract, NOT over-the-air radio sync/IRQ.
#include "../../components/wmbus_native_sx126x/native_frame_boundary.h"
#include "../xmq/legacy_reference/pure_complete_frame.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::boundary;
using namespace wmbus_pure;

static std::vector<uint8_t> encode_t1(const std::vector<uint8_t>& bytes) {
  constexpr uint8_t symbols[] = {
    0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
    0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29
  };
  std::vector<uint8_t> coded((bytes.size()*12+7)/8, 0);
  size_t bit_index=0;
  for(uint8_t value: bytes) {
    for(uint8_t nibble: {static_cast<uint8_t>(value>>4), static_cast<uint8_t>(value&15)}) {
      const uint8_t sym=symbols[nibble];
      for(int bit=5; bit>=0; --bit, ++bit_index)
        coded[bit_index/8] |= static_cast<uint8_t>(((sym>>bit)&1U)<<(7-(bit_index%8)));
    }
  }
  return coded;
}
int main() {
  std::vector<uint8_t> dll={
    0x0C,0x44,0x68,0x50,0x01,0x00,0x00,0x31,
    0x95,0x37,0x72,0x00,0x00
  };
  const auto crc=crc16_en13757(dll.data(),dll.size()-2);
  dll[dll.size()-2]=static_cast<uint8_t>(crc>>8);
  dll[dll.size()-1]=static_cast<uint8_t>(crc);
  auto coded=encode_t1(dll);
  auto frame=convert(Mode::T1,coded.data(),coded.size(),dll.size());
  assert(frame.status==Status::Framed);
  assert(frame.decoded_dll_with_crc==dll);
  CompleteFrame verified;
  assert(verify_complete_frame(frame.decoded_dll_with_crc.data(),
         frame.decoded_dll_with_crc.size(),CrcFrameFormat::B,verified)==CompleteStatus::Ok);
  assert(verified.header.meter_id==31000001u);
  coded[0]=0;
  auto malformed=convert(Mode::T1,coded.data(),coded.size(),dll.size());
  assert(malformed.status==Status::InvalidSymbol);
  assert(malformed.decoded_dll_with_crc.empty());
  coded=encode_t1(dll);
  coded[7]^=1;
  auto damaged=convert(Mode::T1,coded.data(),coded.size(),dll.size());
  if(damaged.status==Status::Framed) {
    assert(verify_complete_frame(damaged.decoded_dll_with_crc.data(),
           damaged.decoded_dll_with_crc.size(),CrcFrameFormat::B,verified)!=CompleteStatus::Ok);
    assert(verified.bytes.empty());
  }
  assert(convert(Mode::T1,coded.data(),coded.size()-1,dll.size()).status==Status::Incomplete);
  return 0;
}
