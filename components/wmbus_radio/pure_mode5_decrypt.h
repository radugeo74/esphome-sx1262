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
} // namespace wmbus_pure
