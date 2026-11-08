#pragma once
// Single tested runtime admission path for explicit secure_meters bindings.
// Mirrors the ESPHome YAML whitelist; never logs or retains the key in hex.
// Pure C++17: host regression can exercise the exact function the native
// Heltec receiver calls, independently of ESPHome/ESP-IDF headers.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_native_sx126x {
namespace secure_bindings {

struct Entry {
  std::string id;
  std::string dll_id;
  std::string driver;
  std::array<uint8_t, 16> key{};
};

inline bool supported_driver(const std::string &driver) {
  return driver == "minomess" || driver == "kamwater" ||
         driver == "apator162" || driver == "mkradio4a" ||
         driver == "topaseskr";
}

inline bool decimal_meter_id(const std::string &id) {
  if (id.size() != 8) return false;
  for (char c : id) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

inline int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// A separate DLL radio converter is supported only for the drivers that
// explicitly validate a distinct nested TPL meter identity on decode.
// No AUTO, guessed keys, plaintext fallback or firmware flashing.
inline bool add(std::vector<Entry> &entries, const std::string &id,
                const std::string &driver, const std::string &key_hex,
                const std::string &dll_id) {
  if (!decimal_meter_id(id) || !decimal_meter_id(dll_id) ||
      key_hex.size() != 32 || !supported_driver(driver) ||
      (dll_id != id && driver != "minomess" && driver != "mkradio4a"))
    return false;
  for (const auto &existing : entries) {
    if (existing.id == id || existing.dll_id == dll_id)
      return false;
  }
  Entry binding{};
  binding.id = id;
  binding.dll_id = dll_id;
  binding.driver = driver;
  for (size_t i = 0; i < binding.key.size(); ++i) {
    const int high = hex_digit(key_hex[2 * i]);
    const int low = hex_digit(key_hex[2 * i + 1]);
    if (high < 0 || low < 0) return false;
    binding.key[i] = static_cast<uint8_t>((high << 4) | low);
  }
  entries.push_back(binding);
  return true;
}

}  // namespace secure_bindings
}  // namespace wmbus_native_sx126x
}  // namespace esphome
