#pragma once
// Diagnostic-only, bounded T1 Format A CRC search. No ESPHome or radio dependencies.
// Never dispatch the reconstructed bytes to wmbus_meter.
#include <array>
#include <cstddef>
#include <cstdint>
#include "wmbus_t1_length.h"

namespace esphome {
namespace sx127x {

enum class T1RecoveryStatus : uint8_t {
  CRC_VALID, CRC_FAIL, INCOMPLETE, BAD_PREFIX, TOO_MANY_ERRORS,
  NO_ONE_BIT_CHOICE, COMBINATION_LIMIT, TIME_BUDGET
};

struct T1RecoveryResult {
  T1RecoveryStatus status{T1RecoveryStatus::BAD_PREFIX};
  uint8_t bad_symbols{0};
  uint16_t combinations{0};
  uint16_t attempts{0};
  uint8_t crc_valid_candidates{0};
};

inline uint16_t t1_diag_crc16_en13757(const uint8_t *data, size_t size) {
  uint16_t crc = 0;
  for (size_t i = 0; i < size; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8U;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = static_cast<uint16_t>((crc << 1U) ^ ((crc & 0x8000U) ? 0x3D65U : 0U));
  }
  return static_cast<uint16_t>(crc ^ 0xFFFFU);
}

inline bool t1_diag_format_a_crc(const uint8_t *frame, size_t bytes) {
  if (frame == nullptr || bytes < 14U) return false;
  const size_t l = frame[0];
  if (l < 10U) return false;
  const size_t data_bytes = l + 1U;
  const size_t blocks = 1U + (l - 9U + 15U) / 16U;
  if (bytes != data_bytes + blocks * 2U) return false;
  size_t offset = 0, remaining = data_bytes;
  bool first = true;
  while (remaining != 0) {
    const size_t length = remaining < (first ? 10U : 16U) ?
                            remaining : (first ? 10U : 16U);
    if (offset + length + 2U > bytes) return false;
    const uint16_t found = static_cast<uint16_t>(
        (static_cast<uint16_t>(frame[offset + length]) << 8U) |
        frame[offset + length + 1U]);
    if (t1_diag_crc16_en13757(frame + offset, length) != found) return false;
    offset += length + 2U;
    remaining -= length;
    first = false;
  }
  return offset == bytes;
}

inline uint8_t t1_diag_read_symbol(const uint8_t *raw, size_t symbol_index) {
  const size_t bit_offset = symbol_index * 6U;
  uint8_t code = 0;
  for (unsigned i = 0; i < 6U; ++i) {
    const size_t pos = bit_offset + i;
    code = static_cast<uint8_t>(
        (code << 1U) | ((raw[pos / 8U] >> (7U - pos % 8U)) & 1U));
  }
  return code;
}

// Intended for a LOW priority diagnostic worker, not the ISR or main RX loop.
// `continue_search` may stop work based on a wall-time budget and yield.
// Caller must not publish the reconstructed data; only the summary is returned.
template<typename KeepRunning>
T1RecoveryResult t1_diag_try_recover(const uint8_t *raw, size_t raw_bytes,
                                     KeepRunning continue_search,
                                     unsigned max_invalid = 5U,
                                     unsigned max_combinations = 1024U) {
  T1RecoveryResult result{};
  if (raw == nullptr || raw_bytes < 3U) return result;
  const int hi = t1_symbol_to_nibble(t1_diag_read_symbol(raw, 0));
  const int lo = t1_symbol_to_nibble(t1_diag_read_symbol(raw, 1));
  if (hi < 0 || lo < 0) return result;
  const size_t l = static_cast<size_t>((hi << 4) | lo);
  if (l < 10U) return result;
  const size_t blocks = 1U + (l - 9U + 15U) / 16U;
  const size_t frame_bytes = l + 1U + 2U * blocks;
  const size_t symbols_needed = frame_bytes * 2U;
  const size_t raw_needed = (frame_bytes * 3U + 1U) / 2U;
  if (raw_needed > raw_bytes || frame_bytes > 128U)
    return {T1RecoveryStatus::INCOMPLETE};
  // Decode all valid symbols once; enumerate only invalid ones later.
  std::array<uint8_t, 256> symbols{};
  std::array<uint16_t, 5> error_indices{};
  std::array<std::array<uint8_t, 6>, 5> options{};
  std::array<uint8_t, 5> option_counts{};
  unsigned invalid_count = 0;
  unsigned combinations = 1U;
  for (size_t i = 0; i < symbols_needed; ++i) {
    const uint8_t code = t1_diag_read_symbol(raw, i);
    symbols[i] = code;
    if (t1_symbol_to_nibble(code) >= 0) continue;
    ++invalid_count;
    if (invalid_count > max_invalid || invalid_count > error_indices.size())
      return {T1RecoveryStatus::TOO_MANY_ERRORS, static_cast<uint8_t>(invalid_count)};
    const size_t index = invalid_count - 1U;
    error_indices[index] = static_cast<uint16_t>(i);
    for (unsigned candidate = 0; candidate < 64U; ++candidate) {
      const auto legal = static_cast<uint8_t>(candidate);
      if (t1_symbol_to_nibble(legal) < 0) continue;
      const uint8_t delta = static_cast<uint8_t>(code ^ legal);
      if (delta == 0U || (delta & static_cast<uint8_t>(delta - 1U)) != 0U) continue;
      options[index][option_counts[index]++] = legal;
    }
    if (option_counts[index] == 0U)
      return {T1RecoveryStatus::NO_ONE_BIT_CHOICE, static_cast<uint8_t>(invalid_count)};
    if (combinations > max_combinations / option_counts[index])
      return {T1RecoveryStatus::COMBINATION_LIMIT, static_cast<uint8_t>(invalid_count)};
    combinations *= option_counts[index];
  }
  result.bad_symbols = static_cast<uint8_t>(invalid_count);
  result.combinations = static_cast<uint16_t>(combinations);
  std::array<uint8_t, 5> choice{};
  std::array<uint8_t, 128> decoded{};
  for (unsigned trial = 0; trial < combinations; ++trial) {
    if (!continue_search(trial))
      return {T1RecoveryStatus::TIME_BUDGET, result.bad_symbols,
              result.combinations, result.attempts, result.crc_valid_candidates};
    for (unsigned e = 0; e < invalid_count; ++e)
      symbols[error_indices[e]] = options[e][choice[e]];
    bool decode_ok = true;
    for (size_t i = 0; i < frame_bytes; ++i) {
      const int a = t1_symbol_to_nibble(symbols[i * 2U]);
      const int b = t1_symbol_to_nibble(symbols[i * 2U + 1U]);
      if (a < 0 || b < 0) { decode_ok = false; break; }
      decoded[i] = static_cast<uint8_t>((a << 4) | b);
    }
    ++result.attempts;
    if (decode_ok && t1_diag_format_a_crc(decoded.data(), frame_bytes))
      ++result.crc_valid_candidates;
    // Mixed-radix enumeration, including the no-errors/one-trial case.
    for (unsigned e = 0; e < invalid_count; ++e) {
      if (++choice[e] < option_counts[e]) break;
      choice[e] = 0;
    }
  }
  result.status = result.crc_valid_candidates ? T1RecoveryStatus::CRC_VALID :
                                               T1RecoveryStatus::CRC_FAIL;
  return result;
}

}  // namespace sx127x
}  // namespace esphome
