#include "../../../components/wmbus_radio/pure_mode5_prepare.h"
#include <cassert>
#include <cstring>
#include <vector>
using namespace wmbus_pure;
int main() {
  std::vector<uint8_t> f(11+4+32+3,0);
  f[0]=static_cast<uint8_t>(f.size()-1);
  f[1]=0x44; f[2]=0x68; f[3]=0x50;
  f[4]=0x67; f[5]=0x30; f[6]=0x27; f[7]=0x24;
  f[8]=0x69; f[9]=0x80; f[10]=0x7A;
  f[11]=0x2A; f[12]=0; f[13]=0x20; f[14]=0x05;
  Header h{};
  assert(parse_header(f.data(),f.size(),h)==HeaderStatus::Ok);
  TransportEnvelope t{};
  assert(inspect_transport(h,t)==TransportStatus::Ok);
  Mode5Prepared p{};
  assert(prepare_mode5(f.data(),f.size(),h,t,p)==Mode5PrepareStatus::Ok);
  const uint8_t expected[16]={0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,
    0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A};
  assert(std::memcmp(p.iv,expected,16)==0);
  assert(p.encrypted==f.data()+15 && p.encrypted_size==32);
  assert(p.unencrypted_tail_size==3);
  t.configuration=0x0500;
  assert(prepare_mode5(f.data(),f.size(),h,t,p)==Mode5PrepareStatus::InvalidBlockCount);
  assert(p.encrypted==nullptr);
  t.configuration=0x0530;
  assert(prepare_mode5(f.data(),f.size(),h,t,p)==Mode5PrepareStatus::TruncatedPayload);
  t.configuration=0x0700;
  assert(prepare_mode5(f.data(),f.size(),h,t,p)==Mode5PrepareStatus::UnsupportedSecurity);
  // Long TPL: an independent transport address may differ from the DLL address.
  std::vector<uint8_t> lg(11+12+16,0);
  lg[0]=static_cast<uint8_t>(lg.size()-1);lg[1]=0x44;
  std::memcpy(lg.data()+2,f.data()+2,8);lg[10]=0x72;
  uint8_t tpl[]{0x11,0x22,0x33,0x44,0x68,0x50,0x69,0x80,
                 0x3B,0x00,0x10,0x05};
  std::memcpy(lg.data()+11,tpl,sizeof(tpl));
  assert(parse_header(lg.data(),lg.size(),h)==HeaderStatus::Ok);
  assert(inspect_transport(h,t)==TransportStatus::Ok);
  assert(prepare_mode5(lg.data(),lg.size(),h,t,p)==Mode5PrepareStatus::Ok);
  const uint8_t expected_long[16]={0x68,0x50,0x11,0x22,0x33,0x44,0x69,0x80,
    0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B,0x3B};
  assert(std::memcmp(p.iv,expected_long,16)==0);
  assert(p.encrypted==lg.data()+23 && p.encrypted_size==16);
}
