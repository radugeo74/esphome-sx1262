#pragma once
// Passive identification ONLY. Call after independently verified DLL CRC.
// A candidate is not a decoding authorization, an AES key or an HA entity.
// Nested TPL/ELL candidates take precedence over converter DLL metadata.
// Keeps unknown/ambiguous secure-converter identities fail-closed.
#include <cstddef>
#include <cstdint>
#include "batch2_auto_identify.h"
#include "xmq_auto_dispatch.h"

namespace wmbus_pure { namespace passive_auto {

inline batch2_identify::Result inspect(const uint8_t *frame,size_t length,
                                      const DriverSignature *signatures,
                                      size_t signature_count) {
  auto result=batch2_identify::inspect(frame,length,signatures,signature_count);
  if(result.state!=batch2_identify::State::Unknown)
    return result;  // In particular: never override an ambiguous nested MVT.
  const auto identity_result=identity::inspect(frame,length);
  if(identity_result.status!=identity::Status::Ok ||
     !identity_result.dll.decimal_bcd)
    return result;
  const auto dll=auto_driver::identify(
      signatures,signature_count,identity_result.dll.manufacturer,
      identity_result.dll.version,identity_result.dll.device_type);
  if(dll.status!=auto_driver::RecognitionStatus::Candidate ||
     !dll.driver || batch2_identify::is_batch2(dll.driver))
    return result; // A TCH/95/37 converter alone is not an MK Radio4a meter.
  result.state=batch2_identify::State::Candidate;
  result.source=batch2_identify::Source::Dll;
  result.driver=dll.driver;
  result.meter_id=batch2_identify::decimal_id(identity_result.dll);
  result.decimal_bcd=true;
  result.different_dll_id=false;
  return result;
}

}}  // namespace wmbus_pure::passive_auto
