#pragma once
// Host-only, clear ELL II + compact TPL79 end-to-end validation.
// Accepts ELL bytes only AFTER a separate caller verified DLL CRC and
// verified the meter model/identity against trusted configuration.
// Never wired to native RF, decryption, MQTT or sensor publication.
#include <cstddef>
#include <cstdint>
#include "kamwater_ell_ii_probe.h"
#include "kamwater_tpl79_probe.h"
#include "kamwater_clear_records.h"
#include "kamwater_compact_admission.h"
namespace xmq_portable { namespace kamwater { namespace pipeline {
enum class Status : uint8_t {
  Ok, UnverifiedDll, UnverifiedIdentity, EllRejected,
  CompactRejected, AdmissionRejected, RecordsRejected
};
struct Preconditions {
  bool dll_crc_verified{false};
  bool meter_identity_verified{false};
};
struct Output {
  Reading reading{};
  int profile_index{-1};
};
inline Status decode_clear_ell_ii_tpl79(const uint8_t *ell_bytes,
                                         size_t length,
                                         const Preconditions &preconditions,
                                         Output &out) {
  out={};
  if (!preconditions.dll_crc_verified) return Status::UnverifiedDll;
  if (!preconditions.meter_identity_verified) return Status::UnverifiedIdentity;
  ell::ClearPayload clear{};
  if (ell::inspect_ell_ii(ell_bytes,length,clear)!=ell::Status::ClearAndCrcValid)
    return Status::EllRejected;
  uint8_t expanded[128]{};
  tpl79::Reading compact_result{};
  if (tpl79::verify(clear.tpl,clear.tpl_size,expanded,sizeof(expanded),
                    compact_result)!=tpl79::Status::Ok)
    return Status::CompactRejected;
  admission::Evidence evidence{};
  evidence.manufacturer_and_model_verified=preconditions.meter_identity_verified;
  evidence.compact_format_signature_verified=true; // just matched in TPL79
  evidence.ell_security=admission::SecurityProof::VerifiedClearApplication;
  evidence.profile_index=compact_result.profile_index;
  if (admission::check(evidence)!=admission::Decision::AcceptClearApplication)
    return Status::AdmissionRejected;
  Reading result{};
  if (decode_clear_records(expanded,compact_result.expanded_size,result)!=kamwater::Status::Ok)
    return Status::RecordsRejected;
  out.reading=result;
  out.profile_index=compact_result.profile_index;
  return Status::Ok;
}
// Clear (no AES key) ELL-II application route for complete RF frames.
// ELL integrity MUST be checked even when the application looks like DIF/VIF.
// A decrypted upstream NOKEY fixture with a missing ELL CRC is not on-air
// evidence and cannot enter this path. Strictly reject ELL session mode 1.
// Reuse the reviewed compact path; add full TPL78 and mode-zero short TPL7A.
inline Status decode_clear_ell_ii(const uint8_t *ell_bytes,size_t length,
                                  const Preconditions &preconditions,
                                  Output &out) {
  out={};
  if(!preconditions.dll_crc_verified) return Status::UnverifiedDll;
  if(!preconditions.meter_identity_verified) return Status::UnverifiedIdentity;
  ell::ClearPayload clear{};
  if(ell::inspect_ell_ii(ell_bytes,length,clear)!=
      ell::Status::ClearAndCrcValid)
    return Status::EllRejected;
  if(clear.tpl[0]==0x79)
    return decode_clear_ell_ii_tpl79(ell_bytes,length,preconditions,out);
  const uint8_t *records=nullptr;
  size_t record_size=0;
  if(clear.tpl[0]==0x78 && clear.tpl_size>1) {
    records=clear.tpl+1;
    record_size=clear.tpl_size-1;
  } else if(clear.tpl[0]==0x7A && clear.tpl_size>5) {
    // Short TPL: CI, ACC, status, 16-bit LE configuration.
    const uint16_t config=static_cast<uint16_t>(clear.tpl[3]) |
        static_cast<uint16_t>(clear.tpl[4])<<8;
    if(((config>>8)&15u)!=0u) return Status::EllRejected;
    records=clear.tpl+5;
    record_size=clear.tpl_size-5;
  } else return Status::CompactRejected;
  Reading reading{};
  if(kamwater::decode_clear_records(records,record_size,reading)!=
     kamwater::Status::Ok)
    return Status::RecordsRejected;
  out.reading=reading;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::pipeline
