#pragma once
// HOST/PROBE ONLY: fail-closed persistent watermark contract for ONE meter.
// A caller supplies a trusted atomic/durable checkpoint store and an
// independently configured nonzero key epoch. NO NVS implementation here.
// Checkpoint CRC detects accidental corruption, NOT tampering or rollback.
// Successful synchronous sink callback precedes checkpoint write; HA
// exactly-once delivery is NOT guaranteed across crashes or partial writes.
#include <array>
#include <cstddef>
#include <cstdint>
#include "kamwater_replay_gate.h"

namespace xmq_portable { namespace kamwater { namespace checkpoint {
using Meter=replay_gate::Meter;
using State=replay_gate::State;
constexpr size_t kSize=25;
using Bytes=std::array<uint8_t,kSize>;
enum class Status : uint8_t {
  Ok, Missing, InvalidFormat, Corrupt, WrongMeter, WrongEpoch,
  NotReady, Busy, DecodeRejected, DuplicateOrStale,
  DeliveryFailed, StorageFailed
};
struct Session {
  State state{};
  Meter configured{};
  uint32_t epoch{0};
  bool ready{false};
  bool busy{false};
};
inline void put16(Bytes &b,size_t i,uint16_t v) {
  b[i]=static_cast<uint8_t>(v);
  b[i+1]=static_cast<uint8_t>(v>>8);
}
inline void put32(Bytes &b,size_t i,uint32_t v) {
  for(unsigned n=0;n<4;++n)
    b[i+n]=static_cast<uint8_t>(v>>(8*n));
}
inline uint16_t get16(const uint8_t *p,size_t i) {
  return static_cast<uint16_t>(p[i]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[i+1])<<8);
}
inline uint32_t get32(const uint8_t *p,size_t i) {
  uint32_t v=0;
  for(unsigned n=0;n<4;++n)
    v|=static_cast<uint32_t>(p[i+n])<<(8*n);
  return v;
}
// Standard reflected IEEE CRC32 for accidental torn-record detection.
// Intentionally NOT a cryptographic MAC or rollback protection.
inline uint32_t crc32(const uint8_t *bytes,size_t length) {
  uint32_t crc=0xFFFFFFFFu;
  for(size_t i=0;i<length;++i) {
    crc^=bytes[i];
    for(unsigned bit=0;bit<8;++bit)
      crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
  }
  return ~crc;
}
// Canonical wire format: magic KWR1 [0..3], format version [4],
// manufacturer [5..6], decimal meter id [7..10], version [11],
// medium [12], external key epoch [13..16], CCM watermark [17..20],
// CRC32 [21..24], all integers little-endian.
inline bool encode(const State &state,uint32_t epoch,Bytes &out) {
  out=Bytes{};
  if(!state.initialized || epoch==0) return false;
  out[0]='K';out[1]='W';out[2]='R';out[3]='1';out[4]=1;
  put16(out,5,state.identity.manufacturer);
  put32(out,7,state.identity.id);
  out[11]=state.identity.version;
  out[12]=state.identity.medium;
  put32(out,13,epoch);
  put32(out,17,state.highest_counter);
  put32(out,21,crc32(out.data(),21));
  return true;
}
// On ANY failure no replay state is admitted. The caller MUST NOT
// auto-create a replacement watermark when a stored record is missing.
inline Status restore(Session &session,const uint8_t *bytes,size_t length,
                      const Meter &configured,uint32_t epoch) {
  session=Session{};
  if(!bytes || length==0) return Status::Missing;
  if(length!=kSize || epoch==0 ||
     bytes[0]!='K'||bytes[1]!='W'||bytes[2]!='R'||bytes[3]!='1'||
     bytes[4]!=1) return Status::InvalidFormat;
  if(get32(bytes,21)!=crc32(bytes,21)) return Status::Corrupt;
  const Meter found{get16(bytes,5),get32(bytes,7),bytes[11],bytes[12]};
  if(!replay_gate::same_meter(found,configured)) return Status::WrongMeter;
  if(get32(bytes,13)!=epoch) return Status::WrongEpoch;
  session.state=State{true,configured,get32(bytes,17)};
  session.configured=configured;
  session.epoch=epoch;
  session.ready=true;
  return Status::Ok;
}
// Simulated/real sink success occurs BEFORE an atomic checkpoint update.
// If storing fails after sink side effects, latch the session blocked:
// caller must recover an independently verified watermark before retry.
// Store must make the ENTIRE 25-byte record durable, never a partial write.
// Sink must have a synchronous completion signal and be idempotent on retry.
template<typename Sink,typename Store>
inline Status deliver_once(Session &session,
                           const uint8_t *frame,size_t length,
                           const uint8_t key[16],
                           Sink &&sink,Store &&store) {
  if(!session.ready) return Status::NotReady;
  if(session.busy) return Status::Busy;
  session.busy=true;
  replay_gate::Prepared prepared{};
  const auto result=replay_gate::prepare(
      frame,length,session.configured,key,session.state,prepared);
  if(result!=replay_gate::Status::Ok) {
    session.busy=false;
    return result==replay_gate::Status::DuplicateOrStale
        ? Status::DuplicateOrStale : Status::DecodeRejected;
  }
  if(!sink(prepared.plan)) {
    replay_gate::discard(prepared);
    session.busy=false;
    return Status::DeliveryFailed;
  }
  // A callback can be reentrant; busy flag blocks recursive deliveries.
  State next{true,session.configured,prepared.counter};
  Bytes snapshot{};
  if(!encode(next,session.epoch,snapshot) ||
     !store(snapshot.data(),snapshot.size())) {
    replay_gate::discard(prepared);
    session.busy=false;
    session.ready=false; // fail closed until trusted restore
    return Status::StorageFailed;
  }
  session.state=next;
  replay_gate::discard(prepared);
  session.busy=false;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::checkpoint
