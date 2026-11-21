#pragma once
// HOST-ONLY experimental TPL security-mode-5 AES-128-CBC block adapter.
// The caller must obtain key and IV from an audited telegram-specific
// security header parser. This helper does NOT validate the IV derivation,
// authenticate ciphertext, inspect a radio frame or update sensors.
// Reuses only the existing tiny-AES128-C primitive (no legacy Telegram).
#include "aes.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xmq_runtime {

struct Tpl5CbcResult {
  bool accepted{false};
  std::vector<std::uint8_t> plaintext;
};

inline Tpl5CbcResult decrypt_tpl5_cbc_blocks(
    const std::vector<std::uint8_t> &ciphertext,
    const std::array<std::uint8_t, 16> &key,
    const std::array<std::uint8_t, 16> &iv) noexcept {
  Tpl5CbcResult result;
  // Keep the scope intentionally smaller than the verified application cap.
  // Never call the legacy crypto primitive on partial AES blocks.
  if (ciphertext.size() < 16 || ciphertext.size() > 512 ||
      ciphertext.size() % 16 != 0)
    return result;
  std::vector<std::uint8_t> input;
  std::vector<std::uint8_t> plain;
  try {
    input = ciphertext;
    plain.resize(ciphertext.size());
    AES_CBC_decrypt_buffer(plain.data(), input.data(),
                           static_cast<std::uint32_t>(input.size()),
                           key.data(), iv.data());
    // TPL mode 5 check bytes are a weak sanity check, NOT authenticated
    // integrity. A future radio adapter must also validate TPL metadata.
    if (plain[0] == 0x2F && plain[1] == 0x2F) {
      result.plaintext = std::move(plain);
      result.accepted = true;
    }
  } catch (...) {
    result.accepted = false;
    result.plaintext.clear();
  }
  // Do not retain ciphertext or failed plaintext in this adapter.
  std::fill(input.begin(), input.end(), std::uint8_t{0});
  std::fill(plain.begin(), plain.end(), std::uint8_t{0});
  return result;
}

}  // namespace xmq_runtime
