#pragma once
// Narrow Kamwater status mapping backed by the original public
// kamwater.xmq CCM #10 fixture. Caller MUST authenticate the complete
// encrypted TPL and verify the configured DLL identity before invoking.
// Shared status mapping for the narrowly validated CCM #10 profile.
// No generic TPL status inference, RF admission, or publishing.
#include <cstdint>
#include <string>
namespace xmq_portable { namespace kamwater { namespace authenticated_status {
enum class Status : uint8_t { Ok, UnsupportedStatus };
inline Status decode_original_ccm_status(uint8_t authenticated_tpl_status,
                                        std::string &out) {
  out.clear();
  // Public upstream Kamwater #10 has TPL status 0x10 and expects
  // TEMPORARY_ERROR. Do NOT generalize unverified bit combinations.
  if (authenticated_tpl_status != 0x10) return Status::UnsupportedStatus;
  out="TEMPORARY_ERROR";
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::authenticated_status
