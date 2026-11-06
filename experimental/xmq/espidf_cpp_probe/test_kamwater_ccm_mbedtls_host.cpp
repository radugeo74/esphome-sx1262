// Executable HOST mbedTLS parity for the exact ESP-IDF probe implementation.
// This runs real CMAC/CCM crypto, unlike the build-only ESP-IDF app_main.
// It does NOT prove ESP32-S3 execution or authorize live RF driver admission.
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "../../../components/wmbus_radio/pure_complete_frame.h"
#include <cmath>
#include "../../semtech-radio/host_only/kamwater_authenticated_tpl_status.h"
#include "../../../components/wmbus_native_sx126x/kamwater_clear_records.h"
#include "../../../components/wmbus_native_sx126x/kamwater_numeric_semantics.h"
#include "main/oms_ccm_mbedtls_probe.h"
#include "main/kamwater_ccm_original_fixtures.h"

using xmq_portable::oms_ccm_idf_probe::Output;
using xmq_portable::oms_ccm_idf_probe::Status;
using xmq_portable::oms_ccm_idf_probe::decode_short_tpl;
using xmq_portable::oms_ccm_idf_probe::fixtures::decode_hex;

static bool cleared(const Output &out) {
  return out.clear_application.empty() && out.tpl_status == 0 &&
         out.counter == 0;
}

