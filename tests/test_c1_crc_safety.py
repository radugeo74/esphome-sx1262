#!/usr/bin/env python3
"""Compile the REAL wmbus.cc Format A/B CRC routines with ASan/UBSan.

Extract only these functions to avoid linking the entire ESPHome application.
This tests the implementation in the repository, not a separately rewritten CRC
trimmer. All frames are synthetic; no meter IDs, keys or user data.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "components/wmbus_common/wmbus.cc").read_text()


def extract_function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    level = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            level += 1
        elif source[pos] == "}":
            level -= 1
            if level == 0:
                return source[start : pos + 1]
    raise ValueError(f"Unterminated function: {signature}")


format_a = extract_function("bool trimCRCsFrameFormatAInternal(")
format_b = extract_function("bool trimCRCsFrameFormatBInternal(")
harness = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include "components/sx127x/wmbus_t1_length.h"
#include "components/wmbus_radio/decode3of6.h"
#include <vector>
using uchar = unsigned char;
static constexpr bool FUZZING = false;
static void debug(const char *, ...) {}
static void debugPayload(const char *, const std::vector<uchar> &) {}
static uchar *safeButUnsafeVectorPtr(std::vector<uchar> &v) { return v.data(); }
// EN 13757-4, reflected-independent MSB-first bitwise reference CRC.
static uint16_t crc16_EN13757(const uchar *buf, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; ++i) {
    crc ^= static_cast<uint16_t>(buf[i]) << 8;
    for (int b = 0; b < 8; ++b)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x3D65)
                           : static_cast<uint16_t>(crc << 1);
  }
  return static_cast<uint16_t>(~crc);
}
static void append_crc(std::vector<uchar> &out, size_t start, size_t count) {
  const uint16_t crc = crc16_EN13757(out.data() + start, count);
  out.push_back(static_cast<uchar>(crc >> 8));
  out.push_back(static_cast<uchar>(crc & 255));
}
"""
harness += "\n" + format_a + "\n" + format_b + "\n"
harness += r"""
static std::vector<uchar> format_b_frame(size_t n) {
  assert(n >= 12 && n <= 253 && n != 129 && n != 130);
  std::vector<uchar> data;
  const size_t payload = n <= 128 ? n - 2 : n - 4;
  data.reserve(n);
  for (size_t i = 0; i < payload; ++i)
    data.push_back(i == 0 ? static_cast<uchar>(n - 1)
                          : i == 1 ? 0x44 : static_cast<uchar>(i * 13));
  if (n <= 128) {
    append_crc(data, 0, payload);
  } else {
    // CRC1 after the first 126 bytes; CRC2 covers the remaining data.
    std::vector<uchar> full(data.begin(), data.begin() + 126);
    append_crc(full, 0, 126);
    full.insert(full.end(), data.begin() + 126, data.end());
    append_crc(full, 128, n - 130);
    data = std::move(full);
  }
  assert(data.size() == n);
  return data;
}
static std::vector<uchar> format_a_frame(size_t l) {
  assert(l >= 10 && l <= 200);
  std::vector<uchar> data;
  data.reserve(256);
  for (size_t i = 0; i <= l; ++i)
    data.push_back(i == 0 ? static_cast<uchar>(l)
                          : i == 1 ? 0x44 : static_cast<uchar>(i * 17));
  std::vector<uchar> framed;
  framed.insert(framed.end(), data.begin(), data.begin() + 10);
  append_crc(framed, 0, 10);
  for (size_t pos = 10; pos < data.size();) {
    const size_t count = std::min<size_t>(16, data.size() - pos);
    const size_t first = framed.size();
    framed.insert(framed.end(), data.begin() + pos, data.begin() + pos + count);
    append_crc(framed, first, count);
    pos += count;
  }
  return framed;
}
// Test generator only. Decode uses the unchanged project decoder linked below.
static std::vector<uchar> encode_t1(const std::vector<uchar> &plain) {
  static constexpr uint8_t symbols[16] = {
    0x16, 0x0D, 0x0E, 0x0B, 0x1C, 0x19, 0x1A, 0x13,
    0x2C, 0x25, 0x26, 0x23, 0x34, 0x31, 0x32, 0x29
  };
  std::vector<uchar> encoded;
  uint8_t partial = 0;
  unsigned count = 0;
  for (uchar v : plain) {
    for (unsigned nibble = 0; nibble != 2; ++nibble) {
      const uint8_t sym = symbols[nibble == 0 ? v >> 4 : v & 15];
      for (int b = 5; b >= 0; --b) {
        partial = static_cast<uint8_t>((partial << 1) | ((sym >> b) & 1));
        if (++count == 8) {
          encoded.push_back(partial);
          partial = 0;
          count = 0;
        }
      }
    }
  }
  if (count != 0)
    encoded.push_back(static_cast<uchar>(partial << (8 - count)));
  return encoded;
}

struct CandidateVerdict { bool t1{false}; bool c1{false}; };
static CandidateVerdict receive_candidate(uint8_t marker, const std::vector<uchar> &raw) {
  CandidateVerdict verdict;
  if (raw.size() < 3)
    return verdict;
  const auto sizes = esphome::sx127x::wmbus_raw_candidates(
      marker, true, raw.data(), raw.size(), 384);
  if (sizes.t1 != 0 && sizes.t1 == raw.size()) {
    auto copy = raw;
    const auto decoded = esphome::wmbus_radio::decode3of6(copy);
    if (decoded && !decoded->empty()) {
      auto frame = *decoded;
      if (frame[0] + 1 <= frame.size()) {
        verdict.t1 = trimCRCsFrameFormatAInternal(frame, true) &&
                     frame.size() == static_cast<size_t>(frame[0]) + 1;
      }
    }
  }
  if (sizes.c1 != 0 && sizes.c1 == raw.size()) {
    auto frame = raw;
    const bool crc_ok = marker == 0xCD
                            ? trimCRCsFrameFormatAInternal(frame, true)
                            : trimCRCsFrameFormatBInternal(frame, true);
    verdict.c1 = crc_ok && !frame.empty() &&
                 frame.size() == static_cast<size_t>(frame[0]) + 1;
  }
  return verdict;
}

static void regression_sx127x_t1_c1() {
  // C1/B and T1 share radio sync 54 3D; only a CRC-valid candidate wins.
  for (size_t l : {10u, 25u, 26u, 42u, 46u, 90u, 180u}) {
    auto encoded = encode_t1(format_a_frame(l));
    auto good = receive_candidate(0x3D, encoded);
    assert(good.t1 && !good.c1);
    auto bad = encoded;
    bad.at(bad.size() / 2) ^= 0x80;
    auto outcome = receive_candidate(0x3D, bad);
    assert(!outcome.t1 && !outcome.c1);
    encoded.pop_back(); // incomplete 3-of-6/CRC frame
    outcome = receive_candidate(0x3D, encoded);
    assert(!outcome.t1 && !outcome.c1);
  }
  for (size_t l : {10u, 25u, 26u, 42u, 90u, 180u}) {
    const auto raw = format_a_frame(l);
    const auto good = receive_candidate(0xCD, raw);
    assert(good.c1 && !good.t1);
    auto bad = raw;
    bad.back() ^= 1;
    auto outcome = receive_candidate(0xCD, bad);
    assert(!outcome.c1 && !outcome.t1);
    bad = raw;
    bad.pop_back();
    outcome = receive_candidate(0xCD, bad);
    assert(!outcome.c1 && !outcome.t1);
  }
  // Include both sides of the 128-byte Format-B CRC boundary.
  for (size_t n : {12u, 64u, 127u, 128u, 131u, 132u, 200u, 253u}) {
    const auto raw = format_b_frame(n);
    const auto good = receive_candidate(0x3D, raw);
    assert(good.c1 && !good.t1);
    auto bad = raw;
    bad.back() ^= 1;
    auto outcome = receive_candidate(0x3D, bad);
    assert(!outcome.c1 && !outcome.t1);
    bad = raw;
    bad.pop_back();
    outcome = receive_candidate(0x3D, bad);
    assert(!outcome.c1 && !outcome.t1);
  }
  for (int i = 0; i < 32; ++i) {
    assert(receive_candidate(0x3D, encode_t1(format_a_frame(42))).t1);
    assert(receive_candidate(0xCD, format_a_frame(26)).c1);
    assert(receive_candidate(0x3D, format_b_frame(131)).c1);
  }
  const std::vector<uchar> garbage(120, 0);
  assert(!receive_candidate(0x3D, garbage).t1);
  assert(!receive_candidate(0x3D, garbage).c1);
  assert(!receive_candidate(0xCD, garbage).c1);
  assert(!receive_candidate(0x12, format_a_frame(26)).c1);
}

int main() {
  regression_sx127x_t1_c1();
  for (size_t n : {12u, 64u, 127u, 128u, 131u, 132u, 200u, 253u}) {
    auto original = format_b_frame(n);
    auto frame = original;
    assert(trimCRCsFrameFormatBInternal(frame, true));
    assert(frame.size() == n - (n <= 128 ? 2u : 4u));
    assert(frame[0] == frame.size() - 1);
    frame = original;
    frame[n - 1] ^= 1;
    assert(!trimCRCsFrameFormatBInternal(frame, true));
    if (n > 128) {
      frame = original;
      frame[126] ^= 1;
      assert(!trimCRCsFrameFormatBInternal(frame, true));
    }
  }
  // Format-B boundary: the second segment must contain at least one byte.
  for (size_t n : {129u, 130u}) {
    std::vector<uchar> malformed(n, 0);
    malformed[0] = static_cast<uchar>(n - 1);
    assert(!trimCRCsFrameFormatBInternal(malformed, true));
  }
  for (size_t l : {10u, 25u, 26u, 42u, 90u, 180u}) {
    auto original = format_a_frame(l);
    auto frame = original;
    assert(trimCRCsFrameFormatAInternal(frame, true));
    assert(frame.size() == l + 1);
    frame = original;
    frame.back() ^= 1;
    assert(!trimCRCsFrameFormatAInternal(frame, true));
  }
  std::puts("PASS: real 3-of-6 + Format A/B CRC, T1/C1-A/B, truncation, corruption, back-to-back frames");
}
"""
# Trimmer uses std::min indirectly only in the fixture.
harness = "#include <algorithm>\n#include <utility>\n" + harness
with tempfile.TemporaryDirectory() as td:
    td = Path(td)
    (td / "test.cpp").write_text(harness)
    binary = td / "test"
    (td / "esphome" / "core").mkdir(parents=True, exist_ok=True)
    for name in ("helpers.h", "log.h"):
        (td / "esphome" / "core" / name).write_text("#pragma once\n")
    command = [
        os.environ.get("CXX", "g++"), "-std=c++17", "-O1", "-g",
        "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-Wno-unused-variable",
        "-I", str(ROOT), "-I", str(td),
        str(ROOT / "components/wmbus_radio/decode3of6.cpp"),
        str(td / "test.cpp"), "-o", str(binary)
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
