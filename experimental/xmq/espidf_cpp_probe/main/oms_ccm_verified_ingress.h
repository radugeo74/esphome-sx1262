#pragma once
// Experimental host/ESP-IDF probe composition, NOT a live RF decoder.
// Full format-B DLL verification and independent configured meter identity
// precede the narrow authenticated OMS short-TPL CCM implementation.
// No Home Assistant publication, key persistence, or replay acceptance.
#include <cstddef>
#include <cstdint>
#include "../../legacy_reference/pure_complete_frame.h"
#include "oms_ccm_mbedtls_probe.h"

namespace xmq_portable { namespace oms_ccm_idf_probe { namespace ingress {
struct ConfiguredMeter {
  uint16_t manufacturer{0};
  uint32_t id{0};
  uint8_t version{0};
  uint8_t medium{0};
};
enum class Status : uint8_t {
  Ok, InvalidDll, IdentityMismatch, UnsupportedEnvelope, CryptoRejected
};
inline Status decode_format_b(const uint8_t *raw, size_t length,
                              const ConfiguredMeter &configured,
                              const uint8_t key[16], Output &out) {
  out={};
  wmbus_pure::CompleteFrame frame{};
  if (wmbus_pure::verify_complete_frame(
          raw,length,wmbus_pure::CrcFrameFormat::B,frame)
      != wmbus_pure::CompleteStatus::Ok)
    return Status::InvalidDll;
  const auto &header=frame.header;
  if (header.manufacturer!=configured.manufacturer ||
      header.meter_id!=configured.id ||
      header.version!=configured.version ||
      header.device_type!=configured.medium)
    return Status::IdentityMismatch;
  const auto &bytes=frame.bytes;
  if (header.ci!=0x8C || bytes.size()<14 ||
      bytes[11]!=0x20 || bytes[12]!=0x30 || bytes[13]!=0x7A)
    return Status::UnsupportedEnvelope;
  // Decryption failure clears out in decode_short_tpl; no partial output.
  if (decode_short_tpl(bytes.data()+2,bytes.data()+4,
                       header.version,header.device_type,key,
                       bytes.data()+13,bytes.size()-13,out)
      != oms_ccm_idf_probe::Status::Ok)
    return Status::CryptoRejected;
  return Status::Ok;
}
}}} // namespace xmq_portable::oms_ccm_idf_probe::ingress
