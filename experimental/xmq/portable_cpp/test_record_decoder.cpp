#include "record_decoder.h"
#include <cassert>
#include <cmath>
#include <cstdint>
using namespace xmq_portable;
static void test(const char *key, size_t n, uint8_t lo, uint8_t hi,
                 Unit unit, double scaled) {
  const uint8_t value[2] = {lo, hi};
  Record record{};
  assert(decode_record(key, n, value, 2, record) == Status::Ok);
  assert(record.unit == unit);
  assert(record.raw == static_cast<unsigned>(lo | (hi << 8)));
  assert(std::fabs(record.scaled - scaled) < 1e-7);
}
int main() {
  test("4215", 4, 1, 0, Unit::VolumeM3, 0.1);
  test("0215", 4, 3, 0, Unit::VolumeM3, 0.3);
  test("426E", 4, 0x2D, 0, Unit::Hca, 45);
  test("026E", 4, 4, 0, Unit::Hca, 4);
  test("0265", 4, 0xBD, 0x08, Unit::TemperatureC, 22.37);
  test("025D", 4, 0x38, 0x09, Unit::TemperatureC, 23.60);
  test("02FD3A", 6, 0x9F, 0x29, Unit::RawCounter, 0x299F);
  test("42FD3A", 6, 0x50, 0x24, Unit::RawCounter, 0x2450);
  const uint8_t data[2] = {0, 0};
  Record out{};
  assert(decode_record("0201", 4, data, 2, out) == Status::UnsupportedVif);
  assert(decode_record("0201", 4, data, 1, out) == Status::InvalidLength);
  assert(decode_record("02ZZ", 4, data, 2, out) == Status::InvalidKey);
  return 0;
}
