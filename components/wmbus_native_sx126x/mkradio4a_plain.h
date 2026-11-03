#pragma once
// Bounded portable extractor implementing the two standard storage-1 fields
// declared by the pinned upstream mkradio4a.xmq. No AES or legacy decoder.
// Accepts only CRC-verified, CRC-stripped DLL frames from the native receiver.
#include <cstddef>
#include <cstdint>
namespace xmq_portable::mkradio4a {
enum class Status : uint8_t { Ok, TooShort, WrongMeterType, UnsupportedEnvelope, MissingFields, InvalidDate };
struct Reading { double target_m3{0}; int year{0}; int month{0}; int day{0}; };
inline Status decode(const uint8_t *data, size_t size, Reading &out) {
  out = {};
  if (!data || size < 35 || size != static_cast<size_t>(data[0]) + 1) return Status::TooShort;
  // TCH/95/37 is the radio converter header in upstream mkradio4a.
  // Other supported upstream MVTs can be added after a separate fixture test.
  if (data[2] != 0x68 || data[3] != 0x50 ||
      data[8] != 0x95 || data[9] != 0x37) return Status::WrongMeterType;
  // CI=72 long transport header, 3005 mode-5 config and 2F2F
  // cleartext confirmation prefix. Do not infer that unknown ciphertext is clear.
  if (data[10] != 0x72 || data[21] != 0x30 || data[22] != 0x05 ||
      data[23] != 0x2F || data[24] != 0x2F) return Status::UnsupportedEnvelope;
  // DIF 43 + VIF 15 = 24-bit Volume 0.1 m3, storage 1.
  // DIF 42 + VIF 6C = 16-bit Date G, storage 1.
  if (data[25] != 0x43 || data[26] != 0x15 ||
      data[30] != 0x42 || data[31] != 0x6C ||
      data[34] != 0x0F) return Status::MissingFields;
  const uint32_t volume = static_cast<uint32_t>(data[27]) |
                          (static_cast<uint32_t>(data[28]) << 8) |
                          (static_cast<uint32_t>(data[29]) << 16);
  const uint8_t d0 = data[32], d1 = data[33];
  const int day = d0 & 0x1F, month = d1 & 0x0F;
  const int year = 2000 + ((d0 >> 5) | ((d1 >> 4) << 3));
  if (day < 1 || day > 31 || month < 1 || month > 12 ||
      (month == 2 && day > 29) ||
      ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30))
    return Status::InvalidDate;
  out.target_m3 = static_cast<double>(volume) * 0.1;
  out.year = year; out.month = month; out.day = day;
  return Status::Ok;
}
} // namespace xmq_portable::mkradio4a
