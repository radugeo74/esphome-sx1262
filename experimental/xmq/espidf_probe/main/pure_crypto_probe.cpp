// Cross-compiled with ESP-IDF's real mbedTLS component on ESP32-S3.
// This probe tests linkage and exercises the public NIST known-answer case.
#include <algorithm>
#include <cstdint>
#include <vector>
#include "../../../../components/wmbus_radio/pure_aes_cbc.h"
#include "../../../../components/wmbus_radio/pure_aes_mbedtls.h"

extern "C" bool pure_crypto_probe(void) {
  constexpr uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                               0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  constexpr uint8_t iv[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  constexpr uint8_t cipher[16] = {0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,
                                  0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
  constexpr uint8_t expected[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
                                    0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
  std::vector<uint8_t> plaintext;
  return wmbus_pure::decrypt_aes128_cbc(
             cipher, sizeof(cipher), key, iv,
             wmbus_pure::mbedtls_aes128_decrypt_block, nullptr, plaintext)
           == wmbus_pure::AesCbcStatus::Ok &&
         plaintext.size() == sizeof(expected) &&
         std::equal(plaintext.begin(), plaintext.end(), expected);
}
