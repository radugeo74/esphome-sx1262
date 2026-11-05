#pragma once
// HOST ONLY: strict integrated ingress for two known Kamwater transport shapes.
// All outputs are transactional. Never used by ESPHome radio or publishing.
// Meter identity/CRC are input filters, NOT cryptographic authentication.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "kamwater_verified_dll_ingress.h"
#include "kamwater_ell_ctr_openssl.h"
#include "oms_tpl_ccm_openssl.h"
namespace xmq_portable { namespace kamwater { namespace secure_ingress {
enum class Status : uint8_t {
  Ok, DllRejected, IdentityMismatch, UnsupportedTransport,
  CryptoRejected, ApplicationRejected
};
struct Output {
  Reading reading{};
  int profile_index{-1};
};
inline Status decode(const uint8_t *raw,size_t raw_length,
                     wmbus_pure::CrcFrameFormat format,
                     const ingress::ExpectedMeter &configured,
                     const uint8_t key[16],Output &out) {
  out={};
  wmbus_pure::CompleteFrame verified{};
  if(wmbus_pure::verify_complete_frame(raw,raw_length,format,verified)!=
     wmbus_pure::CompleteStatus::Ok)
    return Status::DllRejected;
  if(!ingress::matches_expected(verified.header,configured))
    return Status::IdentityMismatch;
  if(!key) return Status::CryptoRejected;
  const uint8_t *frame=verified.bytes.data();
  const size_t len=verified.bytes.size();
  if(len<14) return Status::UnsupportedTransport;
  Reading measured{};
  int profile=-1;
  if(verified.header.ci==0x8D) {
    // ELL II AES-CTR: encrypted payload CRC and application begin at index 17.
    std::vector<uint8_t> tpl{};
    if(ell_ctr::decrypt_ell_ii(frame+2,frame+4,key,
         frame+10,len-10,tpl)!=ell_ctr::DecryptStatus::Ok)
      return Status::CryptoRejected;
    if(tpl.empty()) return Status::ApplicationRejected;
    if(tpl[0]==0x79) {
      uint8_t expanded[160]{};
      tpl79::Reading compact{};
      if(tpl79::verify(tpl.data(),tpl.size(),expanded,sizeof(expanded),compact)!=
         tpl79::Status::Ok) return Status::ApplicationRejected;
      if(decode_clear_records(expanded,compact.expanded_size,measured)!=
         kamwater::Status::Ok) return Status::ApplicationRejected;
      profile=compact.profile_index;
    } else if(tpl[0]==0x78) {
      if(decode_clear_records(tpl.data()+1,tpl.size()-1,measured)!=
         kamwater::Status::Ok) return Status::ApplicationRejected;
    } else return Status::UnsupportedTransport;
  } else if(verified.header.ci==0x8C) {
    // ELL I header: CI, CC, ACC. The following TPL 7A uses short form.
    if(len<14 || frame[13]!=0x7A) return Status::UnsupportedTransport;
    oms_ccm::Output clear{};
    if(oms_ccm::decode_short_tpl(frame+2,frame+4,frame[8],frame[9],
         key,frame+13,len-13,clear)!=oms_ccm::Status::Ok)
      return Status::CryptoRejected;
    if(decode_clear_records(clear.clear_application.data(),
                            clear.clear_application.size(),measured)!=
       kamwater::Status::Ok) return Status::ApplicationRejected;
  } else return Status::UnsupportedTransport;
  out.reading=measured;
  out.profile_index=profile;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::secure_ingress
