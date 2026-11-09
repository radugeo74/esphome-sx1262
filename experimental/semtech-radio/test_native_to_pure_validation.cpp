// Validate the native radio boundary against the existing pure DLL verifier.
// This is a host-side integration test; no publishing path is enabled.
#include "../../components/wmbus_native_sx126x/native_frame_boundary.h"
#include "../xmq/legacy_reference/pure_complete_frame.h"
#include <cassert>
#include <cstdint>
#include <vector>

using namespace esphome::wmbus_native_sx126x::boundary;
using namespace wmbus_pure;

int main() {
  // Proper manufacturer, BCD meter id and CI header, plus computed format B CRC.
  std::vector<uint8_t> dll = {
      0x0C, 0x44, 0x68, 0x50, 0x01, 0x00, 0x00, 0x31,
      0x95, 0x37, 0x72, 0x00, 0x00
  };
  const uint16_t crc = crc16_en13757(dll.data(), dll.size()-2);
  dll[dll.size()-2] = static_cast<uint8_t>(crc >> 8);
  dll[dll.size()-1] = static_cast<uint8_t>(crc);
  const auto framed = convert(Mode::C1B, dll.data(), dll.size());
  assert(framed.status == Status::Framed);
  CompleteFrame verified;
  assert(verify_complete_frame(framed.decoded_dll_with_crc.data(),
      framed.decoded_dll_with_crc.size(), CrcFrameFormat::B, verified)
      == CompleteStatus::Ok);
  assert(verified.header.meter_id == 31000001u);
  assert(verified.bytes.size() == 11);

  // Corruption must never be promoted from framed to verified.
  dll[5] ^= 0x01;
  const auto corrupted = convert(Mode::C1B, dll.data(), dll.size());
  assert(corrupted.status == Status::Framed);
  assert(verify_complete_frame(corrupted.decoded_dll_with_crc.data(),
      corrupted.decoded_dll_with_crc.size(), CrcFrameFormat::B, verified)
      == CompleteStatus::BadCrc);
  assert(verified.bytes.empty());

  // Format B source length must be exact at the pure verifier boundary,
  // even if the native SX126x callback contains a padded RX buffer.
  dll[5] ^= 0x01;
  dll.push_back(0x00);
  const auto padded = convert(Mode::C1B, dll.data(), dll.size());
  assert(padded.status == Status::Framed);
  assert(padded.decoded_dll_with_crc.size() == dll.size()-1);
  return 0;
}
