#pragma once
// Common bounded EN13757 DIF/VIF reader for plaintext application data.
// No meter identifiers, no keys, no radio assumptions, no implicit decrypt.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "calculate_subset.h"
namespace xmq_portable { namespace standard {
enum class Status : uint8_t { Ok, Invalid, Unsupported, Capacity };
struct Record {
  uint8_t dif{0}, vif{0}, vif_extension{0};
  uint16_t storage{0};
  uint8_t tariff{0}, function{0};
  double value{0};
  uint32_t raw{0};
  bool numeric{false};
};
struct Calendar {
  int year{0}, month{0}, day{0}, hour{0}, minute{0};
  bool has_time{false};
};
inline bool leap(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
inline bool decode_calendar(uint32_t raw, bool datetime, Calendar &out) {
  out = {};
  const uint8_t date0 = datetime ? static_cast<uint8_t>(raw >> 16)
                                 : static_cast<uint8_t>(raw);
  const uint8_t date1 = datetime ? static_cast<uint8_t>(raw >> 24)
                                 : static_cast<uint8_t>(raw >> 8);
  out.day = date0 & 31;
  out.month = date1 & 15;
  out.year = 2000 + (date0 >> 5) + ((date1 >> 4) << 3);
  out.has_time = datetime;
  if (datetime) {
    out.minute = raw & 0x3F;
    out.hour = (raw >> 8) & 0x1F;
  }
  static constexpr int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
  if (out.month < 1 || out.month > 12) return false;
  const int max_day = days[out.month] + (out.month == 2 && leap(out.year) ? 1 : 0);
  if (out.day < 1 || out.day > max_day) return false;
  return !datetime || (out.hour < 24 && out.minute < 60);
}
inline size_t format_length(uint8_t f) {
  switch(f) {
    case 0: return 0; case 1: return 1; case 2: return 2;
    case 3: return 3; case 4: return 4; case 5: return 4;
    case 6: return 6; case 7: return 8; case 9: return 1;
    case 10: return 2; case 11: return 3; case 12: return 4;
    case 14: return 6; default: return SIZE_MAX;
  }
}
inline Status records(const uint8_t *data, size_t size, Record *out,
                      size_t capacity, size_t &count) {
  count = 0;
  if (!data || !out || !capacity) return Status::Invalid;
  size_t i = 0;
  while (i < size) {
    uint8_t dif = data[i++];
    if (dif == 0x2F) continue;
    if (dif == 0x0F || dif == 0x1F) break; // manufacturer-specific tail
    Record r{};
    r.dif = dif;
    r.function = (dif >> 4) & 3;
    r.storage = (dif >> 6) & 1;
    unsigned shift = 1;
    unsigned tariff_shift = 0;
    while (dif & 0x80) {
      if (i >= size || shift > 13) return Status::Invalid;
      const uint8_t ext = data[i++];
      r.storage |= static_cast<uint16_t>(ext & 0x0F) << shift;
      r.tariff |= static_cast<uint8_t>((ext >> 4) & 3) << tariff_shift;
      tariff_shift += 2;
      shift += 4;
      dif = ext;
    }
    if (i >= size) return Status::Invalid;
    uint8_t vif = data[i++];
    r.vif = vif & 0x7F;
    if (r.vif == 0x7C) { // variable VIF label; validated length
      if (i >= size) return Status::Invalid;
      const size_t label = data[i++];
      if (label > size-i) return Status::Invalid;
      i += label;
    }
    if (vif & 0x80) {
      if (i >= size) return Status::Invalid;
      r.vif_extension = data[i] & 0x7F;
      do {
        vif = data[i++];
        if ((vif & 0x80) && i >= size) return Status::Invalid;
      } while (vif & 0x80);
    } else if (r.vif == 0x7D || r.vif == 0x7B) {
      // 0xFD/FB extended VIF has its first extension octet even if
      // higher-level matching is not yet supported.
      if (i >= size) return Status::Invalid;
    }
    const uint8_t fmt = r.dif & 0x0F;
    size_t n = format_length(fmt);
    if (fmt == 13) {
      if (i >= size) return Status::Invalid;
      const uint8_t lvar = data[i++];
      n = lvar <= 0xBF ? lvar :
          (lvar >= 0xC0 && lvar <= 0xC9) ? lvar-0xC0 :
          (lvar >= 0xD0 && lvar <= 0xD9) ? lvar-0xD0 : SIZE_MAX;
    }
    if (n == SIZE_MAX) return Status::Unsupported;
    if (n > size-i) return Status::Invalid;
    if (fmt >= 1 && fmt <= 4) {
      uint32_t raw = 0;
      for (size_t j = 0; j < n; ++j) raw |= uint32_t(data[i+j]) << (8*j);
      r.raw = raw;
      int32_t signed_value = static_cast<int32_t>(raw);
      if (n < 4 && (data[i+n-1] & 0x80))
        signed_value = static_cast<int32_t>(raw | (~uint32_t(0) << (8*n)));
      r.value = signed_value; r.numeric = true;
    } else if (fmt == 9 || fmt == 10 || fmt == 11 || fmt == 12 || fmt == 14) {
      double multiplier = 1, value = 0;
      bool negative = false;
      for (size_t j=0;j<n;j++) {
        const uint8_t low=data[i+j]&15, high=data[i+j]>>4;
        if (low>9 || (high>9 && !(j==n-1 && high==0xF)))
          return Status::Unsupported;
        value += low*multiplier; multiplier *= 10;
        if (j==n-1 && high==0xF) negative = true;
        else { value += high*multiplier; multiplier *= 10; }
      }
      r.value = negative ? -value : value; r.numeric = true;
    } else if (fmt==5 || fmt==6 || fmt==7 || fmt==13) {
      // Future standard types: preserve bounds, do not invent values.
      r.numeric = false;
    }
    i += n;
    if (count >= capacity) return Status::Capacity;
    out[count++] = r;
  }
  return Status::Ok;
}
inline double scale10(int power) {
  double v=1;
  while(power<0) { v/=10; ++power; }
  while(power>0) { v*=10; --power; }
  return v;
}
} } // xmq_portable::standard
