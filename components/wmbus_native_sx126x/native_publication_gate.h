#pragma once
// Strict admission policy for CRC-verified, CRC-stripped RF frames before
// any Home Assistant measurement or RSSI publication. No decryption, driver
// dispatch, sensor updates, or allocation.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "native_identity_probe.h"
#include "tpl_security_probe.h"

namespace wmbus_pure { namespace publication {
enum class Decision : uint8_t {
  Allowed, InvalidFrame, UnsupportedTransport, UnverifiedSecurity,
  InvalidIdentity, ConflictingIdentity
};
inline Decision inspect(const uint8_t *frame, size_t size,
                        uint32_t configured_dll_id) {
  Header header{};
  if (parse_header(frame,size,header)!=HeaderStatus::Ok ||
      header.meter_id!=configured_dll_id)
    return Decision::InvalidFrame;
  // Manufacturer-specific payload formats have their own strict
  // XMQ decoding rules. They do not contain a standard TPL header.
  if (header.ci==0xA0 || header.ci==0xA2)
    return Decision::Allowed;
  if (header.ci!=0x72 && header.ci!=0x7A)
    return Decision::UnsupportedTransport;
  const auto parsed=identity::inspect(frame,size);
  if (parsed.status!=identity::Status::Ok)
    return Decision::InvalidIdentity;
  // A long TPL header can describe a distinct meter behind a converter.
  // Do not publish anything under the DLL meter binding in that case.
  if (header.ci==0x72 &&
      (!parsed.has_tpl ||
       !identity::same_address(parsed.dll,parsed.tpl)))
    return Decision::ConflictingIdentity;
  // Fail closed for encrypted, unknown and incomplete standard TPL.
  const auto security=tpl_security::inspect(frame,size);
  if (security.state!=tpl_security::State::NotEncrypted)
    return Decision::UnverifiedSecurity;
  return Decision::Allowed;
}
}} // namespace wmbus_pure::publication
