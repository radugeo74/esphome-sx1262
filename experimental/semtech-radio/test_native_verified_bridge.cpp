#include "../../components/wmbus_native_sx126x/native_verified_frame.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x;
int main() {
  std::vector<uint8_t> dll={0x0C,0x44,0x68,0x50,0x01,0x00,0x00,0x31,
                            0x95,0x37,0x72,0x00,0x00};
  const uint16_t crc=wmbus_pure::crc16_en13757(dll.data(),dll.size()-2);
  dll[dll.size()-2]=static_cast<uint8_t>(crc>>8);
  dll.back()=static_cast<uint8_t>(crc);
  const auto ok=verified::process(boundary::Mode::C1B,dll.data(),dll.size());
  assert(ok.status==verified::Status::Ok);
  assert(ok.frame.header.meter_id==31000001u);
  assert(ok.frame.bytes.size()==11);
  dll[6]^=1;
  const auto bad=verified::process(boundary::Mode::C1B,dll.data(),dll.size());
  assert(bad.status==verified::Status::BadCrcOrHeader);
  assert(bad.frame.bytes.empty());
  const auto short_packet=verified::process(boundary::Mode::C1B,dll.data(),2);
  assert(short_packet.status==verified::Status::BadBoundary);
  assert(short_packet.frame.bytes.empty());
  return 0;
}
