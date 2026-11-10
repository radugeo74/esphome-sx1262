#pragma once
// Optional typed text fields for already-validated, genuinely plaintext TPL.
// No decryption, no conversion of calendar dates to floating-point sensors.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "hydrus_plain.h"
#include "iperl_plain.h"
#include "lansenpu_plain.h"
#include "lansendw_plain.h"
#include "cma12w_plain.h"
#include "elvsense_plain.h"
#include "lansenrp_plain.h"
#include "rfmamb_plain.h"
#include "sensostar_plain.h"
#include "itron_plain.h"
#include "qualcosonic_plain.h"
#include "lse_08_plain.h"
#include "supercal_plain.h"
#include "waterstarm_plain.h"
#include "munia_plain.h"
#include "maddalena_plain.h"
#include "sontex868_plain.h"
#include "qheatv2_plain.h"
#include "c5isf_plain.h"
#include "apatoreitn_plain.h"
#include "evo868_plain.h"
#include "supercom587_plain.h"
#include "vario451_plain.h"
#include "ultraheat_plain.h"
#include "hcae2_plain.h"
#include "tpl_security_probe.h"

namespace xmq_portable { namespace text_fields {
struct Field {
  const char *name{nullptr};
  char text[128]{}; // full 53-byte HEX compact profile needs 106 characters
};
struct Result {
  Field fields[8]{};
  size_t count{0};
};
inline bool append(Result &out, const char *name, const char *value) {
  if (!name || !value || out.count >= 8) return false;
  Field &slot = out.fields[out.count++];
  slot.name = name;
  std::snprintf(slot.text, sizeof(slot.text), "%s", value);
  return true;
}
inline void append_calendar(Result &out,const char *field,
                            const standard::Calendar &date) {
  char formatted[24]{};
  if(date.has_time)
    std::snprintf(formatted,sizeof(formatted),"%04d-%02d-%02d %02d:%02d",
                  date.year,date.month,date.day,date.hour,date.minute);
  else
    std::snprintf(formatted,sizeof(formatted),"%04d-%02d-%02d",
                  date.year,date.month,date.day);
  append(out,field,formatted);
}
inline Result decode(const uint8_t *frame, size_t size, const char *driver) {
  Result out{};
  if (!frame || !driver || size < 15) return out;
  if(std::strcmp(driver,"apatoreitn")==0) {
    // Proprietary A0/B6 has no OMS TPL config word. The model-specific
    // decoder independently enforces exact MVT, original payload profile
    // and validated seal-date semantics. Do not claim blanket plaintext.
    apatoreitn::Reading r{};
    if(apatoreitn::decode(frame,size,r)!=apatoreitn::Status::Ok)
      return out;
    append(out,"status",r.seal_broken?"SEAL_BROKEN":"OK");
    return out;
  }
  if(std::strcmp(driver,"vario451")==0) {
    // CI=A2 is manufacturer-specific and has no OMS config word.
    // The native model-specific decoder is the only safe admission.
    vario451::Reading reading{};
    if(vario451::decode(frame,size,reading)!=vario451::Status::Ok)
      return out;
    append(out,"mfct_payload_hex",reading.mfct_payload_hex);
    return out;
  }
  const auto security = wmbus_pure::tpl_security::inspect(frame, size);
  if (security.state != wmbus_pure::tpl_security::State::NotEncrypted)
    return out;
  if(std::strcmp(driver,"lansendw")==0) {
    lansendw::Reading reading{};
    if(lansendw::decode(frame,size,reading)!=lansendw::Status::Ok)
      return out;
    append(out,xmq_compiled_profiles::lansendw::rules[0].name,
           lansendw::state_text(reading.digital_input));
    append(out,xmq_compiled_profiles::lansendw::rules[1].name,
           lansendw::error_text(reading.error_flags,reading.tpl_status));
    return out;
  }
  if(std::strcmp(driver,"rfmamb")==0) {
    rfmamb::Reading reading{};
    if(rfmamb::decode(frame,size,reading)!=rfmamb::Status::Ok)return out;
    // The original clear fixture has TPL 0x08 = MODULE_REMOVED.
    // Do not invent semantics for unrecognized status combinations.
    if(reading.tpl_status==0) append(out,"status","OK");
    else if(reading.tpl_status==0x08)
      append(out,"status","MODULE_REMOVED");
    return out;
  }
  if(std::strcmp(driver,"hcae2")==0) {
    hcae2::Reading r{};
    if(hcae2::decode(frame,size,r)!=hcae2::Status::Ok)return out;
    const char *status=hcae2::known_status(r);
    if(status) append(out,"status",status);
    return out;
  }
  if(std::strcmp(driver,"lse_08")==0) {
    lse_08::Reading r{};
    if(lse_08::decode(frame,size,r)!=lse_08::Status::Ok)return out;
    // Source XMQ says TPL_STS_128; never mislabel as "OK".
    if(r.tpl_status==0x80)append(out,"status","TPL_STS_128");
    append_calendar(out,"set_date",r.set_date);
    append_calendar(out,"device_datetime",r.device_datetime);
    append(out,"model_version",r.model_version);
    return out;
  }
  if(std::strcmp(driver,"supercal")==0) {
    supercal::Reading r{};
    if(supercal::decode(frame,size,r)!=supercal::Status::Ok)return out;
    append_calendar(out,"target_date",r.target_date);
    append_calendar(out,"meter_datetime",r.meter_datetime);
    append(out,"status","OK"); // verified TPL status 0
    return out;
  }
  if(std::strcmp(driver,"qheatv2")==0) {
    qheatv2::Reading r{};
    if(qheatv2::decode(frame,size,r)!=qheatv2::Status::Ok)return out;
    append(out,"status","OK"); // source-proven zero TPL status only
    return out;
  }
  if(std::strcmp(driver,"c5isf")==0) {
    c5isf::Reading r{};
    if(c5isf::decode(frame,size,r)!=c5isf::Status::Ok)return out;
    // Exact upstream error-state sample. Never report a successful
    // numeric decode as "OK" or infer unverified alarm combinations.
    if(r.tpl_status==0x02 && r.error_flags==0x0024)
      append(out,"status","DRY ERROR RETURN_SENSOR_INTERRUPTED");
    return out;
  }
  if(std::strcmp(driver,"maddalena")==0) {
    maddalena::Reading reading{};
    if(maddalena::decode(frame,size,reading)!=maddalena::Status::Ok)return out;
    append(out,"status","OK"); // zero flags source-verified
    append(out,"fabrication_no",reading.fabrication_no);
    append_calendar(out,"target_date",reading.target_date);
    append_calendar(out,"meter_datetime",reading.meter_datetime);
    append_calendar(out,"storage_2_date",reading.storage_2_date);
    append_calendar(out,"storage_3_datetime",reading.storage_3_datetime);
    return out;
  }
  if(std::strcmp(driver,"ultraheat")==0) {
    ultraheat::Reading reading{};
    if(ultraheat::decode(frame,size,reading)!=ultraheat::Status::Ok)
      return out;
    append(out,"status","TPL_STS_32");
    append(out,"meter_datetime",reading.meter_datetime);
    return out;
  }
  if(std::strcmp(driver,"sontex868")==0) {
    sontex868::Reading reading{};
    if(sontex868::decode(frame,size,reading)!=sontex868::Status::Ok)return out;
    append_calendar(out,"set_date",reading.set_date);
    append_calendar(out,"device_datetime",reading.device_datetime);
    append_calendar(out,"storage_48_date",reading.storage_48_date);
    append_calendar(out,"tariff_1_date",reading.tariff_1_date);
    append_calendar(out,"tariff_2_date",reading.tariff_2_date);
    append(out,"software_version",reading.software_version);
    append(out,"compact_profile_hex",reading.compact_profile_hex);
    append(out,"status","OK"); // TPL status 0 and clear-mode gated
    return out;
  }
  if(std::strcmp(driver,"munia")==0) {
    munia::Reading r{};
    if(munia::decode(frame,size,r)!=munia::Status::Ok)return out;
    append(out,"status","OK"); // two original zero-status clear frames
    return out;
  }
  if(std::strcmp(driver,"waterstarm")==0) {
    waterstarm::Reading r{};
    if(waterstarm::decode(frame,size,r)!=waterstarm::Status::Ok)return out;
    append(out,"status","OK"); // upstream clear fixtures both zero-status
    return out;
  }
  if(std::strcmp(driver,"itron")==0) {
    itron::Reading reading{};
    if(itron::decode(frame,size,reading)!=itron::Status::Ok)return out;
    append(out,"status","OK"); // only zero-status original Mode0 frames
    return out;
  }
  if(std::strcmp(driver,"qualcosonic")==0) {
    qualcosonic::Reading reading{};
    if(qualcosonic::decode(frame,size,reading)!=qualcosonic::Status::Ok)return out;
    append(out,"status","OK"); // no unverified alarm-bit interpretation
    return out;
  }
  if(std::strcmp(driver,"evo868")==0) {
    evo868::Reading reading{};
    if(evo868::decode(frame,size,reading)!=evo868::Status::Ok)return out;
    append(out,"status","OK");
    return out;
  }
  if(std::strcmp(driver,"supercom587")==0) {
    supercom587::Reading reading{};
    if(supercom587::decode(frame,size,reading)!=supercom587::Status::Ok)return out;
    append(out,"status","OK"); // only source-proven zero error flags
    append(out,"software_version",reading.software_version);
    return out;
  }
  if(std::strcmp(driver,"sensostar")==0) {
    sensostar::Reading reading{};
    if(sensostar::decode(frame,size,reading)!=sensostar::Status::Ok)return out;
    append(out,"status","OK"); // only zero-status clear frames admitted
    return out;
  }
  if(std::strcmp(driver,"elvsense")==0) {
    elvsense::Reading reading{};
    if(elvsense::decode(frame,size,reading)!=elvsense::Status::Ok)
      return out;
    append(out,"status","OK");
    return out;
  }
  if(std::strcmp(driver,"lansenrp")==0) {
    lansenrp::Reading reading{};
    if(lansenrp::decode(frame,size,reading)!=lansenrp::Status::Ok)
      return out;
    append(out,"status",reading.power_low?"POWER_LOW":"OK");
    append(out,"is_repeater_listening",reading.listening?"YES":"NO");
    return out;
  }
  if(std::strcmp(driver,"cma12w")==0) {
    cma12w::Reading reading{};
    if(cma12w::decode(frame,size,reading)!=cma12w::Status::Ok)
      return out;
    char battery[32]{};
    std::snprintf(battery,sizeof(battery),"%s_%X",
                  xmq_compiled_profiles::cma12w::profile.battery_prefix,
                  static_cast<unsigned>(reading.battery_bits));
    append(out,"status",cma12w::status_text(reading.tpl_status));
    append(out,xmq_compiled_profiles::cma12w::rules[2].name,battery);
    append(out,xmq_compiled_profiles::cma12w::rules[3].name,
           reading.software_version);
    return out;
  }
  if (std::strcmp(driver, "lansenpu") == 0) {
    lansenpu::Reading reading{};
    if(lansenpu::decode(frame,size,reading)!=lansenpu::Status::Ok)
      return out;
    const char *status=lansenpu::status_text(reading.tpl_status);
    if(status) append(out,"status",status);
    return out;
  }
  if (std::strcmp(driver, "iperl") == 0) {
    iperl::Reading reading{};
    if (iperl::decode(frame, size, reading) != iperl::Status::Ok) return out;
    // Only status patterns explicitly covered by official iPERL fixtures.
    if (reading.tpl_status == 0) append(out, "status", "OK");
    else if (reading.tpl_status == 0x06)
      append(out, "status", "ERROR POWER_LOW");
    return out;
  }
  if (std::strcmp(driver, "hydrus") != 0) return out;
  wmbus_pure::Header header{};
  if (wmbus_pure::parse_header(frame, size, header) != wmbus_pure::HeaderStatus::Ok)
    return out;
  const size_t offset = header.ci == 0x7A ? 15 :
                        header.ci == 0x72 ? 23 : 0;
  if (!offset || offset >= size) return out;
  hydrus::DateField dates[4]{};
  size_t date_count=0, value_count=0;
  CalcValue numeric[24]{};
  if (hydrus::decode_clear_application(frame + offset, size - offset,
      numeric, 24, value_count, dates, 4, &date_count) != hydrus::Status::Ok)
    return out;
  for (size_t i=0; i<date_count && i<4; ++i) {
    char result[24]{};
    const auto &d = dates[i].value;
    if (d.has_time)
      std::snprintf(result,sizeof(result),"%04d-%02d-%02d %02d:%02d",
                    d.year,d.month,d.day,d.hour,d.minute);
    else
      std::snprintf(result,sizeof(result),"%04d-%02d-%02d",
                    d.year,d.month,d.day);
    append(out,dates[i].name,result);
  }
  return out;
}
} }
