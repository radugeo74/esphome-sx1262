#include "xmq_tpl5_direct_header.h"
#include "xmq_tpl5_identity_gate.h"
#include "xmq_tpl5_live_admission.h"
#include "xmq_primary_readiness.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace xmq_runtime;
static void require(bool yes, const char *reason) {
  if (!yes) throw std::runtime_error(reason);
}
static std::string upstream(const char *name) {
  std::ifstream stream(std::string("experimental/xmq-runtime-stage-a/upstream/src/") +
                       name + ".xmq");
  if (!stream) throw std::runtime_error("missing public XMQ driver fixture");
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}
static std::vector<std::uint8_t> synthetic_tpl5_frame(
    std::uint8_t ci, const Tpl5HeaderFields &fields,
    const std::array<std::uint8_t, 16> &key,
    const std::vector<std::uint8_t> &application) {
  // 2F2F is a CBC framing sentinel. The exact payload length is known only
  // because this is an offline fixture, NOT an inference from arbitrary RF.
  std::vector<std::uint8_t> clear{0x2F,0x2F};
  clear.insert(clear.end(), application.begin(), application.end());
  clear.resize((clear.size() + 15) / 16 * 16, 0);
  auto input = clear;
  std::vector<std::uint8_t> ciphertext(clear.size());
  const auto iv = make_tpl5_iv(fields);
  AES_CBC_encrypt_buffer(ciphertext.data(), input.data(),
                         static_cast<std::uint32_t>(input.size()),
                         key.data(), iv.data());
  // Manufacturer TCH = 0x5068; M=68 50; A: ID[4], ver=95, type=62
  std::vector<std::uint8_t> frame{0,0x44,0x68,0x50,
      0x04,0x03,0x02,0x01,0x95,0x62,ci};
  if (ci == 0x72) {
    frame.insert(frame.end(),fields.address_a.begin(),
                 fields.address_a.begin()+4);
    frame.insert(frame.end(),fields.mfct_le.begin(),fields.mfct_le.end());
    frame.push_back(fields.address_a[4]);
    frame.push_back(fields.address_a[5]);
  }
  frame.push_back(fields.access);
  frame.push_back(0); // TPL status
  frame.push_back(0); // cfg low: all blocks encrypted
  frame.push_back(5); // cfg high: security mode 5
  frame.insert(frame.end(),ciphertext.begin(),ciphertext.end());
  frame[0] = static_cast<std::uint8_t>(frame.size() - 1);
  return frame;
}
int main() {
  try {
    const DriverRegistry registry({load_driver(upstream("mkradio4"))});
    const auto water = parse_hex(
        "06702901006017030004000300000000000000000000000000000000000000000000");
    const std::array<std::uint8_t,16> key{
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    const Tpl5HeaderFields fields{
        {0x68,0x50},{0x04,0x03,0x02,0x01,0x95,0x62},0x4b};

    // Fixture-only path: synthetically encrypt an original upstream clear
    // XMQ water application, then exercise DLL->TPL header->CBC->XMQ.
    // No legacy Telegram::parse; no HA sensor writes.
    XmqFrameResult last;
    for (auto ci : {std::uint8_t{0x7a}, std::uint8_t{0x72}}) {
      const auto frame = synthetic_tpl5_frame(ci, fields, key, water);
      const auto parsed = parse_direct_tpl5_header(frame, true);
      require(parsed.valid, "fixture TPL5 header rejected");
      const ExpectedTpl5Identity expected{fields.mfct_le, fields.address_a};
      const auto identity = inspect_tpl5_routing_identity(parsed, expected);
      require(identity.accepted && identity.inner_mvt.manufacturer == "TCH" &&
              identity.inner_mvt.version == 0x95 &&
              identity.inner_mvt.type == 0x62,
              "configured inner meter routing identity failed");
      // Fixture deliberately feeds proprietary mkradio4 bytes through a
      // synthetic standard OMS CI. It MUST remain an offline crypto/parsing
      // unit test, not a live transport admission or sensor writer.
      require(!tpl5_encoding_admissible(ci, ApplicationEncoding::OriginalIxml),
              "proprietary fixture admitted on standard TPL CI");
      require(tpl5_encoding_admissible(ci, ApplicationEncoding::StandardDv),
              "standard OMS CI unexpectedly rejected");
      require(!admit_direct_tpl5_xmq_candidate(
                  parsed, identity, ci, ApplicationEncoding::OriginalIxml,
                  PrimaryPayloadTrust::VerifiedCleartext),
              "synthetic proprietary fixture falsely admitted to live TPL");
      require(!admit_direct_tpl5_xmq_candidate(
                  parsed, identity, ci, ApplicationEncoding::StandardDv,
                  PrimaryPayloadTrust::Ciphertext),
              "unverified ciphertext falsely admitted to XMQ");
      require(!tpl5_cbc_sentinel_proves_verified_cleartext(),
              "CBC framing check falsely declared cryptographic proof");
      auto wrong_meter = expected;
      wrong_meter.address_a[0] ^= 1;
      require(!inspect_tpl5_routing_identity(parsed, wrong_meter).accepted,
              "different meter ID was allowed to decode");
      wrong_meter = expected;
      wrong_meter.mfct_le[0] ^= 1;
      require(!inspect_tpl5_routing_identity(parsed, wrong_meter).accepted,
              "different manufacturer was allowed to decode");
      wrong_meter = expected;
      wrong_meter.address_a[4] ^= 1;
      require(!inspect_tpl5_routing_identity(parsed, wrong_meter).accepted,
              "different meter version was allowed to decode");
      wrong_meter = expected;
      wrong_meter.address_a[5] ^= 1;
      require(!inspect_tpl5_routing_identity(parsed, wrong_meter).accepted,
              "different meter media type was allowed to decode");
      const auto decrypted = decrypt_direct_tpl5_frame(frame, true, key);
      require(decrypted.accepted && decrypted.plaintext.size() >= 2 + water.size(),
              "fixture CBC decryption rejected");
      require(decrypted.plaintext[0] == 0x2f &&
              decrypted.plaintext[1] == 0x2f,
              "TPL5 sanity prefix missing");
      const std::vector<std::uint8_t> clear(
          decrypted.plaintext.begin() + 2,
          decrypted.plaintext.begin() + 2 + water.size());
      require(clear == water, "synthetic CBC payload differed from fixture");
      require(decode_primary_candidate(
                  registry, identity.inner_mvt,
                  ApplicationEncoding::OriginalIxml, clear,
                  "mkradio4", {"target_m3","total_m3"}, {}, last),
              "decrypted fixture rejected by independent XMQ parser");
      require(last.numeric.at("total_m3").value == 0.4 &&
              last.numeric.at("target_m3").value == 0.1,
              "decrypted synthetic frame did not match upstream water readings");
      require(!decrypt_direct_tpl5_frame(frame, false, key).accepted,
              "unverified CRC admitted to decryptor");
      auto other_key = key;
      other_key[0] ^= 1;
      const auto bad = decrypt_direct_tpl5_frame(frame, true, other_key);
      require(!bad.accepted, "wrong key accepted despite 2F2F sentinel");
    }
    // This deliberately does not mark plaintext VerifiedCleartext for an
    // actual radio frame: CBC 2F2F is weak and payload length was supplied
    // externally from a public fixture.
    std::cout << "XMQ_TPL5_TO_XMQ_HOST_OK: synthetic CI7A/72 CBC to "
                 "original mkradio4 numeric fields; no live AES proof\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
