#include "xmq_tpl5_cbc.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace xmq_runtime;
static void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}

int main() {
  try {
    // NIST SP 800-38A F.2.1 independent AES-128-CBC known-answer vector.
    const std::array<std::uint8_t, 16> key = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    const std::array<std::uint8_t, 16> iv = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    std::vector<std::uint8_t> cipher = {
        0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,
        0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
    const std::vector<std::uint8_t> expected = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
    std::vector<std::uint8_t> core_result(16);
    AES_CBC_decrypt_buffer(core_result.data(), cipher.data(), 16,
                           key.data(), iv.data());
    require(core_result == expected, "AES-CBC NIST vector failed");
    // Known-answer ciphertext lacks TPL5 sentinel, and must NOT pass
    // the TPL5 framing check even though AES itself was correct.
    const auto rejected = decrypt_tpl5_cbc_blocks(cipher, key, iv);
    require(!rejected.accepted && rejected.plaintext.empty(),
            "plaintext lacking TPL5 check accepted");

    // Synthetic TPL5 payload: verify successful decryption, same-value
    // repetitions, and rejection after corrupting IV/key/ciphertext.
    std::vector<std::uint8_t> clear(32, 0x00);
    clear[0] = 0x2F;
    clear[1] = 0x2F;
    clear[2] = 0x11;
    clear[3] = 0x22;
    clear[16] = 0x33;
    clear[31] = 0x44;
    std::vector<std::uint8_t> encrypted(32);
    auto mutable_input = clear;  // tiny-AES CBC encryption XORs input in place
    AES_CBC_encrypt_buffer(encrypted.data(), mutable_input.data(),
                           static_cast<std::uint32_t>(mutable_input.size()),
                           key.data(), iv.data());
    const auto first = decrypt_tpl5_cbc_blocks(encrypted, key, iv);
    require(first.accepted && first.plaintext == clear,
            "valid synthetic TPL5 check bytes rejected");
    const auto again = decrypt_tpl5_cbc_blocks(encrypted, key, iv);
    require(again.accepted && again.plaintext == first.plaintext,
            "CBC state leaked between calls");
    auto wrong_iv = iv;
    wrong_iv[0] ^= 0x01;
    const auto bad_iv = decrypt_tpl5_cbc_blocks(encrypted, key, wrong_iv);
    require(!bad_iv.accepted && bad_iv.plaintext.empty(),
            "invalid IV accepted");
    auto wrong_key = key;
    wrong_key[0] ^= 0x01;
    const auto bad_key = decrypt_tpl5_cbc_blocks(encrypted, wrong_key, iv);
    require(!bad_key.accepted && bad_key.plaintext.empty(),
            "invalid key accepted");

    for (std::size_t len : {std::size_t{0}, std::size_t{1},
                            std::size_t{15}, std::size_t{17},
                            std::size_t{513}}) {
      const auto malformed = decrypt_tpl5_cbc_blocks(
          std::vector<std::uint8_t>(len, 0), key, iv);
      require(!malformed.accepted && malformed.plaintext.empty(),
              "misaligned or oversized AES input accepted");
    }
    require(!decrypt_tpl5_cbc_blocks(cipher, key, iv).accepted,
            "failed decrypt reused last successful plaintext");
    std::cout << "XMQ_TPL5_AES_HOST_OK: NIST CBC vector, TPL5 check, "
                 "replay isolation, wrong key/IV and block bounds\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
