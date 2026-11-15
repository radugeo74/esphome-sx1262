// Host-only regression: g++ -std=c++17 -Wall -Wextra -Werror -pedantic
//   -Icomponents/sx127x experimental/sx127x/test_t1_recovery_diag.cpp -o /tmp/t1_diag_test
//   && /tmp/t1_diag_test
#include <cassert>
#include <cstdint>
#include <vector>
#include "wmbus_t1_recovery_diag.h"

using namespace esphome::sx127x;

static std::vector<uint8_t> hex_decode(const char *hex) {
  std::vector<uint8_t> out;
  while (*hex != '\0' && hex[1] != '\0') {
    const auto nibble = [](char c) -> uint8_t {
      return static_cast<uint8_t>(c >= '0' && c <= '9' ? c - '0' :
                                  c >= 'A' && c <= 'F' ? c - 'A' + 10 : c - 'a' + 10);
    };
    out.push_back(static_cast<uint8_t>((nibble(hex[0]) << 4U) | nibble(hex[1])));
    hex += 2;
  }
  return out;
}

int main() {
  const uint8_t a[] = {0x0F, 0x44, 0xAE, 0x0C, 0x78, 0x56, 0x34, 0x12, 0x01, 0x07};
  const uint8_t b[] = {0x78, 0x0B, 0x13, 0x43, 0x65, 0x87};
  assert(t1_diag_crc16_en13757(a, sizeof(a)) == 0x4447U);
  assert(t1_diag_crc16_en13757(b, sizeof(b)) == 0x1E6DU);
  const auto keep_running = [](unsigned) { return true; };
  // Two real rejected T1 RAW frames, known to require a single invalid
  // 3-of-6 symbol replacement. Identifiers are intentionally not printed.
  auto raw1 = hex_decode(
      "3A971C6AC6563992EC70D38E9594CED23E9398E59A2F22D998E596A5635A58B59659659658B59658D58D2CD2F159658E58E58D58B59659658D58D58D58D58D59658E58B58E8CB8D958E58B58E58E58B58ECAC95A");
  const auto r1 = t1_diag_try_recover(raw1.data(), raw1.size(), keep_running, 5, 1024);
  assert(r1.status == T1RecoveryStatus::CRC_VALID);
  assert(r1.bad_symbols == 1 && r1.crc_valid_candidates == 1);
  assert(r1.combinations == 2 && r1.attempts == 2);
  assert(r1.coherent_candidates == 1);
  assert(r1.unique_meter_id == 0x22413825U);
  auto raw2 = hex_decode(
      "2CE71C6AC65658E66539939C6A5B1666CD1A99634D6F22D93B134DA5635A966596D265A539A5B42CB59635CCB458E59659659659659659659659659659659664DB26B234E59A92D6959B0B95C9538CB966723596596C5AD32B");
  const auto r2 = t1_diag_try_recover(raw2.data(), raw2.size(), keep_running, 5, 1024);
  assert(r2.status == T1RecoveryStatus::CRC_VALID);
  assert(r2.bad_symbols == 1 && r2.crc_valid_candidates == 1);
  assert(r2.combinations == 4 && r2.attempts == 4);
  assert(r2.coherent_candidates == 1);
  assert(r2.unique_meter_id == 0x24255902U);
  auto raw5 = hex_decode(
      "3A971C6EC65665358B95338E9590CEB25B1A98E59A2F22D991C5ACA5635A70D59659659635C35324B5B25A94E634D36636135A36335336635335934B35A35C35934E35C34DA4E4EC35334B35C36C5B134E596966");
  const auto r5 = t1_diag_try_recover(raw5.data(), raw5.size(), keep_running, 5, 1024);
  assert(r5.status == T1RecoveryStatus::CRC_VALID);
  assert(r5.bad_symbols == 5 && r5.combinations == 768);
  assert(r5.crc_valid_candidates == 1 && r5.coherent_candidates == 1);
  // Strict header validation: malformed C/manufacturer/BCD address rejected.
  uint8_t header[] = {0x2F, 0x44, 0x68, 0x50, 0x25, 0x38, 0x41, 0x22, 0x95, 0x72, 0, 0};
  assert(t1_diag_coherent_meter_id(header, sizeof(header)) == 0x22413825U);
  header[4] = 0xFA;
  assert(t1_diag_coherent_meter_id(header, sizeof(header)) == 0U);
  header[4] = 0x25;
  header[1] = 0x00;
  assert(t1_diag_coherent_meter_id(header, sizeof(header)) == 0U);
  auto raw16 = hex_decode(
      "3A971C6AC67668B2EC70D38E9594CEC65A6598E59A2F22D98D6599A5635A71659651659634B3593533592F1D1C35C35936938E34B3593533535A95A934E34E5A35929356C9C96C2CD2D6D16B16C8D2CD5ACD269A");
  const auto too_many = t1_diag_try_recover(raw16.data(), raw16.size(), keep_running, 5, 1024);
  assert(too_many.status == T1RecoveryStatus::TOO_MANY_ERRORS);
  const auto stopped = t1_diag_try_recover(raw2.data(), raw2.size(),
                                            [](unsigned) { return false; }, 5, 1024);
  assert(stopped.status == T1RecoveryStatus::TIME_BUDGET);
  const auto incomplete = t1_diag_try_recover(raw2.data(), 10, keep_running, 5, 1024);
  assert(incomplete.status == T1RecoveryStatus::INCOMPLETE);
  return 0;
}
