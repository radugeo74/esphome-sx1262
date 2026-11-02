#include "../../components/wmbus_native_sx126x/t1_fifo_contract.h"
#include <cassert>
#include <vector>
using namespace esphome::wmbus_native_sx126x::t1;
int main() {
  FifoContract rx;
  const uint8_t bad[] = {0,0,0};
  assert(rx.state() == RxState::Idle);
  assert(rx.feed_bytes(bad, sizeof(bad)) == RxState::Idle);
  rx.begin_frame();
  assert(rx.feed_bytes(nullptr,0) == RxState::Collecting);
  assert(rx.end_frame() == RxState::Rejected); // short frame never accepted
  assert(rx.feed_bytes(bad,sizeof(bad)) == RxState::Rejected);
  rx.begin_frame();
  assert(rx.state() == RxState::Collecting);
  assert(rx.feed_bytes(bad,sizeof(bad)) == RxState::Rejected);
  assert(rx.end_frame() == RxState::Rejected);
  rx.begin_frame();
  assert(rx.feed_bytes(nullptr,1) == RxState::Rejected);
  // Full valid T1 frame through the non-owning FIFO contract.
  constexpr uint8_t symbols[16] = {0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
                                   0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29};
  std::vector<uint8_t> dll = {0x0B,0x44,0x68,0x50,0x01,0x00,0x00,0x31,0x95,0x37,0x72,0x00};
  const auto crc1 = wmbus_pure::crc16_en13757(dll.data(),10);
  const auto crc2 = wmbus_pure::crc16_en13757(dll.data()+10,2);
  std::vector<uint8_t> raw(dll.begin(),dll.begin()+10);
  raw.push_back(static_cast<uint8_t>(crc1>>8));
  raw.push_back(static_cast<uint8_t>(crc1));
  raw.insert(raw.end(),dll.begin()+10,dll.end());
  raw.push_back(static_cast<uint8_t>(crc2>>8));
  raw.push_back(static_cast<uint8_t>(crc2));
  std::vector<uint8_t> encoded(raw.size()*12/8,0);
  size_t bit_offset=0;
  for(uint8_t byte:raw)
    for(uint8_t nibble:{static_cast<uint8_t>(byte>>4),static_cast<uint8_t>(byte&15)})
      for(int bit=5;bit>=0;--bit,++bit_offset)
        encoded[bit_offset/8] |= static_cast<uint8_t>(((symbols[nibble]>>bit)&1u)<<(7-bit_offset%8));
  rx.begin_frame();
  for(size_t i=0;i<encoded.size();++i)
    assert(rx.feed_bytes(&encoded[i],1)==(i+1==encoded.size()?RxState::Verified:RxState::Collecting));
  assert(rx.end_frame()==RxState::Verified);
  assert(rx.result().frame.header.meter_id==31000001u);
  assert(rx.feed_bytes(encoded.data(),1)==RxState::Verified);
  rx.begin_frame();
  encoded[7] ^= 1;
  assert(rx.feed_bytes(encoded.data(),encoded.size())==RxState::Rejected);
  assert(rx.end_frame()==RxState::Rejected);
  return 0;
}
