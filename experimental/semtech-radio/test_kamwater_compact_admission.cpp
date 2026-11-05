#include "components/wmbus_native_sx126x/kamwater_compact_admission.h"
#include <cassert>
int main() {
  using namespace xmq_portable::kamwater::admission;
  Evidence e{};
  assert(check(e)==Decision::MissingIdentity);
  e.manufacturer_and_model_verified=true;
  assert(check(e)==Decision::UnverifiedFormat);
  e.compact_format_signature_verified=true;
  assert(check(e)==Decision::UnverifiedSecurity);
  e.ell_security=SecurityProof::Encrypted;
  assert(check(e)==Decision::UnverifiedSecurity);
  e.ell_security=SecurityProof::VerifiedClearApplication;
  assert(check(e)==Decision::UnsupportedProfile);
  // No valid profile is selected until evidence is independently checked.
  for(int i=0;i<9;++i) {
    e.profile_index=i;
    assert(check(e)==Decision::AcceptClearApplication);
  }
  e.profile_index=9;
  assert(check(e)==Decision::UnsupportedProfile);
  e.profile_index=-1;
  assert(check(e)==Decision::UnsupportedProfile);
  e.profile_index=3;
  e.compact_format_signature_verified=false;
  assert(check(e)==Decision::UnverifiedFormat);
  e.compact_format_signature_verified=true;
  e.ell_security=SecurityProof::Unknown;
  assert(check(e)==Decision::UnverifiedSecurity);
  // TPL mode 0 alone cannot upgrade ELL security evidence.
  e.ell_security=SecurityProof::Encrypted;
  assert(check(e)==Decision::UnverifiedSecurity);
  return 0;
}
