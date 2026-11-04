#pragma once
// Common bounded EN13757 DIF/VIF reader for plaintext application data.
// No meter identifiers, no keys, no radio assumptions, no implicit decrypt.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include "calculate_subset.h"
namespace xmq_portable { namespace standard {
enum class Status : uint8_t { Ok, Invalid, Unsupported, Capacity };
struct Record {
  uint8_t dif{0}, vif{0}, vif_extension{0};
  uint8_t vif_extension_count{0};
  uint32_t storage{0};
  uint16_t tariff{0};
  uint8_t subunit{0}, function{0};
  double value{0};
  uint32_t raw{0}; // Low 32 bits, retained for date/time compatibility.
  uint64_t raw64{0};
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
inline Status parse_records(const uint8_t *data, size_t size, Record *out,
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
    unsigned subunit_shift = 0;
    while (dif & 0x80) {
      if (i >= size || shift > 29 || tariff_shift > 14 || subunit_shift > 7)
        return Status::Unsupported;
      const uint8_t ext = data[i++];
      r.storage |= static_cast<uint32_t>(ext & 0x0F) << shift;
      r.tariff |= static_cast<uint16_t>((ext >> 4) & 3) << tariff_shift;
      r.subunit |= static_cast<uint8_t>((ext >> 6) & 1) << subunit_shift;
      ++subunit_shift;
      tariff_shift += 2;
      shift += 4;
      dif = ext;
    }
    if (i >= size) return Status::Invalid;
    uint8_t vif = data[i++];
    r.vif = vif & 0x7F;
    if (r.vif == 0x7C) { // Variable-length VIF label (including extensions).
      if (i >= size) return Status::Invalid;
      const size_t label = data[i++];
      if (label > size-i) return Status::Invalid;
      i += label;
    }
    // The high bit announces the VIFE chain. The previous implementation
    // failed to consume the extension after a literal 0xFD or 0xFB, causing
    // subsequent record bytes to be treated as new DIFs.
    bool more = (vif & 0x80u) != 0;
    unsigned extensions = 0;
    while (more) {
      if (i >= size) return Status::Invalid;
      if (++extensions > 10) return Status::Unsupported;
      const uint8_t ext = data[i++];
      if (extensions == 1) r.vif_extension = ext & 0x7Fu;
      r.vif_extension_count = static_cast<uint8_t>(extensions);
      more = (ext & 0x80u) != 0;
    }
    const uint8_t fmt = r.dif & 0x0F;
    size_t n = format_length(fmt);
    if (fmt == 13) {
      if (i >= size) return Status::Invalid;
      const uint8_t lvar = data[i++];
      // EN 13757-3 LVAR byte counts. Preserve variable-length records
      // as opaque: knowing their length does not establish a numeric value.
      if (lvar <= 0xBF) n = lvar; // 8-bit text
      else if (lvar >= 0xC0 && lvar <= 0xC9) n = lvar - 0xC0; // positive BCD
      else if (lvar >= 0xD0 && lvar <= 0xD9) n = lvar - 0xD0; // negative BCD
      else if (lvar >= 0xE0 && lvar <= 0xEF) n = lvar - 0xE0; // binary 0..15
      else if (lvar >= 0xF0 && lvar <= 0xF4)
        n = static_cast<size_t>(lvar - 0xEC) * 4; // binary 16..32
      else if (lvar == 0xF5) n = 48;
      else if (lvar == 0xF6) n = 64;
      else n = SIZE_MAX; // reserved code: fail closed
    }
    if (n == SIZE_MAX) return Status::Unsupported;
    if (n > size-i) return Status::Invalid;
    if ((fmt >= 1 && fmt <= 4) || fmt == 6 || fmt == 7) {
      uint64_t raw = 0;
      for (size_t j = 0; j < n; ++j)
        raw |= static_cast<uint64_t>(data[i+j]) << (8*j);
      r.raw = static_cast<uint32_t>(raw);
      r.raw64 = raw;
      // The old dvparser defaults to signed extraction. For unsigned
      // measurements the calling field matcher can use raw64 explicitly.
      const unsigned bits = static_cast<unsigned>(n*8);
      if (bits < 64 && (raw & (uint64_t{1} << (bits-1))))
        r.value = static_cast<double>(static_cast<int64_t>(
            raw | (~uint64_t{0} << bits)));
      else if (bits == 64) {
        // Convert magnitude *before* floating-point conversion. Subtracting
        // two rounded 2^63-sized doubles loses small negatives such as -3.
        r.value = (raw & (uint64_t{1} << 63)) ?
            -static_cast<double>((~raw) + uint64_t{1}) :
            static_cast<double>(raw);
      } else
        r.value = static_cast<double>(raw);
      r.numeric = true;
    } else if (fmt == 5) {
      // IEEE 754 binary32, little-endian, as in the original dvparser.
      uint32_t bits = 0;
      for (size_t j=0; j<4; ++j)
        bits |= static_cast<uint32_t>(data[i+j]) << (8*j);
      float v=0;
      static_assert(sizeof(v)==sizeof(bits), "binary32 required");
      std::memcpy(&v,&bits,sizeof(v));
      if (!std::isfinite(v)) return Status::Unsupported;
      r.raw=bits; r.raw64=bits;
      r.value=static_cast<double>(v); r.numeric=true;
    } else if (fmt == 9 || fmt == 10 || fmt == 11 ||
               fmt == 12 || fmt == 14) {
      double multiplier = 1, value = 0;
      bool negative = false;
      // BCD bytes contain the least significant pair first. An F in the
      // most significant nibble is a negative sign (upstream convention).
      for (size_t j=0; j<n; ++j) {
        const uint8_t low=data[i+j]&15, high=data[i+j]>>4;
        if (low>9 || (high>9 && !(j==n-1 && high==0xF)))
          return Status::Unsupported;
        value += low*multiplier; multiplier *= 10;
        if (j==n-1 && high==0xF) negative = true;
        else { value += high*multiplier; multiplier *= 10; }
      }
      r.value = negative ? -value : value; r.numeric = true;
    } else if (fmt == 13) {
      // LVAR text/binary remains opaque. Do not publish invented numbers.
      r.numeric = false;
    }
    i += n;
    if (count >= capacity) return Status::Capacity;
    out[count++] = r;
  }
  return Status::Ok;
}
// Transactional public API: malformed/truncated tails never leak a partial
// record count to downstream meter drivers or Home Assistant publications.
inline Status records(const uint8_t *data, size_t size, Record *out,
                      size_t capacity, size_t &count) {
  count = 0;
  const Status status = parse_records(data,size,out,capacity,count);
  if (status != Status::Ok) count = 0;
  return status;
}
// Canonical units mirror the original wmbusmeters vifScale() rules.
// Convert only known VIF families; do not silently assign units to unknown,
// extended, manufacturer-specific, or date/time VIFs.
enum class CanonicalUnit : uint8_t {
  Unknown, CubicMeters, CubicMetersPerHour, Celsius, Hours, Bar
};
inline CanonicalUnit canonical_unit(uint8_t vif) {
  if (vif >= 0x10 && vif <= 0x17) return CanonicalUnit::CubicMeters;
  if (vif >= 0x38 && vif <= 0x4F) return CanonicalUnit::CubicMetersPerHour;
  if ((vif >= 0x58 && vif <= 0x5F) ||
      (vif >= 0x64 && vif <= 0x67)) return CanonicalUnit::Celsius;
  if (vif >= 0x20 && vif <= 0x27) return CanonicalUnit::Hours;
  if (vif >= 0x68 && vif <= 0x6B) return CanonicalUnit::Bar;
  return CanonicalUnit::Unknown;
}
inline bool to_canonical(uint8_t vif, double raw, double &out) {
  out=0;
  if (!std::isfinite(raw) || canonical_unit(vif)==CanonicalUnit::Unknown)
    return false;
  double divisor=1;
  if (vif >= 0x10 && vif <= 0x17) divisor=std::pow(10.0, 0x16 - vif);
  else if (vif >= 0x38 && vif <= 0x3F)
    divisor=std::pow(10.0, 0x3E - vif);
  else if (vif >= 0x40 && vif <= 0x47)
    divisor=60.0*std::pow(10.0, 0x47 - vif);
  else if (vif >= 0x48 && vif <= 0x4F)
    divisor=360000.0*std::pow(10.0, 0x4F - vif);
  else if ((vif >= 0x58 && vif <= 0x5F) ||
           (vif >= 0x64 && vif <= 0x67)) {
    const int reference=vif<=0x5F ? (vif<=0x5B ? 0x5B : 0x5F) : 0x67;
    divisor=std::pow(10.0, reference-vif);
  } else if (vif >= 0x20 && vif <= 0x27) {
    switch (vif & 3u) {
      case 0: divisor=3600.0; break;
      case 1: divisor=60.0; break;
      case 2: divisor=1.0; break;
      default: divisor=1.0/24.0; break;
    }
  } else if (vif>=0x68 && vif<=0x6B)
    divisor=std::pow(10.0, 0x6B - vif);
  else return false;
  const double converted=raw/divisor;
  if (!std::isfinite(converted)) return false;
  out=converted;
  return true;
}
inline double scale10(int power) {
  double v=1;
  while(power<0) { v/=10; ++power; }
  while(power>0) { v*=10; --power; }
  return v;
}
} } // xmq_portable::standard
