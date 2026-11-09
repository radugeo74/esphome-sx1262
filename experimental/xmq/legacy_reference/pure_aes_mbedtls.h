#pragma once
// ESP32/ESP-IDF AES-128 decrypt-block adapter for the pure CBC engine.
// Compiles only where mbedTLS is enabled. No key or plaintext logging.
#include <cstdint>
#include "mbedtls/aes.h"
namespace wmbus_pure {
inline bool mbedtls_aes128_decrypt_block(const uint8_t key[16],
                                        const uint8_t ciphertext[16],
                                        uint8_t plaintext[16], void *) {
  if (!key || !ciphertext || !plaintext) return false;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const int key_status = mbedtls_aes_setkey_dec(&aes, key, 128);
  const int decrypt_status = key_status == 0
      ? mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, ciphertext, plaintext)
      : key_status;
  mbedtls_aes_free(&aes);
  return decrypt_status == 0;
}
} // namespace wmbus_pure
