#pragma once
// Host-only explicitly supplied already-clear MK Radio 4a XMQ fixture.
// Never include in the Heltec live receiver: AES mode 5 still requires
// the configured key and both DLL converter / nested TPL identities.
// Original wmbusmeters mkradio4a.xmq examples are GPL-3.0-or-later.
#include <cstddef>
#include <cstdint>
#include "components/wmbus_native_sx126x/native_identity_probe.h"
#include "components/wmbus_native_sx126x/pure_frame_header.h"
#include "components/wmbus_native_sx126x/pure_meter_match.h"
#include "components/wmbus_native_sx126x/mkradio4a_plain.h"

namespace xmq_portable { namespace mkradio4a { namespace supplied_clear {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongDll, WrongTpl, WrongModel,
  NotSuppliedClear, InvalidRecords
};
inline Status decode(const uint8_t *frame,size_t size,
                     const char *tpl_meter_id,const char *dll_sender_id,
                     Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,size,h)!=
     wmbus_pure::HeaderStatus::Ok) return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(h,dll_sender_id)!=
     wmbus_pure::MatchStatus::Match) return Status::WrongDll;
  if(h.manufacturer!=0x5068 || h.version!=0x95 ||
     h.device_type!=0x37 || h.ci!=0x72)
    return Status::WrongModel;
  const auto identity=wmbus_pure::identity::inspect(frame,size);
  if(identity.status!=wmbus_pure::identity::Status::Ok ||
     !identity.has_tpl || !identity.tpl.decimal_bcd ||
     !wmbus_pure::identity::same_id(identity.tpl,tpl_meter_id))
    return Status::WrongTpl;
  if(!((identity.tpl.manufacturer==0x2324 &&
        identity.tpl.version==0xFE && identity.tpl.device_type==0x06) ||
       (identity.tpl.manufacturer==0x5CA8 &&
        identity.tpl.version==0xFE && identity.tpl.device_type==0x07)))
    return Status::WrongModel;
  // Source examples declare 3-block Mode-5 but already contain the
  // first two plaintext record fields. This marker is not an RF gate.
  if(size!=76 || frame[21]!=0x30 || frame[22]!=0x05 ||
     frame[23]!=0x2F || frame[24]!=0x2F)
    return Status::NotSuppliedClear;
  Reading next{};
  if(decode_clear_application(frame+25,size-25,next)!=mkradio4a::Status::Ok)
    return Status::InvalidRecords;
  out=next;return Status::Ok;
}
}}} // namespace xmq_portable::mkradio4a::supplied_clear
