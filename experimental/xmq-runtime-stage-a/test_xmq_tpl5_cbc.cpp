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

    // Construct mode-5 IV from on-frame manufacturer M, address A and
    // the TPL access byte exactly as wmbus_utils.cc does (8+8 bytes).
    Tpl5HeaderFields header;
    header.mfct_le = {0x34, 0x12};
    header.address_a = {0x78, 0x56, 0x34, 0x12, 0x94, 0x80};
    header.access = 0x5B;
    const auto derived_iv = make_tpl5_iv(header);
    require(derived_iv[0] == 0x34 && derived_iv[1] == 0x12 &&
            derived_iv[2] == 0x78 && derived_iv[7] == 0x80 &&
            std::all_of(derived_iv.begin() + 8, derived_iv.end(),
                        [](std::uint8_t b) { return b == 0x5B; }),
            "TPL5 M/A/ACC IV derivation does not match legacy implementation");
    const auto synth_clear = first.plaintext;
    auto synth_input = synth_clear;
    std::vector<std::uint8_t> synth_cipher(synth_input.size());
    AES_CBC_encrypt_buffer(synth_cipher.data(), synth_input.data(),
                           static_cast<std::uint32_t>(synth_input.size()),
                           key.data(), derived_iv.data());
    // Two encrypted blocks followed by a clear application tail.
    const std::vector<std::uint8_t> tail = {0xA0, 0xB1, 0xC2};
    auto full_payload = synth_cipher;
    full_payload.insert(full_payload.end(), tail.begin(), tail.end());
    auto clear_with_tail = synth_clear;
    clear_with_tail.insert(clear_with_tail.end(), tail.begin(), tail.end());
    auto complete = decrypt_tpl5_application(full_payload, key, header, 2);
    require(complete.accepted && complete.plaintext == clear_with_tail,
            "TPL5 explicit 2-block payload / clear tail corrupted");
    require(!decrypt_tpl5_application(full_payload, key, header, 0).accepted,
            "TPL5 implicit all-blocks accepted clear misaligned tail");
    require(!decrypt_tpl5_application(full_payload, key, header, 3).accepted,
            "TPL5 encrypted block count larger than frame accepted");
    require(!decrypt_tpl5_application(full_payload, key, header, 16).accepted,
            "TPL5 out-of-range encrypted block count accepted");
    auto wrong_access = header;
    wrong_access.access ^= 0x01;
    // CBC IV bytes 8..15 affect plaintext bytes 8..15, not the 2F2F
    // sentinel at offsets 0..1. Wrong ACC can therefore PASS this weak
    // framing check: never treat success as cryptographic authentication.
    const auto wrong_access_result =
        decrypt_tpl5_application(full_payload, key, wrong_access, 2);
    require(wrong_access_result.accepted &&
            wrong_access_result.plaintext != clear_with_tail,
            "TPL5 wrong access unexpectedly preserved complete plaintext");
    auto wrong_address = header;
    wrong_address.address_a[0] ^= 0x01;
    require(!decrypt_tpl5_application(full_payload, key, wrong_address, 2).accepted,
            "TPL5 wrong inner address accepted");
    const auto all_blocks = decrypt_tpl5_application(synth_cipher, key, header, 0);
    require(all_blocks.accepted && all_blocks.plaintext == synth_clear,
            "TPL5 zero block count should decrypt complete aligned payload");

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
