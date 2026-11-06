#pragma once
// Portable original minomess.xmq wired M-Bus long-frame (CI 72) adapter.
// This is NOT a wireless T1/C1 transport handler; never dispatch live RF here.
// The wired 68/L/L/68/C/A/CI/72 envelope is NOT a wireless T1/C1 DLL.
// Its checksum is the M-Bus byte SUM, not EN13757 wireless block CRC.
// Gateway-supplied RSSI and actuality are not meter-owned measurements.
// This is a deliberately narrow, non-publishing public-fixture contract.
#include <cstddef>
#include <cstdint>
#include "minomess_clear_prefix.h"
namespace xmq_portable { namespace minomess { namespace wired {
enum class Status : uint8_t {
  Ok, Invalid, BadLength, BadChecksum, UnsupportedEnvelope,
  UnsupportedMeter, ApplicationRejected, UnsupportedGateway,
  IdentityMismatch
};
struct Reading {
  minomess::Reading meter{};
  bool has_gateway_rssi{false};
  int gateway_rssi_dbm{0};
  bool has_actuality_duration{false};
  uint16_t actuality_duration_s{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  if(!frame || length<21) return Status::Invalid;
  if(frame[0]!=0x68 || frame[3]!=0x68 || frame[length-1]!=0x16)
    return Status::UnsupportedEnvelope;
  if(frame[1]!=frame[2] ||
     static_cast<size_t>(frame[1])+6!=length)
    return Status::BadLength;
  uint8_t sum=0;
  for(size_t i=4;i<length-2;++i)
    sum=static_cast<uint8_t>(sum+frame[i]);
  if(sum!=frame[length-2]) return Status::BadChecksum;
  // Original CI=72 M-Bus variable data header and one public
  // Zenner minomess meter type; reject accidental reinterpretation.
  if(frame[4]!=0x08 || frame[5]!=0x00 || frame[6]!=0x72)
    return Status::UnsupportedEnvelope;
  if(frame[11]!=0x49 || frame[12]!=0x6A ||
     frame[13]!=0x01 || frame[14]!=0x16)
    return Status::UnsupportedMeter;
  for(size_t i=7;i<=10;++i)
    if((frame[i]&15u)>9 || (frame[i]>>4)>9)
      return Status::UnsupportedMeter;
  // Wire-format status and signature are not user-space DIF/VIF.
  if(frame[15]!=0xA3 && frame[15]!=0xDD)
    return Status::UnsupportedEnvelope;
  if(frame[16]!=0 || frame[17]!=0 || frame[18]!=0)
    return Status::UnsupportedEnvelope;
  const uint8_t *app=frame+19;
  const size_t app_len=length-21;
  minomess::Reading meter{};
  if(minomess::decode_clear_application(app,app_len,meter)
      !=minomess::Status::Ok) return Status::ApplicationRejected;
  // Prefix 22 bytes + 8D049313 + LVAR(44) + 02FD17 status(2).
  const size_t optional_start=22+5+44+5;
  if(app_len<optional_start) return Status::ApplicationRejected;
  Reading candidate{};
  candidate.meter=meter;
  size_t cursor=optional_start;
  if(app_len-cursor>=4 && app[cursor]==0x01 &&
     app[cursor+1]==0xFD && app[cursor+2]==0x71) {
    candidate.has_gateway_rssi=true;
    candidate.gateway_rssi_dbm=static_cast<int>(
        static_cast<int8_t>(app[cursor+3]));
    cursor+=4;
  }
  if(app_len-cursor>=4 && app[cursor]==0x02 &&
     app[cursor+1]==0x74) {
    candidate.has_actuality_duration=true;
    candidate.actuality_duration_s=static_cast<uint16_t>(
        app[cursor+2] | (static_cast<uint16_t>(app[cursor+3])<<8));
    cursor+=4;
  }
  if(cursor!=app_len) return Status::UnsupportedGateway;
  out=candidate;
  return Status::Ok;
}
// Optional configured-ID admission for the wired gateway path only.
// The eight BCD digits are the long-header meter ID (not the gateway
// address, and not an authorization to interpret this as wireless T1/C1).
// Caller supplies its own independently configured 8-digit decimal ID.
inline Status decode_configured(const uint8_t *frame,size_t length,
                                const char *configured_id,Reading &out) {
  out={};
  Reading candidate{};
  const Status result=decode(frame,length,candidate);
  if(result!=Status::Ok) return result;
  if(!configured_id) return Status::IdentityMismatch;
  // BCD ID is four bytes little-endian, high nibble then low nibble
  // within each byte, displayed most significant byte first.
  char id[9]{};
  for(size_t i=0;i<4;++i) {
    const uint8_t b=frame[10-i];
    id[i*2]=static_cast<char>('0'+(b>>4));
    id[i*2+1]=static_cast<char>('0'+(b&15u));
  }
  for(size_t i=0;i<8;++i)
    if(configured_id[i]!=id[i]) return Status::IdentityMismatch;
  if(configured_id[8]!='\\0') return Status::IdentityMismatch;
  out=candidate;
  return Status::Ok;
}
}}} // namespace xmq_portable::minomess::wired
