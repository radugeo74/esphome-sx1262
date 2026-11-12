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
int main() {
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
  std::puts("PASS: actual Format A/B CRC functions, C1 boundaries, corrupt frames");
}
"""
# Trimmer uses std::min indirectly only in the fixture.
harness = "#include <algorithm>\n#include <utility>\n" + harness
with tempfile.TemporaryDirectory() as td:
    td = Path(td)
    (td / "test.cpp").write_text(harness)
    binary = td / "test"
    command = [
        os.environ.get("CXX", "g++"), "-std=c++17", "-O1", "-g",
        "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        str(td / "test.cpp"), "-o", str(binary)
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
