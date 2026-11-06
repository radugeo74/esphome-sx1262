// Original public apator162.xmq fixture #19. OMS mode 5 AES-CBC is
// CONFIDENTIALITY ONLY: no CCM tag, MAC, or proof of authenticity.
// This regression proves decryption + IXML volume parity only.
#include "host_only/oms_tpl_cbc_mode5_openssl.h"
#include "host_only/apator162_ixml_fields.h"
#include "components/wmbus_radio/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;int hi=-1;
  for(;*s;++s) {
    int v=(*s>='0'&&*s<='9')?(*s-'0'):
          (*s>='a'&&*s<='f')?(*s-'a'+10):
          (*s>='A'&&*s<='F')?(*s-'A'+10):-1;
    assert(v>=0);
    if(hi<0) hi=v;
    else {out.push_back(static_cast<uint8_t>(hi*16+v));hi=-1;}
  }
  assert(hi<0);return out;
}
int main() {
  const auto original=hex(
      "3e4401069620280105077a7400308588f90b4dbd012bd789d7b4fb8536"
      "b889425b8624aee20a4862e60223427e24a9060d0db7b112cdd42a0"
      "a825130f3858a");
  const auto expected_clear=hex(
      "0f648e7a9a0c020843900082628557d248570cf1580010581d00007001"
      "531d0000a09de19d02ffffffffffff0c2d");
  const uint8_t fixture_key[16]{}; // Public XMQ, all zero.
  xmq_portable::oms_cbc::Reading output{};
  using Status=xmq_portable::oms_cbc::Status;
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      original.data(),original.size(),fixture_key,output)==Status::Ok);
  assert(output.application==expected_clear && output.tpl_status==0);
  xmq_portable::apator162::Reading meter{};
  assert(xmq_portable::apator162::decode_clear_manufacturer(
      output.application.data(),output.application.size(),meter)
      ==xmq_portable::apator162::Status::Ok);
  assert(std::fabs(meter.total_m3-7.512)<1e-9);
  // Full wireless DLL format-B replay around ORIGINAL upstream #19
  // bytes, which omit on-air DLL CRC. Only the CRC envelope is synthetic;
  // original encrypted TPL bytes and expected plaintext are unchanged.
  auto full=original;
  full[0]=static_cast<uint8_t>(full.size()+1u);
  const uint16_t dll_crc=wmbus_pure::crc16_en13757(
      full.data(),full.size());
  full.push_back(static_cast<uint8_t>(dll_crc>>8));
  full.push_back(static_cast<uint8_t>(dll_crc));
  wmbus_pure::CompleteFrame verified{};
  using FrameStatus=wmbus_pure::CompleteStatus;
  assert(wmbus_pure::verify_complete_frame(
      full.data(),full.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==FrameStatus::Ok);
  // Independent configured identity: never infer it from an incoming frame.
  const auto &identity=verified.header;
  assert(identity.manufacturer==0x0601);
  assert(identity.meter_id==1282096u);
  assert(identity.version==0x05 && identity.device_type==0x07);
  // In format B the transmitted L includes two CRC bytes; unlike the
  // upstream CRC-stripped test, its value is therefore two bytes larger.
  assert(verified.bytes.size()==original.size());
  assert(verified.bytes[0]==static_cast<uint8_t>(original[0]+2));
  assert(std::memcmp(verified.bytes.data()+1,original.data()+1,
                     original.size()-1)==0);
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      verified.bytes.data(),verified.bytes.size(),fixture_key,output)
      ==Status::Ok);
  assert(output.application==expected_clear);
  assert(xmq_portable::apator162::decode_clear_manufacturer(
      output.application.data(),output.application.size(),meter)
      ==xmq_portable::apator162::Status::Ok);
  assert(std::fabs(meter.total_m3-7.512)<1e-9);
  // A failed full-frame check cannot reach decrypted measurements.
  auto broken=full;
  broken[6]^=1; // DLL identity byte changed, CRC not recomputed
  assert(wmbus_pure::verify_complete_frame(
      broken.data(),broken.size(),wmbus_pure::CrcFrameFormat::B,verified)
      !=FrameStatus::Ok);
  // Even a recalculated DLL CRC does not authorize another configured ID.
  broken=full;
  broken[6]^=1;
  const uint16_t forged_crc=wmbus_pure::crc16_en13757(
      broken.data(),broken.size()-2);
  broken[broken.size()-2]=static_cast<uint8_t>(forged_crc>>8);
  broken.back()=static_cast<uint8_t>(forged_crc);
  assert(wmbus_pure::verify_complete_frame(
      broken.data(),broken.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==FrameStatus::Ok);
  assert(verified.header.meter_id!=1282096u);
  // CBC mode 5 has no MAC: marker checks cannot establish authenticity.
  // This host replay is NOT approval for RF admission or HA publication.
  // Missing key, unexpected mode, wrong CI, unsupported block sizing,
  // and bad initial plaintext markers must never produce a reading.
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      original.data(),original.size(),nullptr,output)==Status::MissingKey);
  assert(output.application.empty());
  auto bad=original;bad[10]=0x78;
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      bad.data(),bad.size(),fixture_key,output)==Status::UnsupportedCi);
  bad=original;bad[14]=0x8A;
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      bad.data(),bad.size(),fixture_key,output)==Status::UnsupportedMode);
  bad=original;bad[13]=0x20;
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      bad.data(),bad.size(),fixture_key,output)
      ==Status::UnsupportedBlockCount);
  bad=original;bad[15]^=0x01; // First ciphertext block changes clear marker.
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      bad.data(),bad.size(),fixture_key,output)==Status::BadMarker);
  assert(output.application.empty());
  bad=original;bad[2]^=0x01; // IV manufacturer portion must be exact.
  assert(xmq_portable::oms_cbc::decode_short_tpl(
      bad.data(),bad.size(),fixture_key,output)==Status::BadMarker);
  assert(output.application.empty());
  // IMPORTANT: a corrupted late CBC block is NOT guaranteed to be
  // rejected by the 2F2F marker. Never claim authenticated integrity.
  return 0;
}
