#pragma once
// Crypto-backend-neutral AES-128-CBC decryptor. The caller supplies a
// vetted AES-128 single-block decryption primitive (e.g. mbedTLS).
// CBC does not authenticate data. Never publish solely on successful decrypt.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
namespace wmbus_pure {
enum class AesCbcStatus : uint8_t { Ok, InvalidInput, BlockDecryptFailed };
using Aes128DecryptBlock = bool (*)(const uint8_t key[16], const uint8_t ciphertext[16],
                                   uint8_t plaintext[16], void *context);
inline AesCbcStatus decrypt_aes128_cbc(const uint8_t *ciphertext, size_t size,
                                      const uint8_t key[16], const uint8_t iv[16],
                                      Aes128DecryptBlock decrypt_block, void *context,
                                      std::vector<uint8_t> &output) {
  // Transactional API: always clear stale plaintext; never return partial data.
  output.clear();
  if (!ciphertext || !key || !iv || !decrypt_block || !size ||
      size % 16 != 0 || size > 256) return AesCbcStatus::InvalidInput;
  std::vector<uint8_t> staged(size);
  uint8_t previous[16];
  std::memcpy(previous, iv, 16);
  for (size_t offset = 0; offset < size; offset += 16) {
    uint8_t plaintext[16]{};
    if (!decrypt_block(key, ciphertext + offset, plaintext, context)) {
      std::memset(plaintext, 0, sizeof(plaintext));
      return AesCbcStatus::BlockDecryptFailed;
    }
    for (size_t i = 0; i < 16; ++i)
      staged[offset + i] = static_cast<uint8_t>(plaintext[i] ^ previous[i]);
    std::memcpy(previous, ciphertext + offset, 16);
    std::memset(plaintext, 0, sizeof(plaintext));
  }
  output.swap(staged);
  return AesCbcStatus::Ok;
}
} // namespace wmbus_pure
