#pragma once
// Host-only Kamwater compact admission contract. This does NOT validate an
// OMS ELL MAC, decrypt ELL payloads or calculate compact format signatures.
// A future trusted RF stage must establish all proofs independently.
// Never infer proof from length, volume plausibility, TPL mode 0 or NOKEY.
#include <cstddef>
#include <cstdint>
namespace xmq_portable { namespace kamwater { namespace admission {
enum class Decision : uint8_t {
  AcceptClearApplication, MissingIdentity, UnverifiedFormat,
  UnverifiedSecurity, UnsupportedProfile
};
enum class SecurityProof : uint8_t {
  Unknown, Encrypted, VerifiedClearApplication
};
struct Evidence {
  bool manufacturer_and_model_verified{false};
  bool compact_format_signature_verified{false};
  SecurityProof ell_security{SecurityProof::Unknown};
  // Must come from independently verified compact format metadata, not
  // guessed from packet length or candidate decoder output.
  int profile_index{-1};
};
inline Decision check(const Evidence &e) {
  if (!e.manufacturer_and_model_verified)
    return Decision::MissingIdentity;
  if (!e.compact_format_signature_verified)
    return Decision::UnverifiedFormat;
  if (e.ell_security!=SecurityProof::VerifiedClearApplication)
    return Decision::UnverifiedSecurity;
  if (e.profile_index<0 || e.profile_index>8)
    return Decision::UnsupportedProfile;
  return Decision::AcceptClearApplication;
}
}}} // namespace xmq_portable::kamwater::admission
