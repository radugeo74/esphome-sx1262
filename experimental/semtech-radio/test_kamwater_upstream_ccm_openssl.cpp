// Host-only regression using ORIGINAL PUBLIC kamwater.xmq fixtures #9/#10.
// No private meter capture. Fixture #9 has a corrupted authentication tag;
// #10 authenticates and must exactly match original clear application bytes.
// The decoder must never emit measurements from failed CCM authentication.
#include "host_only/oms_tpl_ccm_openssl.h"
#include "host_only/kamwater_numeric_semantics.h"
#include "host_only/kamwater_authenticated_tpl_status.h"
#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> from_hex(const char *p) {
  std::vector<uint8_t> result;
  for(;*p;) {
    auto digit=[](char ch) -> unsigned {
      if(ch>='0'&&ch<='9')return static_cast<unsigned>(ch-'0');
      if(ch>='a'&&ch<='f')return static_cast<unsigned>(ch-'a'+10);
      if(ch>='A'&&ch<='F')return static_cast<unsigned>(ch-'A'+10);
      assert(false);return 0;
    };
    const unsigned high=digit(*p++);
    assert(*p);
    result.push_back(static_cast<uint8_t>(high*16+digit(*p++)));
  }
  return result;
}
int main() {
  using xmq_portable::oms_ccm::decode_short_tpl;
  using CcmStatus=xmq_portable::oms_ccm::Status;
  const uint8_t manufacturer[]={0x37,0x2C};
  const uint8_t id[]={0x06,0x03,0x82,0x53};
  // Public upstream sample key from kamwater.xmq, not user data.
  const uint8_t public_fixture_key[]={
    0xBF,0x90,0x2D,0xD6,0xDF,0xAD,0xAF,0xE8,
    0x3E,0x4A,0xE6,0x83,0x2B,0x5F,0xA1,0x4C
  };
  auto invalid=from_hex(
    "7a3010ff2a1001dc2e0d00cf1a85672e138d6c0af61338403fdeb528"
    "ab264e2c61e41b06073550aa296ae9815c8a7a9868ec6c624cfbbcecb3");
  auto valid=from_hex(
    "7a3010ff2a10018c570e006da7632471f42b0a3ceccd461353573ed0"
    "2b87118bc5ffe982f7f00a0274543bc51938f8b9c8bb8928114d60788a");
  const auto expected=from_hex(
    "046d1c2a2f3c04150000000002ff25010406ff1bffffffffffff"
    "446d0020213c441500000000");
  xmq_portable::oms_ccm::Output out{};
  assert(decode_short_tpl(manufacturer,id,0x3C,0x16,
                          public_fixture_key,invalid.data(),invalid.size(),
                          out)==CcmStatus::BadTag);
  assert(out.clear_application.empty() && out.counter==0 && out.tpl_status==0);
  assert(decode_short_tpl(manufacturer,id,0x3C,0x16,
                          public_fixture_key,valid.data(),valid.size(),
                          out)==CcmStatus::Ok);
  assert(out.clear_application==expected);
  assert(out.counter==0x000E578Cu && out.tpl_status==0x10);
  // Only after the valid original CCM authentication/tag check above
  // may the original short TPL status byte be interpreted.
  std::string original_status{};
  using AuthStatus=xmq_portable::kamwater::authenticated_status::Status;
  using xmq_portable::kamwater::authenticated_status::decode_original_ccm_status;
  assert(decode_original_ccm_status(out.tpl_status,original_status)
         ==AuthStatus::Ok);
  assert(original_status=="TEMPORARY_ERROR"); // source #10 JSON
  original_status="stale";
  assert(decode_original_ccm_status(0x00,original_status)
         ==AuthStatus::UnsupportedStatus);
  assert(original_status.empty());
  xmq_portable::kamwater::Reading parsed{};
  assert(xmq_portable::kamwater::decode_clear_records(
             out.clear_application.data(),out.clear_application.size(),parsed)
         ==xmq_portable::kamwater::Status::Ok);
  assert(std::fabs(parsed.total_m3)<1e-9);
  assert(parsed.has_target && std::fabs(parsed.target_m3)<1e-9);
  // The original #10 JSON also exposes a six-byte opaque diagnostic.
  // Decode it only from the authenticated application; do not interpret
  // FFFFFFFFFFFF as a numeric temperature or volume reading.
  xmq_portable::kamwater::numeric_semantics::Reading diagnostic{};
  assert(xmq_portable::kamwater::numeric_semantics::decode(
      out.clear_application.data(),out.clear_application.size(),diagnostic)
      ==xmq_portable::kamwater::numeric_semantics::Status::Ok);
  assert(diagnostic.has_acoustic_noise &&
         diagnostic.acoustic_noise=="FFFFFFFFFFFF");
  // Correct original tag is insufficient with wrong configured identity.
  auto wrong_id=std::vector<uint8_t>(id,id+sizeof(id));
  wrong_id[0]^=1;
  assert(decode_short_tpl(manufacturer,wrong_id.data(),0x3C,0x16,
                          public_fixture_key,valid.data(),valid.size(),
                          out)==CcmStatus::BadTag);
  assert(out.clear_application.empty());
  // A tampered original payload, tag, or AAD must never publish values.
  for(size_t index : {size_t(2),size_t(11),valid.size()-1}) {
    auto changed=valid;
    changed[index]^=1;
    assert(decode_short_tpl(manufacturer,id,0x3C,0x16,
                            public_fixture_key,changed.data(),changed.size(),
                            out)==CcmStatus::BadTag);
    assert(out.clear_application.empty());
  }
  // The key material is genuinely required (fail-closed).
  assert(decode_short_tpl(manufacturer,id,0x3C,0x16,
                          nullptr,valid.data(),valid.size(),out)
         ==CcmStatus::MissingKey);
  assert(out.clear_application.empty());
  return 0;
}
