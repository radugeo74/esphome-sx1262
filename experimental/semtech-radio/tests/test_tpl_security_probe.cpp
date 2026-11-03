#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../../../components/wmbus_native_sx126x/tpl_security_probe.h"

using wmbus_pure::tpl_security::State;
using wmbus_pure::tpl_security::inspect;

int main() {
  // Valid DLL fields: L, C, M=TECHEM, meter ID BCD, version, medium.
  uint8_t short_frame[] = {
      0x10, 0x44, 0x68, 0x50, 0x29, 0x37, 0x41, 0x22, 0x95, 0x62,
      0x7A, 0x01, 0x00, 0x20, 0x05, 0x2F, 0x2F};
  auto r = inspect(short_frame, sizeof(short_frame));
  assert(r.has_config_word && r.mode == 5 && r.state == State::Encrypted);
  short_frame[14] = 0x04;
  r = inspect(short_frame, sizeof(short_frame));
  assert(r.has_config_word && r.mode == 4 && r.state == State::Encrypted);
  short_frame[14] = 0x00;
  r = inspect(short_frame, sizeof(short_frame));
  assert(r.has_config_word && r.mode == 0 && r.state == State::NotEncrypted);
  short_frame[14] = 0x0F;
  r = inspect(short_frame, sizeof(short_frame));
  assert(r.has_config_word && r.mode == 15 && r.state == State::Unknown);
  short_frame[0] = 0x11;  // invalid L-field
  assert(inspect(short_frame, sizeof(short_frame)).state == State::Unknown);
  short_frame[0] = 0x10;
  assert(inspect(short_frame, 14).state == State::Unknown);  // truncated

  uint8_t long_frame[] = {
      0x18, 0x44, 0x68, 0x50, 0x29, 0x37, 0x41, 0x22, 0x95, 0x62,
      0x72, 0x29, 0x37, 0x41, 0x22, 0x68, 0x50, 0x95, 0x62,
      0x01, 0x00, 0x20, 0x05, 0x2F, 0x2F};
  r = inspect(long_frame, sizeof(long_frame));
  assert(r.has_config_word && r.mode == 5 && r.state == State::Encrypted);

  short_frame[10] = 0xA2;  // manufacturer specific: mode must not be guessed
  r = inspect(short_frame, sizeof(short_frame));
  assert(!r.has_config_word && r.state == State::Unknown);
  short_frame[10] = 0xA0;
  r = inspect(short_frame, sizeof(short_frame));
  assert(!r.has_config_word && r.state == State::Unknown);
  std::puts("PASS conservative TPL encryption classification");
}
