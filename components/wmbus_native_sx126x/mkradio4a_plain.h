#pragma once
// Portable MK Radio 4a original-XMQ storage-one volume/date interpretation.
// Accept only an explicitly supplied already-clear (NOKEY fixture) envelope;
// caller MUST NOT infer plaintext from an encrypted RF packet.
// No key/AES or legacy meter decoder.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
namespace xmq_portable::mkradio4a {
enum class Status : uint8_t { Ok, TooShort, WrongMeterType, UnsupportedEnvelope, MissingFields, InvalidDate };
struct Reading { double target_m3{0}; int year{0}; int month{0}; int day{0}; };
// Only application bytes explicitly known to be in the clear may enter
// this API (e.g. the right-hand side of an official '_' test telegram).
// A raw RF frame with Mode 5 is never evidence of cleartext.
inline Status decode_clear_application(const uint8_t *application, size_t size,
                                       Reading &out) {
  out = {};
  if (!application || !size) return Status::TooShort;
  standard::Record records[24]{};
  size_t count=0;
  if (standard::records(application,size,records,24,count)!=standard::Status::Ok)
    return Status::MissingFields;
  bool found_volume=false, found_date=false;
  double volume=0;
  standard::Calendar date{};
  for (size_t i=0;i<count;i++) {
    const auto &rec=records[i];
    if (rec.storage!=1 || rec.function!=0 || rec.tariff!=0) continue;
    if (rec.numeric && rec.vif>=0x10 && rec.vif<=0x17 && rec.vif_extension_count==0) {
      if (found_volume) return Status::MissingFields;
      if (!standard::to_canonical(rec.vif,rec.value,volume)) return Status::MissingFields;
      found_volume=true;
    } else if (rec.vif==0x6C && rec.numeric) {
      if (found_date) return Status::MissingFields;
      if (!standard::decode_calendar(rec.raw,false,date)) return Status::InvalidDate;
      found_date=true;
    }
  }
  if (!found_volume || !found_date) return Status::MissingFields;
  out.target_m3=volume;
  out.year=date.year; out.month=date.month; out.day=date.day;
  return Status::Ok;
}
inline Status decode(const uint8_t *data, size_t size, Reading &out) {
  out = {};
  if (!data || size < 35 || size != static_cast<size_t>(data[0]) + 1)
    return Status::TooShort;
  // Official upstream fixture: TCH/95/37 radio converter and CI=72 TPLh.
  // Meter identity in TPL may differ from the converter DLL identity.
  if (data[2] != 0x68 || data[3] != 0x50 ||
      data[8] != 0x95 || data[9] != 0x37)
    return Status::WrongMeterType;
  // Mode 5 and 2F2F are present in an explicitly already-clear fixture.
  // These bytes alone NEVER prove a live encrypted radio frame is clear.
  if (data[10] != 0x72 || data[21] != 0x30 || data[22] != 0x05 ||
      data[23] != 0x2F || data[24] != 0x2F)
    return Status::UnsupportedEnvelope;

  return decode_clear_application(data+25,size-25,out);
}
} // namespace xmq_portable::mkradio4a
