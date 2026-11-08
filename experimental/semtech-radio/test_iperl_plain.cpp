#include "components/wmbus_native_sx126x/iperl_plain.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> result;
  int high = -1;
  for (; *s; ++s) {
    if (*s == '_') continue;
    int v = (*s >= '0' && *s <= '9') ? *s - '0' :
            (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1;
    assert(v >= 0);
    if (high < 0) high = v;
    else { result.push_back(static_cast<uint8_t>((high << 4) | v)); high = -1; }
  }
  assert(high == -1);
  return result;
}
int main() {
  using namespace xmq_portable::iperl;
  Reading r{};
  auto a=hex("1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F");
  // Upstream #1 says "after decryption", but its CF is M=0,N=1.
  // The RF entrypoint must REJECT it; only offline clear-app parsing
  // may read its already-supplied measurement records.
  assert(decode(a.data(),a.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0 && r.max_flow_m3h==0.0);
  assert(decode_clear_application(a.data()+15,a.size()-15,r)==Status::Ok);
  assert(std::fabs(r.total_m3 - 7.704)<0.00001);
  assert(r.max_flow_m3h == 0.0 && !r.power_low);
  auto b=hex("1844AE4C4455223368077A55000000_041389E20100023B0000");
  assert(decode(b.data(),b.size(),r) == Status::Ok);
  assert(std::fabs(r.total_m3 - 123.529)<0.00001);
  assert(r.max_flow_m3h == 0.0 && !r.power_low);
  auto c=hex("1844AE4C4455223368077A55060000_041389E20100023B0000");
  assert(decode(c.data(),c.size(),r) == Status::Ok);
  assert(r.power_low && r.tpl_status == 0x06);
  // Mutations: no reporting values after a failed frame.
  c[0]=0x19;
  assert(decode(c.data(),c.size(),r) == Status::InvalidFrame);
  assert(r.total_m3==0.0);
  c=b; c[13]=0x00; c[14]=0x05; // AES mode 5 must not be interpreted as plaintext
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  c=b; c[13]=0x10; c[14]=0x00; // Mode 0 with N=1 is INVALID OMS
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0);
  c=b; c[13]=0x00; c[14]=0x10; // 5th mode bit: M=16, NOT M=0
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0);
  c=b; c[15]=0x84; // unsupported DIFE must fail closed
  assert(decode(c.data(),c.size(),r) == Status::Unsupported);
  c=b; c.resize(20); c[0]=19; // truncated records with consistent DLL length
  assert(decode(c.data(),c.size(),r) != Status::Ok);
  return 0;
}
