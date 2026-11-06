#pragma once
// PUBLIC wmbusmeters kamwater.xmq #9/#10 host-to-ESP-IDF parity fixtures.
// These are public source tests, not local meter IDs, captures or secrets.
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace xmq_portable { namespace oms_ccm_idf_probe { namespace fixtures {
inline bool decode_hex(const char *s,uint8_t *dst,size_t len) {
  if (!s || !dst || std::strlen(s)!=2*len) return false;
  auto digit=[](char c)->int {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
  };
  for(size_t i=0;i<len;++i) {
    const int hi=digit(s[2*i]),lo=digit(s[2*i+1]);
    if(hi<0||lo<0) return false;
    dst[i]=static_cast<uint8_t>(hi*16+lo);
  }
  return true;
}
static constexpr char valid_tpl[] =
    "7a3010ff2a10018c570e006da7632471"
    "f42b0a3ceccd461353573ed02b87118b"
    "c5ffe982f7f00a0274543bc51938f8b9"
    "c8bb8928114d60788a";
static constexpr char invalid_tpl[] =
    "7a3010ff2a1001dc2e0d00cf1a85672e138d6c0af61338403fdeb528"
    "ab264e2c61e41b06073550aa296ae9815c8a7a9868ec6c624cfbbcecb3";
static constexpr char expected_clear[] =
    "046d1c2a2f3c04150000000002ff25010406ff1bffffffffffff"
    "446d0020213c441500000000";
}}}
