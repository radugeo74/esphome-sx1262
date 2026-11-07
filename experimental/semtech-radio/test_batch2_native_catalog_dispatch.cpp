// Stage B: exercise the ACTUAL native configured dispatch entrypoint.
// Synthetic clear CRC-stripped envelopes around public XMQ application
// bytes. These do not prove on-air Mode-5/ELL encryption support.
#include "components/wmbus_native_sx126x/xmq_decode_catalog.h"
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/kamwater_compact_signature.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

static std::vector<uint8_t> from_hex(const char *s) {
  std::vector<uint8_t> out;int first=-1;
  for(;*s;++s) {
    const char c=*s;
    const int v=c>='0'&&c<='9'?c-'0':
      c>='A'&&c<='F'?c-'A'+10:
      c>='a'&&c<='f'?c-'a'+10:-1;
    assert(v>=0);
    if(first<0) first=v;
    else {out.push_back(static_cast<uint8_t>((first<<4)|v));first=-1;}
  }
  assert(first<0);
  return out;
}
static double value(const xmq_portable::DecodedPayload &out,
                    const char *key) {
  for(size_t i=0;i<out.value_count;++i)
    if(out.values[i].name && std::strcmp(out.values[i].name,key)==0)
      return out.values[i].value;
  assert(false);
  return 0;
}
int main() {
  using Status=xmq_portable::PureDecodeStatus;
  xmq_portable::Capture captures[12]{};
  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload result{values,24,0,dates,2,0};
  const auto decode=[&](const std::vector<uint8_t> &frame,
                        const char *id,const char *driver) {
    return xmq_portable::decode_configured_plain_frame(
      frame.data(),frame.size(),id,driver,captures,12,result);
  };

  // Minomess original public clear application, represented under a
  // synthetic matching DLL+TPL identity and mode-zero long TPL.
  auto minomess=from_hex(
    "6644496A7856341218377278563412496A01067D0050052F2F"
    "0C1354031400026C4F3A82046C413A8C0413F51C0200"
    "8D0493132C33FEEE0F020D05021EFA018CED010CE0013ED401"
    "FFC601A9B701E2A2011B94014884011D7401FC6601EA5A01"
    "02FD1700002F2F");
  assert(minomess.size()==size_t(minomess[0])+1u);
  minomess[8]=0x01; minomess[9]=0x06; // match nested TPL
  minomess[22]=0x00; // MODE 0 synthetic; original is MODE 5
  assert(decode(minomess,"12345678","minomess")==Status::Ok);
  assert(std::fabs(value(result,"total_m3")-140.354)<1e-6);
  assert(std::fabs(value(result,"target_m3")-138.485)<1e-6);
  const auto plain=[&](const std::vector<uint8_t> &bytes,const char *meter,
                       const char *sender,const char *driver) {
    return xmq_portable::batch2_plain::decode_configured(
        bytes.data(),bytes.size(),meter,sender,driver,captures,12,result);
  };
  // Full generated XMQ registry: configured-only AUTO must resolve each
  // driver without promoting encrypted, unknown or ambiguous RF.
  const auto select_auto=[&](const std::vector<uint8_t> &bytes,
                            const char *meter,const char *sender,
                            const wmbus_pure::DriverSignature *signatures=
                                wmbus_pure::generated::signatures,
                            size_t count=wmbus_pure::generated::signature_count) {
    return xmq_portable::batch2_plain::select_configured_driver(
      bytes.data(),bytes.size(),meter,sender,"auto",signatures,count);
  };
  assert(select_auto(minomess,"12345678","12345678")!=nullptr);
  assert(std::strcmp(select_auto(minomess,"12345678","12345678"),
                     "minomess")==0);
  assert(select_auto(minomess,"12345679","12345678")==nullptr);
  assert(plain(minomess,"12345678","12345678","minomess")==Status::Ok);
  assert(result.value_count==2);
  // Explicit radio converter binding: DLL ID and long TPL ID are
  // independently matched. Values belong to the TPL, not the sender.
  auto converted=minomess;
  converted[4]=0x10;converted[5]=0x64;
  converted[6]=0x03;converted[7]=0x55; // DLL sender = 55036410
  assert(plain(converted,"12345678","55036410","minomess")==Status::Ok);
  assert(select_auto(converted,"12345678","55036410")!=nullptr);
  assert(std::strcmp(select_auto(converted,"12345678","55036410"),
                     "minomess")==0);
  assert(select_auto(converted,"12345678","12345678")==nullptr);
  assert(select_auto(converted,"12345679","55036410")==nullptr);

  assert(std::fabs(value(result,"total_m3")-140.354)<1e-6);
  assert(plain(converted,"12345678","12345678","minomess")!=Status::Ok);
  assert(result.value_count==0);
  assert(plain(converted,"12345679","55036410","minomess")!=Status::Ok);
  assert(result.value_count==0);
  assert(plain(converted,"12345678","55036410","auto")!=Status::Ok);
  assert(result.value_count==0);
  converted[22]=0x05; // MODE 5 from original post-decrypted NOKEY fixture
  assert(plain(converted,"12345678","55036410","minomess")!=Status::Ok);
  assert(result.value_count==0);
  // Even when AUTO recognizes the model, actual Mode-5 encrypted
  // transmission must still be rejected by keyless decode.
  assert(select_auto(converted,"12345678","55036410")!=nullptr);
  assert(plain(converted,"12345678","55036410","minomess")!=Status::Ok);
  assert(result.value_count==0);
  // Restore source TPL 0x0550 (high byte 0x05): MODE 5 must reject.
  minomess[22]=0x05;
  if(decode(minomess,"12345678","minomess")==Status::Ok) return 1;
  if(result.value_count!=0) return 2;
  minomess[22]=0x00;
  if(decode(minomess,"12345679","minomess")==Status::Ok ||
     result.value_count!=0) return 3;

  // Kamwater: genuine public compact TPL bytes, synthetic clear ELL-II
  // envelope and known KAM MULTICAL21 model identity.
  const uint8_t compact_tpl[]={
    0x79,0xED,0xA8,0x6A,0xB6,0x71,0x00,0x08,0x19,0x00,0x00,
    0x08,0x19,0x00,0x00,0x7F,0x13
  };
  std::vector<uint8_t> kamwater={
    0,0x44,0x2D,0x2C,0x78,0x56,0x34,0x12,0x1B,0x16,
    0x8D,0x20,0x01,0x03,0,0,0,0,0
  };
  kamwater.insert(kamwater.end(),compact_tpl,
                   compact_tpl+sizeof(compact_tpl));
  const uint16_t ecrc=xmq_portable::kamwater::signature::crc16_en13757(
    kamwater.data()+19,kamwater.size()-19);
  kamwater[17]=static_cast<uint8_t>(ecrc);
  kamwater[18]=static_cast<uint8_t>(ecrc>>8);
  kamwater[0]=static_cast<uint8_t>(kamwater.size()-1);
  assert(decode(kamwater,"12345678","kamwater")==Status::Ok);
  assert(std::fabs(value(result,"total_m3")-6.408)<1e-6);
  assert(std::fabs(value(result,"target_m3")-6.408)<1e-6);
  assert(plain(kamwater,"12345678","12345678","kamwater")==Status::Ok);
  assert(select_auto(kamwater,"12345678","12345678")!=nullptr);
  assert(std::strcmp(select_auto(kamwater,"12345678","12345678"),
                     "kamwater")==0);
  assert(select_auto(kamwater,"12345679","12345678")==nullptr);
  // Conflicting MVTs are not valid AUTO candidates.
  constexpr wmbus_pure::DriverSignature collision[]={
    {0x2C2D,0x1B,0x16,"kamwater"},
    {0x2C2D,0x1B,0x16,"other"}};
  assert(select_auto(kamwater,"12345678","12345678",
                     collision,2)==nullptr);

  // Clear ELL-II FULL TPL78: require DLL identity, ELL mode zero and
  // actual CRC of the full on-air application; no key needed.
  const uint8_t full_tpl[]={
      0x78,0x04,0x13,0x08,0x19,0x00,0x00,
      0x44,0x13,0x08,0x19,0x00,0x00};
  auto full_kamwater=kamwater;
  full_kamwater.resize(19);
  full_kamwater.insert(full_kamwater.end(),full_tpl,
                        full_tpl+sizeof(full_tpl));
  uint16_t full_crc=xmq_portable::kamwater::signature::crc16_en13757(
      full_kamwater.data()+19,full_kamwater.size()-19);
  full_kamwater[17]=static_cast<uint8_t>(full_crc);
  full_kamwater[18]=static_cast<uint8_t>(full_crc>>8);
  full_kamwater[0]=static_cast<uint8_t>(full_kamwater.size()-1);
  assert(plain(full_kamwater,"12345678","12345678","kamwater")==Status::Ok);
  assert(std::fabs(value(result,"total_m3")-6.408)<1e-6);
  assert(std::fabs(value(result,"target_m3")-6.408)<1e-6);
  full_kamwater[17]^=1;
  assert(plain(full_kamwater,"12345678","12345678","kamwater")!=Status::Ok);
  assert(result.value_count==0);
  // Valid MVT alone never authorizes encrypted ELL payload.
  assert(select_auto(kamwater,"12345678","12345678")!=nullptr);
  kamwater[16]=0x20; // ELL AES-CTR, not clear, never guess
  if(decode(kamwater,"12345678","kamwater")==Status::Ok ||
     result.value_count!=0) return 4;
  kamwater[16]=0;
  kamwater[17]^=1; // compact payload CRC negative
  if(decode(kamwater,"12345678","kamwater")==Status::Ok ||
     result.value_count!=0) return 5;

  // Apator162 original #19 independently decrypted PUBLIC application.
  // Synthetic mode-zero TPL is intentionally not the original ciphertext.
  const auto apator_app=from_hex(
    "0f648e7a9a0c020843900082628557d248570cf1580010581d00007001"
    "531d0000a09de19d02ffffffffffff0c2d");
  std::vector<uint8_t> apator={
    0,0x44,0x01,0x06,0x96,0x20,0x28,0x01,0x05,0x07,
    0x7A,0x74,0x00,0x00,0x00
  };
  apator.insert(apator.end(),apator_app.begin(),apator_app.end());
  apator[0]=static_cast<uint8_t>(apator.size()-1);
  assert(decode(apator,"01282096","apator162")==Status::Ok);
  assert(std::fabs(value(result,"total_m3")-7.512)<1e-6);
  assert(plain(apator,"01282096","01282096","apator162")==Status::Ok);
  assert(select_auto(apator,"01282096","01282096")!=nullptr);
  assert(std::strcmp(select_auto(apator,"01282096","01282096"),
                     "apator162")==0);
  assert(select_auto(apator,"01282097","01282096")==nullptr);

  assert(result.value_count==1);
  assert(select_auto(apator,"01282096","01282096")!=nullptr);
  apator[14]=0x05; // mode5 encrypted: must not enter clear decoder
  if(decode(apator,"01282096","apator162")==Status::Ok ||
     result.value_count!=0) return 6;
  assert(plain(apator,"01282096","01282096","apator162")!=Status::Ok);
  assert(result.value_count==0);
  return 0;
}
