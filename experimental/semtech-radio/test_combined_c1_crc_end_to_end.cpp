// Synthetic end-to-end C1-A/C1-B verification, including B's second CRC.
// No real meter IDs, telegrams, or secrets. Format failures must fail closed.
#include "../../components/wmbus_native_sx126x/native_verified_frame.h"
#include "../../components/wmbus_native_sx126x/native_publication_gate.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace native = esphome::wmbus_native_sx126x;
static void crc_append(std::vector<uint8_t> &bytes, size_t offset, size_t length) {
  const uint16_t crc = wmbus_pure::crc16_en13757(bytes.data() + offset, length);
  bytes.push_back(static_cast<uint8_t>(crc >> 8));
  bytes.push_back(static_cast<uint8_t>(crc & 0xff));
}
static std::vector<uint8_t> header(uint8_t length) {
  return {length, 0x44, 0x68, 0x50, 0x01, 0x00, 0x00, 0x31, 0x95, 0x37, 0x72};
}
int main() {
  // C1-A length L=54, DLL=55 data bytes in 10+16+16+13 blocks, CRC after each.
  std::vector<uint8_t> a = header(54);
  a.pop_back(); // first 10 bytes are first CRC-covered block
  crc_append(a, 0, 10);
  for (size_t block = 0; block < 3; ++block) {
    const size_t count = block == 2 ? 13 : 16;
    const size_t start = a.size();
    for (size_t i = 0; i < count; ++i) a.push_back(static_cast<uint8_t>(block == 0 && i == 0 ? 0x72 : i + block));
    crc_append(a, start, count);
  }
  assert(a.size() == 63);
  const auto a_ok = native::verified::process(native::boundary::Mode::C1A, a.data(), a.size());
  assert(a_ok.status == native::verified::Status::Ok);
  assert(a_ok.frame.bytes.size() == 55);
  assert(a_ok.frame.header.meter_id == 31000001u);
  // CRC-valid C1-A is not enough for publication: synthetic CI=72 has
  // incomplete/unverified transport security and must fail closed.
  assert(wmbus_pure::publication::inspect(a_ok.frame.bytes.data(),
      a_ok.frame.bytes.size(),31000001u)!=
      wmbus_pure::publication::Decision::Allowed);
  a[40] ^= 0x01;
  const auto a_bad = native::verified::process(native::boundary::Mode::C1A, a.data(), a.size());
  assert(a_bad.status != native::verified::Status::Ok && a_bad.frame.bytes.empty());

  // C1-B L=140 includes the two CRCs: 126 data + 2 CRC + 11 data + 2 CRC.
  std::vector<uint8_t> b = header(140);
  while (b.size() < 126) b.push_back(0x00);
  crc_append(b, 0, 126);
  const size_t second_start = b.size();
  for (size_t i = 0; i < 11; ++i) b.push_back(static_cast<uint8_t>(i));
  crc_append(b, second_start, 11);
  assert(b.size() == 141);
  const auto b_ok = native::verified::process(native::boundary::Mode::C1B, b.data(), b.size());
  assert(b_ok.status == native::verified::Status::Ok);
  assert(b_ok.frame.bytes.size() == 137);
  assert(b_ok.frame.header.meter_id == 31000001u);
  // Same rule applies to CRC-verified C1 format B.
  assert(wmbus_pure::publication::inspect(b_ok.frame.bytes.data(),
      b_ok.frame.bytes.size(),31000001u)!=
      wmbus_pure::publication::Decision::Allowed);
  b[second_start + 5] ^= 0x01;
  const auto b_bad = native::verified::process(native::boundary::Mode::C1B, b.data(), b.size());
  assert(b_bad.status != native::verified::Status::Ok && b_bad.frame.bytes.empty());
  const auto b_short = native::verified::process(native::boundary::Mode::C1B, b.data(), b.size() - 1);
  assert(b_short.status != native::verified::Status::Ok && b_short.frame.bytes.empty());
  return 0;
}
