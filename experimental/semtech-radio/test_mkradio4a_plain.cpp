#include "../../components/wmbus_native_sx126x/mkradio4a_plain.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
static std::vector<uint8_t> from_hex(const char *hex) {
  std::vector<uint8_t> out;
  const auto digit=[](char c)->uint8_t {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='A'&&c<='F') return c-'A'+10;
    assert(false); return 0;
  };
  while (*hex) {const uint8_t hi=digit(*hex++); const uint8_t lo=digit(*hex++); out.push_back((hi<<4)|lo);}
  return out;
}
int main() {
  // Fixtures from upstream mkradio4a.xmq (public examples, no private meter data).
  using xmq_portable::mkradio4a::decode;
  using xmq_portable::mkradio4a::Status;
  xmq_portable::mkradio4a::Reading value{};
  auto warm=from_hex("4B44685036494600953772253895662423FE064E0030052F2F4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9");
  assert(decode(warm.data(),warm.size(),value)==Status::Ok);
  assert(value.target_m3>16.09 && value.target_m3<16.11);
  assert(value.year==2021 && value.month==12 && value.day==31);
  auto cold=from_hex("4B4468508644710095377202007701A85CFE078A0030052F2F4315F00200426CBF2C0FEE456BF6F802216503E25EB73E9377D54F672681B76C469696E4C7BCCC9072CC79F712360FC3F57D85");
  assert(decode(cold.data(),cold.size(),value)==Status::Ok);
  assert(value.target_m3>75.19 && value.target_m3<75.21);
  assert(value.year==2021 && value.month==12 && value.day==31);
  // Prevent a ciphertext-only or corrupted envelope from becoming a reading.
  warm[23]=0x42;
  assert(decode(warm.data(),warm.size(),value)==Status::UnsupportedEnvelope);
  assert(value.target_m3==0);
  warm[23]=0x2F; warm[26]=0x14;
  assert(decode(warm.data(),warm.size(),value)==Status::MissingFields);
  assert(decode(cold.data(),30,value)==Status::TooShort);
  // Observed C1 55-byte frames do not match the pinned 76-byte fixture.
  auto short_c1=from_hex("3644685036494600953772253895662423FE064E0030052F2F4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F11");
  assert(decode(short_c1.data(),short_c1.size(),value)==Status::TooShort);
  return 0;
}
