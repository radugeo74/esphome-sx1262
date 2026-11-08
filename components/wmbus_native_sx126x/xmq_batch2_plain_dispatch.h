#pragma once
// Explicit no-key native decoder for *actually unencrypted* W-MBus RF.
// Input MUST be complete DLL CRC-verified, CRC-stripped by SX126x.
// Never infer cleartext from NOKEY upstream fixture metadata: original
// wmbusmeters fixtures can contain applications already decrypted upstream.
// No fallback after a failed keyed decode; AUTO is configured-only and
// cannot bypass plaintext transport, meter identity or record validation.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "xmq_decode_catalog.h"
#include "xmq_auto_dispatch.h"
#include "batch2_auto_identify.h"

namespace xmq_portable { namespace batch2_plain {
inline bool is_explicit_driver(const char *name) {
  return name &&
     (std::strcmp(name,"minomess")==0 ||
      std::strcmp(name,"kamwater")==0 ||
      std::strcmp(name,"apator162")==0 ||
      std::strcmp(name,"lansenpu")==0 ||
      std::strcmp(name,"lansendw")==0 ||
      std::strcmp(name,"cma12w")==0 ||
      std::strcmp(name,"elvsense")==0 ||
      std::strcmp(name,"lansenrp")==0 ||
      std::strcmp(name,"qsmoke")==0 ||
      std::strcmp(name,"qcaloric")==0 ||
      std::strcmp(name,"rfmamb")==0 ||
      std::strcmp(name,"sensostar")==0 ||
      std::strcmp(name,"evo868")==0 ||
      std::strcmp(name,"supercom587")==0 ||
      std::strcmp(name,"hcae2")==0 ||
      std::strcmp(name,"bfw240radio")==0 ||
      std::strcmp(name,"ultraheat")==0 ||
      std::strcmp(name,"sontex868")==0 ||
      std::strcmp(name,"supercal")==0 ||
      std::strcmp(name,"q400")==0);
}
// Strict manual or configured AUTO selection for cleartext RF. The radio
// MUST supply CRC-verified, CRC-stripped DLL frames. Metadata selection
// never publishes anything; decode_configured() revalidates all transport,
// security, identity and application record requirements. No AES key trial.
inline const char *select_configured_driver(
    const uint8_t *frame,size_t length,const char *meter_id,
    const char *dll_id,const char *requested,
    const wmbus_pure::DriverSignature *signatures,size_t count) {
  if(!frame || !meter_id || !dll_id || !requested ||
     !signatures || count==0) return nullptr;
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
     wmbus_pure::HeaderStatus::Ok ||
     wmbus_pure::match_meter_id(header,dll_id)!=
     wmbus_pure::MatchStatus::Match)
    return nullptr;
  if(std::strcmp(requested,"auto")!=0)
    return is_explicit_driver(requested) ? requested : nullptr;

  // Preserve existing compiled Lansen AUTO and Techem AUTO dispatch.
  // This helper consumes only these four native experimental drivers.
  const auto resolved=wmbus_pure::auto_driver::resolve(
      requested,signatures,count,header.manufacturer,header.version,
      header.device_type);
  if(resolved.status==wmbus_pure::auto_driver::Status::Ready &&
     resolved.driver &&
     (std::strcmp(resolved.driver,"lansenpu")==0 ||
      std::strcmp(resolved.driver,"lansendw")==0 ||
      std::strcmp(resolved.driver,"cma12w")==0 ||
      std::strcmp(resolved.driver,"elvsense")==0 ||
      std::strcmp(resolved.driver,"lansenrp")==0 ||
      std::strcmp(resolved.driver,"qsmoke")==0 ||
      std::strcmp(resolved.driver,"qcaloric")==0 ||
      std::strcmp(resolved.driver,"rfmamb")==0 ||
      std::strcmp(resolved.driver,"sensostar")==0 ||
      std::strcmp(resolved.driver,"evo868")==0 ||
      std::strcmp(resolved.driver,"supercom587")==0 ||
      std::strcmp(resolved.driver,"hcae2")==0 ||
      std::strcmp(resolved.driver,"bfw240radio")==0 ||
      std::strcmp(resolved.driver,"ultraheat")==0 ||
      std::strcmp(resolved.driver,"sontex868")==0 ||
      std::strcmp(resolved.driver,"supercal")==0 ||
      std::strcmp(resolved.driver,"q400")==0))
    return std::strcmp(meter_id,dll_id)==0 ? resolved.driver : nullptr;

  // Batch-2 candidates can use DLL, ELL or nested TPL metadata.
  // Only accept the source shape supported by the corresponding *clear*
  // decoder. An unrelated converter DLL may carry Minomess in long TPL.
  const auto candidate=wmbus_pure::batch2_identify::inspect(
      frame,length,signatures,count);
  if(candidate.state!=wmbus_pure::batch2_identify::State::Candidate ||
     !candidate.driver || !candidate.decimal_bcd ||
     !is_explicit_driver(candidate.driver))
    return nullptr;

  // Compare eight-digit ASCII meter identity (including leading zeros)
  // against the decimal BCD identity from the selected DLL/TPL header.
  if(std::strlen(meter_id)!=8) return nullptr;
  uint32_t configured=0;
  for(size_t i=0;i<8;++i) {
    if(meter_id[i]<'0' || meter_id[i]>'9') return nullptr;
    configured=configured*10u+static_cast<uint32_t>(meter_id[i]-'0');
  }
  if(configured!=candidate.meter_id) return nullptr;
  using Source=wmbus_pure::batch2_identify::Source;
  if(std::strcmp(candidate.driver,"minomess")==0) {
    // Minomess MUST expose a validated long TPL meter identity.
    // Supports separately configured DLL converter sender.
    return candidate.source==Source::Tpl ? candidate.driver : nullptr;
  }
  // Current Kamwater clear ELL and Apator clear TPL implementations
  // explicitly bind and validate their own *DLL* model and address.
  if(std::strcmp(meter_id,dll_id)!=0 ||
     candidate.source!=Source::Dll) return nullptr;
  return std::strcmp(candidate.driver,"kamwater")==0 ||
         std::strcmp(candidate.driver,"apator162")==0
             ? candidate.driver : nullptr;
}
inline PureDecodeStatus decode_configured(
    const uint8_t *frame,size_t length,const char *meter_id,
    const char *dll_id,const char *driver,Capture *captures,
    size_t capture_capacity,DecodedPayload &out) {
  out.value_count=0;
  out.date_count=0;
  if(!frame || !meter_id || !dll_id || !is_explicit_driver(driver))
    return PureDecodeStatus::UnknownDriver;
  return decode_configured_plain_frame(
      frame,length,meter_id,driver,captures,capture_capacity,out,dll_id);
}
}} // namespace xmq_portable::batch2_plain
