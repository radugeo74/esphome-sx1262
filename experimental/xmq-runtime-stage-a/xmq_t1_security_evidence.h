#pragma once
// Stage 5 read-only independent T1 envelope classification.
// A2 is proprietary: these bytes do NOT establish cryptographic security
// status. Classification is *structural evidence*, not proof of cleartext.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xmq_runtime {

enum class T1EnvelopeEvidence : uint8_t {
  Reject = 0,
  ProprietaryMk4Candidate = 1,
  UnknownSecurity = 2
};

inline T1EnvelopeEvidence inspect_t1_envelope_without_legacy(
    const std::vector<uint8_t> &dll, bool crc_verified,
    bool t1_link_mode) noexcept {
  if (!crc_verified || !t1_link_mode || dll.size() < 16 ||
      dll.size() > 256 || static_cast<size_t>(dll[0]) + 1 != dll.size())
    return T1EnvelopeEvidence::Reject;
  // DLL: L,C,M,ID[4],version,type,CI. Restrict to known TCH MK4 MVT.
  if (dll[1] != 0x44 || dll[2] != 0x68 || dll[3] != 0x50 ||
      (dll[8] != 0x95 && dll[8] != 0x70) ||
      (dll[9] != 0x62 && dll[9] != 0x72))
    return T1EnvelopeEvidence::Reject;
  if (dll[10] != 0xA2)
    return T1EnvelopeEvidence::UnknownSecurity;
  // Proprietary envelope in observed T1 clear-form telegrams.
  // The pattern itself is not a standard authenticated security field.
  if (dll[11] == 0x06 &&
      ((dll[12] == 0x3e && dll[13] == 0x35) ||
       (dll[12] == 0x70 && dll[13] == 0x29)))
    return T1EnvelopeEvidence::ProprietaryMk4Candidate;
  return T1EnvelopeEvidence::UnknownSecurity;
}
}  // namespace xmq_runtime
