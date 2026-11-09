#pragma once
// Mode 5 decryption orchestration. CBC plaintext is not authenticated.
// The 2F 2F marker is only a protocol plausibility check, not cryptographic
// integrity. A caller must enforce its own publication/validation policy.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "pure_aes_cbc.h"
#include "pure_mode5_prepare.h"
namespace wmbus_pure {
enum class Mode5DecryptStatus : uint8_t {
  Ok, InvalidPreparation, CryptoFailure, InvalidPlaintextMarker
};
inline Mode5DecryptStatus decrypt_mode5(const Mode5Prepared &prepared,
                                        const uint8_t key[16],
                                        Aes128DecryptBlock decrypt_block,
                                        void *context,
                                        std::vector<uint8_t> &plaintext) {
  plaintext.clear();
  if (!key || !prepared.encrypted || prepared.encrypted_size < 16 ||
      prepared.encrypted_size > 240 || prepared.encrypted_size % 16 != 0 ||
      !decrypt_block) return Mode5DecryptStatus::InvalidPreparation;
  std::vector<uint8_t> decrypted;
  if (decrypt_aes128_cbc(prepared.encrypted, prepared.encrypted_size,
                         key, prepared.iv, decrypt_block, context, decrypted)
      != AesCbcStatus::Ok) return Mode5DecryptStatus::CryptoFailure;
  // EN13757 mode 5 encrypted data starts with 0x2F 0x2F.
  if (decrypted.size() < 2 || decrypted[0] != 0x2F || decrypted[1] != 0x2F)
    return Mode5DecryptStatus::InvalidPlaintextMarker;
  plaintext.swap(decrypted);
  return Mode5DecryptStatus::Ok;
}

enum class ConfiguredMode5DecryptStatus : uint8_t {
  Ok, PreparationFailed, DecryptionFailed
};

// CRC-stripped DLL input; caller is responsible for DLL CRC verification.
// This routine validates identity and transport before attempting AES.
// The plaintext is cleared on every failure and is never published here.
inline ConfiguredMode5DecryptStatus decrypt_configured_mode5(
    const uint8_t *frame, size_t size, uint32_t expected_meter_id,
    const uint8_t key[16], Aes128DecryptBlock decrypt_block,
    void *context, std::vector<uint8_t> &plaintext) {
  plaintext.clear();
  Mode5Prepared prepared{};
  if (prepare_configured_mode5(frame, size, expected_meter_id, prepared) !=
      ConfiguredMode5Status::Ok)
    return ConfiguredMode5DecryptStatus::PreparationFailed;
  if (decrypt_mode5(prepared, key, decrypt_block, context, plaintext) !=
      Mode5DecryptStatus::Ok) {
    plaintext.clear();
    return ConfiguredMode5DecryptStatus::DecryptionFailed;
  }
  return ConfiguredMode5DecryptStatus::Ok;
}
} // namespace wmbus_pure
