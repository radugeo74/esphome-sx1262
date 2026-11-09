#pragma once
// Strict dual-address clear CI72 short/long TPL admission.
// Only explicit DLL sender ID AND embedded long-TPL target ID may be bound.
// No relaxing the existing single-address publication::inspect() policy.
// Input MUST be independently CRC-verified and CRC-stripped by SX126x.
// No encrypted/ELL/vendor/raw or fixture-supplied decrypted application.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "pure_frame_header.h"
#include "pure_meter_match.h"
#include "native_identity_probe.h"
#include "tpl_security_probe.h"
namespace xmq_portable { namespace bound_long_tpl {
enum class Status:uint8_t {
  Ok,Invalid,WrongDll,WrongTarget,WrongProfile,NotClear
};
inline Status inspect(const uint8_t *frame,size_t n,
                      const char*target_id,const char*dll_id,
                      uint16_t tpl_m,uint8_t tpl_v,uint8_t tpl_t,
                      uint16_t dll_m,uint8_t dll_v,uint8_t dll_t) {
 wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok ||
    h.control!=0x44 || h.ci!=0x72)
   return Status::Invalid;
 if(!dll_id || wmbus_pure::match_meter_id(h,dll_id)!=
     wmbus_pure::MatchStatus::Match || h.manufacturer!=dll_m ||
    h.version!=dll_v || h.device_type!=dll_t)
   return Status::WrongDll;
 const auto address=wmbus_pure::identity::inspect(frame,n);
 if(address.status!=wmbus_pure::identity::Status::Ok ||
    !address.has_tpl || !address.tpl.decimal_bcd ||
    !address.dll.decimal_bcd || !target_id || !dll_id ||
    std::strcmp(target_id,dll_id)==0 ||
    wmbus_pure::identity::same_id_bytes(address.tpl,address.dll) ||
    !wmbus_pure::identity::same_id(address.tpl,target_id) ||
    !wmbus_pure::identity::same_id(address.dll,dll_id))
   return Status::WrongTarget;
 if(address.tpl.manufacturer!=tpl_m || address.tpl.version!=tpl_v ||
    address.tpl.device_type!=tpl_t)
   return Status::WrongProfile;
 const auto security=wmbus_pure::tpl_security::inspect(frame,n);
 if(security.state!=wmbus_pure::tpl_security::State::NotEncrypted)
   return Status::NotClear;
 return Status::Ok;
}
}} // namespace xmq_portable::bound_long_tpl