int main() {
  const uint8_t manufacturer[2] = {0x37, 0x2C};
  const uint8_t meter_id[4] = {0x06, 0x03, 0x82, 0x53};
  const uint8_t key[16] = {
      0xBF, 0x90, 0x2D, 0xD6, 0xDF, 0xAD, 0xAF, 0xE8,
      0x3E, 0x4A, 0xE6, 0x83, 0x2B, 0x5F, 0xA1, 0x4C};
  std::array<uint8_t, 57> valid{}, invalid{};
  std::array<uint8_t, 38> expected{};
  using namespace xmq_portable::oms_ccm_idf_probe::fixtures;
  if (!decode_hex(valid_tpl, valid.data(), valid.size()) ||
      !decode_hex(invalid_tpl, invalid.data(), invalid.size()) ||
      !decode_hex(expected_clear, expected.data(), expected.size()))
    return 1;

  Output out{};
  auto good = [&]() -> bool {
    return decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, key,
                            valid.data(), valid.size(), out) == Status::Ok &&
           out.clear_application.size() == expected.size() &&
           std::memcmp(out.clear_application.data(), expected.data(),
                       expected.size()) == 0 &&
           out.tpl_status == 0x10 && out.counter == 0x000E578Cu;
  };
  if (!good()) return 2;
  // Real mbedTLS authenticated plaintext now passes the shared native
  // Kamwater record and diagnostic decoders in the SAME executed test.
  // The status map remains the narrow reviewed original #10 subset.
  xmq_portable::kamwater::Reading measurement{};
  if (xmq_portable::kamwater::decode_clear_records(
          out.clear_application.data(),out.clear_application.size(),measurement)
      != xmq_portable::kamwater::Status::Ok ||
      std::fabs(measurement.total_m3)>1e-9 ||
      !measurement.has_target || std::fabs(measurement.target_m3)>1e-9)
    return 16;
  xmq_portable::kamwater::numeric_semantics::Reading diagnostic{};
  if (xmq_portable::kamwater::numeric_semantics::decode(
          out.clear_application.data(),out.clear_application.size(),diagnostic)
      != xmq_portable::kamwater::numeric_semantics::Status::Ok ||
      !diagnostic.has_acoustic_noise ||
      diagnostic.acoustic_noise!="FFFFFFFFFFFF")
    return 17;
  std::string status_text{};
  if (xmq_portable::kamwater::authenticated_status::decode_original_ccm_status(
          out.tpl_status,status_text)
      != xmq_portable::kamwater::authenticated_status::Status::Ok ||
      status_text!="TEMPORARY_ERROR")
    return 18;
  // Compose the actual shared complete-DLL verifier, a configured
  // four-field meter identity, and the ESP-IDF probe's mbedTLS decryptor.
  // The format-B DLL envelope is SYNTHETIC: original upstream encrypted
  // TPL #9/#10 is used unchanged, not a captured RF telegram.
  auto envelop = [](const std::array<uint8_t,57> &tpl) {
    std::vector<uint8_t> frame={
        0,0x44,0x37,0x2C,0x06,0x03,0x82,0x53,0x3C,0x16,
        0x8C,0x20,0x30};
    frame.insert(frame.end(),tpl.begin(),tpl.end());
    frame[0]=static_cast<uint8_t>(frame.size()+1);
    const uint16_t crc=wmbus_pure::crc16_en13757(
        frame.data(),frame.size());
    frame.push_back(static_cast<uint8_t>(crc>>8));
    frame.push_back(static_cast<uint8_t>(crc));
    return frame;
  };
  const auto full_valid=envelop(valid);
  const auto full_invalid=envelop(invalid);
  auto renew_crc = [](std::vector<uint8_t> &frame) {
    const uint16_t crc=wmbus_pure::crc16_en13757(
        frame.data(),frame.size()-2);
    frame[frame.size()-2]=static_cast<uint8_t>(crc>>8);
    frame.back()=static_cast<uint8_t>(crc);
  };
  Output admitted{};
  const auto ingress = [&](const std::vector<uint8_t> &frame) {
    admitted={};
    wmbus_pure::CompleteFrame verified{};
    if(wmbus_pure::verify_complete_frame(
           frame.data(),frame.size(),wmbus_pure::CrcFrameFormat::B,
           verified)!=wmbus_pure::CompleteStatus::Ok)
      return false;
    const auto &h=verified.header;
    if(h.manufacturer!=0x2C37 || h.meter_id!=53820306u ||
       h.version!=0x3C || h.device_type!=0x16 || h.ci!=0x8C ||
       verified.bytes.size()<14 || verified.bytes[11]!=0x20 ||
       verified.bytes[12]!=0x30 || verified.bytes[13]!=0x7A)
      return false;
    const auto &b=verified.bytes;
    return decode_short_tpl(b.data()+2,b.data()+4,h.version,
                            h.device_type,key,b.data()+13,
                            b.size()-13,admitted)==Status::Ok;
  };
  if(!ingress(full_valid) ||
     admitted.clear_application.size()!=expected.size() ||
     std::memcmp(admitted.clear_application.data(),expected.data(),
                 expected.size())!=0 ||
     admitted.tpl_status!=0x10 || admitted.counter!=0x000E578Cu)
    return 19;
  // Admission must leave no stale measurements even after a good frame.
  auto damaged=full_valid;
  damaged[5]^=1; // CRC corrupted
  if(ingress(damaged) || !cleared(admitted)) return 20;
  damaged=full_valid;
  damaged[5]^=1;renew_crc(damaged); // valid CRC, wrong configured ID
  if(ingress(damaged) || !cleared(admitted)) return 21;
  damaged=full_valid;
  damaged[10]=0x8D;renew_crc(damaged); // valid DLL, unsupported ELL CI
  if(ingress(damaged) || !cleared(admitted)) return 22;
  if(ingress(full_invalid) || !cleared(admitted)) return 23;
  if(!ingress(full_valid) || admitted.clear_application!=
       std::vector<uint8_t>(expected.begin(),expected.end())) return 24;
  // A later bad tag must clear the authenticated transport output,
  // never leaving a buffer that downstream field decoding can consume.
  // The original #9 has a corrupt CCM tag; never publish its JSON readings.
  if (decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, key,
                       invalid.data(), invalid.size(), out) != Status::BadTag ||
      !cleared(out)) return 3;

  uint8_t wrong_id[4];
  std::memcpy(wrong_id, meter_id, sizeof(wrong_id));
  wrong_id[0] ^= 1;
  if (!good() ||
      decode_short_tpl(manufacturer, wrong_id, 0x3C, 0x16, key,
                       valid.data(), valid.size(), out) != Status::BadTag ||
      !cleared(out)) return 4;

  uint8_t wrong_key[16];
  std::memcpy(wrong_key, key, sizeof(wrong_key));
  wrong_key[0] ^= 1;
  if (!good() ||
      decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, wrong_key,
                       valid.data(), valid.size(), out) != Status::BadTag ||
      !cleared(out)) return 5;

  auto tampered = valid;
  tampered.back() ^= 1;
  if (!good() ||
      decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, key,
                       tampered.data(), tampered.size(), out) != Status::BadTag ||
      !cleared(out)) return 6;

  // Malformed/unsupported TPL headers must fail closed after a valid
  // decode, without preserving application data or transport metadata.
  const auto reject = [&](const uint8_t *data, size_t size,
                          const uint8_t *secret, Status expected_status) {
    return good() &&
           decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, secret,
                            data, size, out) == expected_status &&
           cleared(out);
  };
  auto altered = valid;
  altered[0] = 0x78;
  if (!reject(altered.data(), altered.size(), key, Status::UnsupportedCi))
    return 7;
  altered = valid;
  altered[4] = 0x00;  // wrong security mode
  if (!reject(altered.data(), altered.size(), key, Status::UnsupportedMode))
    return 8;
  altered = valid;
  altered[3] = 0x00;  // partial encryption unsupported
  if (!reject(altered.data(), altered.size(), key, Status::UnsupportedPartial))
    return 9;
  altered = valid;
  altered[5] = 0x00;  // no reviewed KDF1
  if (!reject(altered.data(), altered.size(), key, Status::UnsupportedKdf))
    return 10;
  if (!reject(valid.data(), valid.size(), nullptr, Status::MissingKey))
    return 11;
  if (!reject(valid.data(), 8, key, Status::Invalid))
    return 12;
  if (!reject(valid.data(), 15, key, Status::Invalid))
    return 13;
  altered = valid;
  altered[6] = 0x03;  // 16-byte tag cannot fit in shortened telegram
  if (!reject(altered.data(), 16, key, Status::Invalid))
    return 14;
  // Null input must never disclose a previous successful decode.
  if (!good() ||
      decode_short_tpl(manufacturer, meter_id, 0x3C, 0x16, key,
                       nullptr, 0, out) != Status::Invalid || !cleared(out))
    return 15;

  std::cout << "PASS host mbedTLS: original Kamwater #10 exact authenticated "
               "38-byte payload; #9 bad tag and wrong identity/key/tag "
               "reject with no stale outputs; four source fields decoded "
               "from authenticated mbedTLS plaintext. NOT ESP32-S3 runtime.\\n";
  return 0;
}
