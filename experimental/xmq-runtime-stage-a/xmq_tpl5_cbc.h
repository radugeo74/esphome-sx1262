#pragma once
// HOST-ONLY experimental TPL security-mode-5 AES-128-CBC block adapter.
// The caller must obtain the TPL/DLL metadata from an independently validated
// telegram header parser. This helper constructs the mode-5 IV according to
// the existing wM-Bus implementation, but does NOT validate those header
// bytes, authenticate ciphertext, inspect radio frames or update sensors.
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

// Equivalent to the upstream wmbus_utils.cc mode-5 IV rule:
// M field (2), A field (6), and TPL access number repeated eight times.
// The caller chooses the INNER TPL address if present; otherwise DLL.
// The fields here must already have been validated from this SAME telegram.
struct Tpl5HeaderFields {
  std::array<std::uint8_t, 2> mfct_le{};
  std::array<std::uint8_t, 6> address_a{};
  std::uint8_t access{0};
};
inline std::array<std::uint8_t, 16> make_tpl5_iv(
    const Tpl5HeaderFields &fields) noexcept {
  std::array<std::uint8_t, 16> iv{};
  std::copy(fields.mfct_le.begin(), fields.mfct_le.end(), iv.begin());
  std::copy(fields.address_a.begin(), fields.address_a.end(), iv.begin() + 2);
  std::fill(iv.begin() + 8, iv.end(), fields.access);
  return iv;
}

// Mode 5 can encrypt only a prefix; the remaining trailing bytes are clear.
// An explicit block count comes from TPL config bits 4..7 (0 means entire
// application is encrypted). Never shrink/truncate a malformed ciphertext.
// The returned buffer includes the decrypted prefix and untouched clear tail.
inline Tpl5CbcResult decrypt_tpl5_application(
    const std::vector<std::uint8_t> &application,
    const std::array<std::uint8_t, 16> &key,
    const Tpl5HeaderFields &fields, std::uint8_t encrypted_blocks) noexcept {
  Tpl5CbcResult result{};
  if (encrypted_blocks > 15 || application.empty() ||
      application.size() > 1024)
    return result;
  const std::size_t encrypted_len =
      encrypted_blocks == 0 ? application.size()
                            : static_cast<std::size_t>(encrypted_blocks) * 16;
  if (encrypted_len < 16 || encrypted_len > 512 ||
      encrypted_len % 16 != 0 || encrypted_len > application.size())
    return result;
  try {
    const std::vector<std::uint8_t> cipher(
        application.begin(), application.begin() + encrypted_len);
    auto decrypted = decrypt_tpl5_cbc_blocks(
        cipher, key, make_tpl5_iv(fields));
    if (!decrypted.accepted) return result;
    decrypted.plaintext.insert(decrypted.plaintext.end(),
        application.begin() + encrypted_len, application.end());
    result = std::move(decrypted);
  } catch (...) {
    result = Tpl5CbcResult{};
  }
  return result;
}

}  // namespace xmq_runtime
