#pragma once
// Stage A: read-only capture of a payload from the EXISTING decoder's
// successfully parsed/decrypted Telegram. The caller has already verified
// DLL CRC before the meter callback and must pass handleTelegram's bool
// return value, NOT just id_match (which can be true after parse failure).
// Not connected to the ESPHome callback, does not decrypt, modify the frame,
// update meter state or log any private telegram bytes.
#include "xmq_frame_bridge.h"
#include <cstdint>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>
#include <utility>

namespace xmq_runtime {

struct VerifiedApplication {
  Mvt inner_mvt;
  ApplicationEncoding encoding=ApplicationEncoding::StandardDv;
  std::vector<uint8_t> payload;
};

inline std::string manufacturer_code_to_flag(unsigned code) {
  if (code>0x7fff) return "";
  std::string result;
  for (unsigned shift : {10u,5u,0u}) {
    unsigned n=(code>>shift)&31u;
    if (n<1 || n>26) return "";
    result.push_back(static_cast<char>('A'+n-1));
  }
  return result;
}

inline bool numeric_mfct_matches(const std::string &s, unsigned code) noexcept {
  if (s.size()!=3 || code>0x7fff) return false;
  unsigned result=0;
  for (size_t i=0;i<3;++i) {
    const unsigned char c=static_cast<unsigned char>(s[i]);
    if (c<'A' || c>'Z') return false;
    const unsigned n=c-'A'+1;
    result|= n << (10-5*i);
  }
  return result==code;
}

inline bool encoding_matches_ci(ApplicationEncoding encoding,int tpl_ci) noexcept {
  switch (encoding) {
    // Only known source fixtures/CI variants in Stage A. A future adapter
    // must explicitly extend this list and test the actual payload semantics.
    case ApplicationEncoding::OriginalIxml:
      return tpl_ci==0xA0 || tpl_ci==0xA2;
    case ApplicationEncoding::StandardDv:
      return tpl_ci==0x72 || tpl_ci==0x7A;
    default:
      return false;
  }
}

// Minimal Telegram concept matches upstream Telegram::frame/header_size/
// suffix_size/addresses/tpl_ci/decryption_failed/discard. Template prevents
// pulling live wmbus_common into the isolated host and ESP32-S3 smoke builds.
// Returned false leaves destination empty. This must only be invoked after
// the existing decoder completed successfully on the very same frame.
template<typename TelegramT>
bool capture_verified_payload(const TelegramT &telegram,
                              bool handle_telegram_success,
                              ApplicationEncoding encoding,
                              VerifiedApplication &destination) noexcept {
  destination=VerifiedApplication{};
  if (!handle_telegram_success || telegram.decryption_failed ||
      telegram.discard || telegram.addresses.empty() ||
      !encoding_matches_ci(encoding,telegram.tpl_ci))
    return false;

  const auto &inner=telegram.addresses.back();
  const int header=telegram.header_size;
  const int suffix=telegram.suffix_size;
  const size_t length=telegram.frame.size();
  if (header<=0 || suffix<0 ||
      static_cast<size_t>(header)>length ||
      static_cast<size_t>(suffix)>length-static_cast<size_t>(header))
    return false;
  const size_t payload_length=length-static_cast<size_t>(header)
                                   -static_cast<size_t>(suffix);
  if (payload_length==0 || payload_length>1024)
    return false;

  // DLL address may be a converter. Only the last/inner address supplied
  // by upstream's successful full parse may select the XMQ driver.
  try {
    const std::string maker=manufacturer_code_to_flag(inner.mfct);
    if (maker.empty()) return false;
    VerifiedApplication result;
    result.inner_mvt={maker,static_cast<uint8_t>(inner.version),
                           static_cast<uint8_t>(inner.type)};
    result.encoding=encoding;
    result.payload.assign(telegram.frame.begin()+header,
                          telegram.frame.end()-suffix);
    destination=std::move(result);
    return true;
  } catch (...) {
    destination=VerifiedApplication{};
    return false;
  }
}

} // namespace xmq_runtime
