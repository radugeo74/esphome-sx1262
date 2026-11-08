#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "../../../components/wmbus_native_sx126x/tpl_security_probe.h"

using wmbus_pure::tpl_security::State;
using wmbus_pure::tpl_security::inspect;

int main() {
  // Synthetic, valid DLL header: L, C, M=TCH, BCD meter ID, version, medium.
  // The input here is already DLL CRC-verified and CRC-stripped.
  uint8_t short_frame[] = {
      0x10, 0x44, 0x68, 0x50, 0x29, 0x37, 0x41, 0x22, 0x95, 0x62,
      0x7A, 0x01, 0x00, 0x20, 0x05, 0x2F, 0x2F};
  const auto short_cf = [&](uint8_t lo, uint8_t hi) {
    short_frame[13] = lo;
    short_frame[14] = hi;
    return inspect(short_frame, sizeof(short_frame));
  };

  auto r = short_cf(0x20, 0x05);
  assert(r.has_config_word && r.mode == 5 && r.state == State::Encrypted);
  r = short_cf(0x20, 0x04);
  assert(r.has_config_word && r.mode == 4 && r.state == State::Encrypted);
  for (uint8_t mode : {uint8_t{7}, uint8_t{10}, uint8_t{13}}) {
    r = short_cf(0x20, mode);
    assert(r.has_config_word && r.mode == mode && r.state == State::Encrypted);
  }

  // OMS Vol.2 §7.2.4.2: mode M=0 is clear ONLY when N=0.
  // The old probe mistakenly accepted 0x0020 (M=0, N=2) as clear.
  r = short_cf(0x20, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::Unknown);
  r = short_cf(0x10, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::Unknown);
  r = short_cf(0xF0, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::Unknown);
  r = short_cf(0x00, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::NotEncrypted);

  // Link flags B/A/S/C/R/H may be set; no encrypted blocks still means clear.
  r = short_cf(0x0F, 0xE0);
  assert(r.has_config_word && r.mode == 0 && r.state == State::NotEncrypted);
  r = short_cf(0x2F, 0xE0);
  assert(r.has_config_word && r.mode == 0 && r.state == State::Unknown);

  // The fifth mode bit (CF bit 12) must not alias reserved mode 16 to zero!
  for (uint8_t mode : {uint8_t{15}, uint8_t{16}, uint8_t{17},
                       uint8_t{31}}) {
    r = short_cf(0x00, mode);
    assert(r.has_config_word && r.mode == mode && r.state == State::Unknown);
  }
  // Reserved/unsupported modes do not accidentally become clear.
  r = short_cf(0x00, 0x06);
  assert(r.has_config_word && r.mode == 6 && r.state == State::Unknown);
  // The standard short TPL header must still be complete and valid.
  short_cf(0x00, 0x00);
  short_frame[0] = 0x11;  // invalid L-field
  assert(inspect(short_frame, sizeof(short_frame)).state == State::Unknown);
  short_frame[0] = 0x10;
  assert(inspect(short_frame, 14).state == State::Unknown); // truncated

  // Long TPL behind a potential converter has its own CF offset (21/22).
  uint8_t long_frame[] = {
      0x18, 0x44, 0x68, 0x50, 0x29, 0x37, 0x41, 0x22, 0x95, 0x62,
      0x72, 0x29, 0x37, 0x41, 0x22, 0x68, 0x50, 0x95, 0x62,
      0x01, 0x00, 0x20, 0x05, 0x2F, 0x2F};
  const auto long_cf = [&](uint8_t lo, uint8_t hi) {
    long_frame[21] = lo;
    long_frame[22] = hi;
    return inspect(long_frame, sizeof(long_frame));
  };
  r = long_cf(0x20, 0x05);
  assert(r.has_config_word && r.mode == 5 && r.state == State::Encrypted);
  r = long_cf(0x20, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::Unknown);
  r = long_cf(0x00, 0x00);
  assert(r.has_config_word && r.mode == 0 && r.state == State::NotEncrypted);
  r = long_cf(0x00, 0x10);
  assert(r.has_config_word && r.mode == 16 && r.state == State::Unknown);
  r = long_cf(0xF0, 0x0A);
  assert(r.has_config_word && r.mode == 10 && r.state == State::Encrypted);

  // Manufacturer-specific CI A0/A2 cannot be interpreted as standard CF.
  short_frame[10] = 0xA2;
  r = inspect(short_frame, sizeof(short_frame));
  assert(!r.has_config_word && r.state == State::Unknown);
  short_frame[10] = 0xA0;
  r = inspect(short_frame, sizeof(short_frame));
  assert(!r.has_config_word && r.state == State::Unknown);
  std::puts("PASS OMS TPL security 5-bit mode; Mode-0 M=N=0; short and long "
            "TPL fail closed on malformed/reserved security configurations");
}
