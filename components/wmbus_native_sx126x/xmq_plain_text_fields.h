#pragma once
// Optional typed text fields for already-validated, genuinely plaintext TPL.
// No decryption, no conversion of calendar dates to floating-point sensors.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "hydrus_plain.h"
#include "iperl_plain.h"
#include "lansenpu_plain.h"
#include "tpl_security_probe.h"

namespace xmq_portable { namespace text_fields {
struct Field {
  const char *name{nullptr};
  char text[64]{}; // long upstream status bit flags must not be truncated
};
struct Result {
  Field fields[4]{};
  size_t count{0};
};
inline bool append(Result &out, const char *name, const char *value) {
  if (!name || !value || out.count >= 4) return false;
  Field &slot = out.fields[out.count++];
  slot.name = name;
  std::snprintf(slot.text, sizeof(slot.text), "%s", value);
  return true;
}
inline Result decode(const uint8_t *frame, size_t size, const char *driver) {
  Result out{};
  if (!frame || !driver || size < 15) return out;
  const auto security = wmbus_pure::tpl_security::inspect(frame, size);
  if (security.state != wmbus_pure::tpl_security::State::NotEncrypted)
    return out;
  if (std::strcmp(driver, "lansenpu") == 0) {
    lansenpu::Reading reading{};
    if(lansenpu::decode(frame,size,reading)!=lansenpu::Status::Ok)
      return out;
    const char *status=lansenpu::status_text(reading.tpl_status);
    if(status) append(out,"status",status);
    return out;
  }
  if (std::strcmp(driver, "iperl") == 0) {
    iperl::Reading reading{};
    if (iperl::decode(frame, size, reading) != iperl::Status::Ok) return out;
    // Only status patterns explicitly covered by official iPERL fixtures.
    if (reading.tpl_status == 0) append(out, "status", "OK");
    else if (reading.tpl_status == 0x06)
      append(out, "status", "ERROR POWER_LOW");
    return out;
  }
  if (std::strcmp(driver, "hydrus") != 0) return out;
  wmbus_pure::Header header{};
  if (wmbus_pure::parse_header(frame, size, header) != wmbus_pure::HeaderStatus::Ok)
    return out;
  const size_t offset = header.ci == 0x7A ? 15 :
                        header.ci == 0x72 ? 23 : 0;
  if (!offset || offset >= size) return out;
  hydrus::DateField dates[4]{};
  size_t date_count=0, value_count=0;
  CalcValue numeric[24]{};
  if (hydrus::decode_clear_application(frame + offset, size - offset,
      numeric, 24, value_count, dates, 4, &date_count) != hydrus::Status::Ok)
    return out;
  for (size_t i=0; i<date_count && i<4; ++i) {
    char result[24]{};
    const auto &d = dates[i].value;
    if (d.has_time)
      std::snprintf(result,sizeof(result),"%04d-%02d-%02d %02d:%02d",
                    d.year,d.month,d.day,d.hour,d.minute);
    else
      std::snprintf(result,sizeof(result),"%04d-%02d-%02d",
                    d.year,d.month,d.day);
    append(out,dates[i].name,result);
  }
  return out;
}
} }
