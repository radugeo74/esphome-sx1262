#pragma once
// Generated-XMQ decode catalog. Driver metadata is centralized here;
// dispatch and meter processing do not need driver-specific table branches.
#include <cstddef>
#include <cstring>
#include "frame_bridge.h"
#include "xmq_generated.h"
#include "xmq_native_descriptors.h"
#include "pure_frame_header.h"
#include "pure_meter_match.h"
#include "iperl_plain.h"
#include "hydrus_plain.h"
#include "lansenpu_plain.h"
#include "lansendw_plain.h"
#include "cma12w_plain.h"
#include "elvsense_plain.h"
#include "lansenrp_plain.h"
#include "qsmoke_plain.h"
#include "qcaloric_plain.h"
#include "rfmamb_plain.h"
#include "sensostar_plain.h"
#include "evo868_plain.h"
#include "supercom587_plain.h"
#include "hcae2_plain.h"
#include "bfw240radio_plain.h"
#include "qwaterv2_plain.h"
#include "hydrodigit_plain.h"
#include "itron_plain.h"
#include "qualcosonic_plain.h"
#include "lse_08_plain.h"
#include "waterstarm_plain.h"
#include "lse_07_17_plain.h"
#include "munia_plain.h"
#include "qheat_55_us_plain.h"
#include "maddalena_plain.h"
#include "itronheat_plain.h"
#include "ehzp_plain.h"
#include "qheatv2_plain.h"
#include "c5isf_plain.h"
#include "microclima_plain.h"
#include "abbb23_plain.h"
#include "ei6500_plain.h"
#include "apatoreitn40_plain.h"
#include "elf_plain.h"
#include "engelmann_faw_plain.h"
#include "apatoreitn_plain.h"
#include "gransystems_plain.h"
#include "compact5_plain.h"
#include "vario451_plain.h"
#include "ultraheat_plain.h"
#include "sontex868_plain.h"
#include "supercal_plain.h"
#include "q400_plain.h"
#include "mkradio4a_plain.h"
#include "minomess_clear_prefix.h"
#include "kamwater_clear_pipeline.h"
#include "apator162_ixml_fields.h"
#include "native_publication_gate.h"

namespace xmq_portable {
enum class PayloadPolicy : uint8_t { Application, PreferManufacturer0f };
struct DecodeDescriptor {
  const char *name;
  uint8_t ci;
  PayloadPolicy payload_policy;
  DriverTables tables;
};

inline PayloadStatus descriptor_payload(const DecodeDescriptor &descriptor,
    const uint8_t *frame, size_t length, size_t header_size,
    size_t suffix_size, int mfct_0f_index, PayloadSpan &out) {
  if (descriptor.payload_policy == PayloadPolicy::PreferManufacturer0f &&
      mfct_0f_index >= 0)
    return manufacturer_payload(frame, length, header_size, suffix_size,
                                mfct_0f_index, out);
  return application_payload(frame, length, header_size, suffix_size, out);
}

inline DriverStatus decode_descriptor_frame(const DecodeDescriptor &descriptor,
    const uint8_t *frame, size_t length, size_t header_size,
    size_t suffix_size, int mfct_0f_index,
    Capture *captures, size_t capture_capacity, DecodedPayload &output) {
  PayloadSpan payload{};
  if (descriptor_payload(descriptor, frame, length, header_size,
                         suffix_size, mfct_0f_index, payload) != PayloadStatus::Ok) {
    output.value_count = 0;
    output.date_count = 0;
    return DriverStatus::BadArguments;
  }
  return decode_driver_payload(descriptor.tables, payload.data, payload.length,
                               captures, capture_capacity, output);
}

inline const DecodeDescriptor *find_decode_descriptor(const char *name, uint8_t ci) {
  static const DecodeDescriptor catalog[] = {
    XMQ_NATIVE_CATALOG_ENTRIES
  };
  if (!name) return nullptr;
  for (const auto &descriptor : catalog)
    if (ci == descriptor.ci && std::strcmp(name, descriptor.name) == 0)
      return &descriptor;
  return nullptr;
}

// Explicit handoff of an already decrypted, validated Mode-5 application.
// The 2F 2F marker is checked by the existing AES decoder, but checked again
// here so an accidental direct call cannot skip the protocol prefix.
// This does NOT establish that arbitrary Mode-5 data matches an XMQ driver.
inline DriverStatus decode_mode5_plain_payload(
    const DecodeDescriptor &descriptor, const uint8_t *plaintext,
    size_t length, Capture *captures, size_t capture_capacity,
    DecodedPayload &output) {
  output.value_count = 0;
  output.date_count = 0;
  if (!plaintext || length <= 2 ||
      plaintext[0] != 0x2F || plaintext[1] != 0x2F)
    return DriverStatus::BadArguments;
  return decode_driver_payload(descriptor.tables, plaintext + 2,
                               length - 2, captures, capture_capacity, output);
}

// Application bytes must be explicitly supplied as cleartext by a
// trusted caller (official '_' fixture or a validated upstream decryptor).
// This API MUST NOT be called directly with unknown live Mode-5 RF bytes.
inline DriverStatus decode_supplied_clear_mkradio4a_application(
    const uint8_t *application, size_t length, DecodedPayload &output) {
  output.value_count = 0;
  output.date_count = 0;
  if (!output.values || output.value_capacity < 1 ||
      !output.dates || output.date_capacity < 1)
    return DriverStatus::BadArguments;
  mkradio4a::Reading reading{};
  if (mkradio4a::decode_clear_application(application,length,reading) !=
      mkradio4a::Status::Ok) return DriverStatus::BadArguments;
  output.values[0] = {"target_m3", reading.target_m3};
  output.value_count = 1;
  output.dates[0] = {reading.year, reading.month, reading.day};
  output.date_count = 1;
  return DriverStatus::Ok;
}

enum class PureDecodeStatus : uint8_t {
  Ok, InvalidFrame, InvalidMeterId, UnsupportedTransport, UnknownDriver, DecodeFailure
};

// Input is already DLL-CRC-verified and CRC-stripped by the radio layer.
// Does not decrypt encrypted frames; callers must provide plain data.
// Never expose old decoded values after any failure.
inline PureDecodeStatus decode_configured_plain_frame(
    const uint8_t *frame, size_t length, const char *configured_id,
    const char *driver_name, Capture *captures, size_t capture_capacity,
    DecodedPayload &output, const char *expected_dll_id=nullptr) {
  output.value_count = 0;
  output.date_count = 0;
  wmbus_pure::Header header{};
  if (wmbus_pure::parse_header(frame, length, header) !=
      wmbus_pure::HeaderStatus::Ok)
    return PureDecodeStatus::InvalidFrame;
  if (wmbus_pure::match_meter_id(
          header,expected_dll_id ? expected_dll_id : configured_id) !=
      wmbus_pure::MatchStatus::Match)
    return PureDecodeStatus::InvalidMeterId;
  // Strict CI=7A Sensus iPERL plaintext path, using official XMQ fixtures.
  // Encrypted telegrams cannot enter this path before a validated decryptor.
  if (driver_name && std::strcmp(driver_name, "iperl") == 0) {
    // Defense in depth: enforce the same OMS/identity gate used by
    // NativeReceiverProbe, not merely application-record plausibility.
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    if (!output.values || output.value_capacity < 2)
      return PureDecodeStatus::DecodeFailure;
    iperl::Reading water{};
    if (iperl::decode(frame, length, water) != iperl::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0] = {"total_m3", water.total_m3};
    output.values[1] = {"max_flow_m3h", water.max_flow_m3h};
    output.value_count = 2;
    return PureDecodeStatus::Ok;
  }
  // Diehl Hydrus: receive only actual plaintext CI=72/7A with TPL
  // security mode zero. The shared standard DIF/VIF parser must produce
  // a complete valid reading before any sensor is updated.
  if (driver_name && std::strcmp(driver_name, "hydrus") == 0) {
    if (!output.values || output.value_capacity == 0)
      return PureDecodeStatus::DecodeFailure;
    size_t count = 0;
    if (hydrus::decode_plain_frame(frame, length, output.values,
                                   output.value_capacity, count) != hydrus::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.value_count = count;
    return PureDecodeStatus::Ok;
  }
  // Explicit-only Lansen pulse counter. Strict manufacturer/version/medium,
  // mode-zero short TPL, original DIF/VIF byte signatures and 12-digit BCD.
  // The preceding configured DLL ID is mandatory; no guessed AUTO driver.
  if(driver_name && std::strcmp(driver_name,"lansenpu")==0) {
    // A/B pulse inputs plus complete standard Analyzer diagnostics for
    // the three strictly admitted XMQ TPL statuses. Unsupported status
    // combinations still fail closed. Maximum 11 mapped numeric fields.
    if(!output.values || output.value_capacity<11)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(
          frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    lansenpu::Reading counts{};
    if(lansenpu::decode(frame,length,counts)!=lansenpu::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    if(counts.has_a)
      output.values[output.value_count++]={"a_counter",counts.a_counter};
    if(counts.has_b)
      output.values[output.value_count++]={"b_counter",counts.b_counter};
    output.values[output.value_count++]={"tpl_status_raw",
        static_cast<double>(counts.tpl_status)};
    output.values[output.value_count++]={"status_power_low",
        lansenpu::power_low(counts.tpl_status)?1.0:0.0};
    output.values[output.value_count++]={"status_permanent_error",
        lansenpu::permanent_error(counts.tpl_status)?1.0:0.0};
    output.values[output.value_count++]={"status_sabotage_enclosure",
        lansenpu::sabotage_enclosure(counts.tpl_status)?1.0:0.0};
    // All four standard flags are FALSE in the exact source-admitted
    // status set (0x00, 0x04, 0x4C). 0x40 is a known LAS tamper bit,
    // not an unknown TPL flag. No guess is made for other TPL states.
    output.values[output.value_count++]={"status_alarm",0.0};
    output.values[output.value_count++]={"status_busy",0.0};
    output.values[output.value_count++]={"status_error",0.0};
    output.values[output.value_count++]={"status_temporary_error",0.0};
    output.values[output.value_count++]={"tpl_status_unknown_raw",0.0};
    return PureDecodeStatus::Ok;
  }
  // Lansen door/window: both pulse counters plus source-backed raw
  // status diagnostics. Text CLOSE/OPEN and unknown flags remain distinct.
  if(driver_name && std::strcmp(driver_name,"lansendw")==0) {
    if(!output.values || output.value_capacity<14) return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    lansendw::Reading reading{};
    if(lansendw::decode(frame,length,reading)!=lansendw::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"a_counter",reading.a_counter};
    output.values[1]={"b_counter",reading.b_counter};
    output.values[2]={"digital_input_raw",static_cast<double>(reading.digital_input)};
    output.values[3]={"door_open",reading.digital_input==0x55u?1.0:0.0};
    output.values[4]={"error_flags_raw",static_cast<double>(reading.error_flags)};
    output.values[5]={"error_flags_unknown_raw",
        static_cast<double>(lansendw::unknown_error_bits(reading.error_flags))};
    output.values[6]={"tpl_status_raw",static_cast<double>(reading.tpl_status)};
    output.values[7]={"tpl_status_unknown_raw",
        static_cast<double>(lansendw::unknown_tpl_bits(reading.tpl_status))};
    output.values[8]={"status_alarm",0.0};
    output.values[9]={"status_busy",0.0};
    output.values[10]={"status_error",0.0};
    output.values[11]={"status_permanent_error",
        lansendw::confirmed_permanent_error(reading.tpl_status)?1.0:0.0};
    output.values[12]={"status_power_low",0.0};
    output.values[13]={"status_temporary_error",0.0};
    output.value_count=14;
    return PureDecodeStatus::Ok;
  }
  // Elvaco CMA12w: two temperatures in degrees C, battery/software
  // and OK status as optional text sensors. No general VIF inference.
  if(driver_name && std::strcmp(driver_name,"cma12w")==0) {
    if(!output.values || output.value_capacity<2) return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    cma12w::Reading reading{};
    if(cma12w::decode(frame,length,reading)!=cma12w::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_temperature_c",reading.current_temperature_c};
    output.values[1]={"average_temperature_1h_c",reading.average_temperature_1h_c};
    output.value_count=2;
    return PureDecodeStatus::Ok;
  }
  // Elvaco Sense only verified Mode-0 short TPL and strict records.
  if(driver_name && std::strcmp(driver_name,"elvsense")==0){
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    elvsense::Reading reading{};
    if(elvsense::decode(frame,length,reading)!=elvsense::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"temperature_c",reading.temperature_c};
    output.values[1]={"battery_v",reading.battery_v};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // Lansen R4 repeater, two public clear Mode-0 fixed-profile examples.
  if(driver_name && std::strcmp(driver_name,"lansenrp")==0){
    if(!output.values || output.value_capacity<7)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    lansenrp::Reading reading{};
    if(lansenrp::decode(frame,length,reading)!=lansenrp::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_routed_messages_counter",reading.total_routed_messages_counter};
    output.values[1]={"used_router_slots_counter",reading.used_router_slots_counter};
    output.values[2]={"seconds_to_mode_change_counter",reading.seconds_to_mode_change_counter};
    output.values[3]={"listen_timer_value_counter",reading.listen_timer_value_counter};
    output.values[4]={"pause_timer_value_counter",reading.pause_timer_value_counter};
    output.values[5]={"start_time_value_counter",reading.start_time_value_counter};
    output.values[6]={"battery_v",reading.battery_v};
    output.value_count=7;return PureDecodeStatus::Ok;
  }
  // QSmoke is telemetry (test/alarm counters), NOT a fire alarm signal.
  if(driver_name && std::strcmp(driver_name,"qsmoke")==0){
    if(!output.values || output.value_capacity<2)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qsmoke::Reading r{};
    if(qsmoke::decode(frame,length,r)!=qsmoke::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"alarm_counter",r.alarm_counter};
    output.values[1]={"test_button_counter",r.test_button_counter};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // QCaloric: standard unencrypted HCA records only; no vendor WalkBy.
  if(driver_name && std::strcmp(driver_name,"qcaloric")==0){
    if(!output.values || output.value_capacity<2)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qcaloric::Reading r{};
    if(qcaloric::decode(frame,length,r)!=qcaloric::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_consumption_hca",r.current_consumption_hca};
    output.value_count=1;
    if(r.has_set)
      output.values[output.value_count++]=
        {"consumption_at_set_date_hca",r.consumption_at_set_date_hca};
    return PureDecodeStatus::Ok;
  }
  // BMeters RFM-AMB: six timestamp-independent temperature and RH fields.
  // Mode-5 encrypted original example is NEVER a no-key RF golden vector.
  if(driver_name && std::strcmp(driver_name,"rfmamb")==0) {
    if(!output.values || output.value_capacity<6)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    rfmamb::Reading r{};
    if(rfmamb::decode(frame,length,r)!=rfmamb::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_temperature_c",r.current_temperature_c};
    output.values[1]={"average_temperature_1h_c",r.average_temperature_1h_c};
    output.values[2]={"average_temperature_24h_c",r.average_temperature_24h_c};
    output.values[3]={"current_relative_humidity_rh",r.current_relative_humidity_rh};
    output.values[4]={"average_relative_humidity_1h_rh",r.average_relative_humidity_1h_rh};
    output.values[5]={"average_relative_humidity_24h_rh",r.average_relative_humidity_24h_rh};
    output.value_count=6;return PureDecodeStatus::Ok;
  }
  // Engelmann Sensostar: original clear short-TPL energy/volume/target.
  // Other vendor, wired and AES formats require separate proof.
  if(driver_name && std::strcmp(driver_name,"sensostar")==0) {
    if(!output.values || output.value_capacity<3)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    sensostar::Reading r{};
    if(sensostar::decode(frame,length,r)!=sensostar::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_kwh",r.total_kwh};
    output.values[1]={"total_water_m3",r.total_water_m3};
    output.value_count=2;
    if(r.has_target_kwh)output.values[output.value_count++]={"target_kwh",r.target_kwh};
    return PureDecodeStatus::Ok;
  }
  // Maddalena EVO868: bounded standard Mode-0 water and billing volume.
  if(driver_name && std::strcmp(driver_name,"evo868")==0) {
    if(!output.values || output.value_capacity<2)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    evo868::Reading r{};
    if(evo868::decode(frame,length,r)!=evo868::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"consumption_at_set_date_m3",r.consumption_at_set_date_m3};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // Sontex Supercom 587: original XMQ total + software version + status.
  if(driver_name && std::strcmp(driver_name,"supercom587")==0) {
    if(!output.values || output.value_capacity<1)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    supercom587::Reading r{};
    if(supercom587::decode(frame,length,r)!=supercom587::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.value_count=1;return PureDecodeStatus::Ok;
  }
  // Engelmann HCA E2: source-validated T1 Mode-0 consumption subset.
  if(driver_name && std::strcmp(driver_name,"hcae2")==0) {
    if(!output.values || output.value_capacity<2)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    hcae2::Reading r{};
    if(hcae2::decode(frame,length,r)!=hcae2::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"consumption_hca",r.consumption_hca};
    output.values[1]={"target_hca",r.target_hca};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // BFW240radio: proprietary offset-specific clear-TPL HCA subset.
  if(driver_name && std::strcmp(driver_name,"bfw240radio")==0) {
    if(!output.values || output.value_capacity<2)return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    bfw240radio::Reading r{};
    if(bfw240radio::decode(frame,length,r)!=bfw240radio::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_hca",r.current_hca};
    output.values[1]={"prev_hca",r.prev_hca};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // Qundis QWater V2: source-tested standard CI7A Mode0 BCD volume subset.
  if(driver_name && std::strcmp(driver_name,"qwaterv2")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qwaterv2::Reading r{};
    if(qwaterv2::decode(frame,length,r)!=qwaterv2::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"target_year_m3",r.target_year_m3};
    output.values[2]={"target_m3",r.target_m3};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // B.Meters Hydrodigit: only genuinely Mode0 standard water total.
  if(driver_name && std::strcmp(driver_name,"hydrodigit")==0) {
    if(!output.values || output.value_capacity<1)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    hydrodigit::Reading r{};
    if(hydrodigit::decode(frame,length,r)!=hydrodigit::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.value_count=1;return PureDecodeStatus::Ok;
  }
  // Itron/Allmess: two public CI7A Mode0 short application variants.
  if(driver_name && std::strcmp(driver_name,"itron")==0) {
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    itron::Reading r{};
    if(itron::decode(frame,length,r)!=itron::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"target_m3",r.target_m3};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // Qualcosonic: 3 public clear Mode0 records, standard volume/temp/power.
  if(driver_name && std::strcmp(driver_name,"qualcosonic")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qualcosonic::Reading r{};
    if(qualcosonic::decode(frame,length,r)!=qualcosonic::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"flow_temperature_c",r.flow_temperature_c};
    output.values[2]={"return_temperature_c",r.return_temperature_c};
    output.values[3]={"power_kw",r.power_kw};
    output.value_count=4;return PureDecodeStatus::Ok;
  }
  // LSE/01/08: all XMQ fields and raw timing/status diagnostics.
  if(driver_name && std::strcmp(driver_name,"lse_08")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    lse_08::Reading r{};
    if(lse_08::decode(frame,length,r)!=lse_08::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"consumption_at_set_date_hca",r.consumption_at_set_date_hca};
    output.values[1]={"duration_since_readout_h",r.duration_since_readout_h};
    output.values[2]={"duration_since_readout_s",static_cast<double>(r.duration_since_readout_s)};
    output.values[3]={"tpl_status_raw",static_cast<double>(r.tpl_status)};
    output.value_count=4;return PureDecodeStatus::Ok;
  }
  // DWZ Waterstarm: two clear fixed 151-byte upstream public profiles.
  if(driver_name && std::strcmp(driver_name,"waterstarm")==0) {
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    waterstarm::Reading r{};
    if(waterstarm::decode(frame,length,r)!=waterstarm::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"consumption_at_set_date_m3",r.consumption_at_set_date_m3};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // LSE 07/17: original clear short-TPL water, variable recorded fields.
  if(driver_name && std::strcmp(driver_name,"lse_07_17")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    lse_07_17::Reading r{};
    if(lse_07_17::decode(frame,length,r)!=lse_07_17::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    if(r.has_total)output.values[output.value_count++]={"total_m3",r.total_m3};
    if(r.has_due)output.values[output.value_count++]={"due_m3",r.due_m3};
    if(r.has_set8)output.values[output.value_count++]=
       {"consumption_at_set_date_8_m3",r.consumption_at_set_date_8_m3};
    return PureDecodeStatus::Ok;
  }
  // WEP Munia: two public clear-temperature/humidity BCD profiles.
  if(driver_name && std::strcmp(driver_name,"munia")==0) {
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    munia::Reading r{};
    if(munia::decode(frame,length,r)!=munia::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_temperature_c",r.current_temperature_c};
    output.values[1]={"current_relative_humidity_rh",r.current_relative_humidity_rh};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // QHeat 5.5 US: bounded original Mode-0 energy and billing readings.
  if(driver_name && std::strcmp(driver_name,"qheat_55_us")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qheat_55_us::Reading r{};
    if(qheat_55_us::decode(frame,length,r)!=qheat_55_us::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.values[1]={"key_datetime_kwh",r.key_datetime_kwh};
    output.value_count=2;
    if(r.has_total_m3)output.values[output.value_count++]={"total_m3",r.total_m3};
    return PureDecodeStatus::Ok;
  }
  // Maddalena MAD/01/07: source-verified volumes and storage-3 peak.
  if(driver_name && std::strcmp(driver_name,"maddalena")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    maddalena::Reading r{};
    if(maddalena::decode(frame,length,r)!=maddalena::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",r.total_m3};
    output.values[1]={"target_m3",r.target_m3};
    output.values[2]={"storage_2_volume_m3",r.storage_2_volume_m3};
    output.values[3]={"storage_3_max_flow_m3h",r.storage_3_max_flow_m3h};
    output.value_count=4;return PureDecodeStatus::Ok;
  }
  // Itron/Allmess ITW/00/04 Mode0 clear 65-byte profile.
  if(driver_name && std::strcmp(driver_name,"itronheat")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    itronheat::Reading r{};
    if(itronheat::decode(frame,length,r)!=itronheat::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_kwh",r.total_kwh};
    output.values[1]={"total_m3",r.total_m3};
    output.values[2]={"last_year_kwh",r.last_year_kwh};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // EMH eHZP/00/02 original 175-byte TPL Mode0 electricity profile.
  if(driver_name && std::strcmp(driver_name,"ehzp")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    ehzp::Reading r{};
    if(ehzp::decode(frame,length,r)!=ehzp::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.values[1]={"total_energy_production_kwh",r.total_energy_production_kwh};
    output.values[2]={"current_power_consumption_kw",r.current_power_consumption_kw};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // QHeat V2: one original LSE/23/04 short Mode0 47-byte profile.
  if(driver_name && std::strcmp(driver_name,"qheatv2")==0) {
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    qheatv2::Reading r{};
    if(qheatv2::decode(frame,length,r)!=qheatv2::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_kwh",r.total_kwh};
    output.values[1]={"target_year_kwh",r.target_year_kwh};
    output.value_count=2;return PureDecodeStatus::Ok;
  }
  // C5ISF: one source-validated ZRI/88/0D error-state Mode0 frame.
  if(driver_name && std::strcmp(driver_name,"c5isf")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    c5isf::Reading r{};
    if(c5isf::decode(frame,length,r)!=c5isf::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.values[1]={"total_volume_m3",r.total_volume_m3};
    output.values[2]={"total_energy_consumption_last_month_kwh",
                      r.total_energy_consumption_last_month_kwh};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // MAD/00/04 Microclima source-verified clear profiles.
  if(driver_name && std::strcmp(driver_name,"microclima")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    microclima::Reading r{};
    if(microclima::decode(frame,length,r)!=microclima::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[output.value_count++]={"total_energy_consumption_kwh",
                                         r.total_energy_consumption_kwh};
    output.values[output.value_count++]={"total_volume_m3",r.total_volume_m3};
    if(r.has_flow_temperature)
      output.values[output.value_count++]={"flow_temperature_c",r.flow_temperature_c};
    if(r.has_return_temperature)
      output.values[output.value_count++]={"return_temperature_c",r.return_temperature_c};
    return PureDecodeStatus::Ok;
  }
  // ABB/20/02 ABB B23 strict 133-byte source clear-only energy profile.
  if(driver_name && std::strcmp(driver_name,"abbb23")==0) {
    if(!output.values || output.value_capacity<1)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    abbb23::Reading r{};
    if(abbb23::decode(frame,length,r)!=abbb23::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.value_count=1;return PureDecodeStatus::Ok;
  }
  // Ei Electronics EIE/0C/1A: diagnostic counters ONLY.
  // Never fabricate a "no smoke" status, safety reading or live alarm.
  if(driver_name && std::strcmp(driver_name,"ei6500")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    ei6500::Reading r{};
    if(ei6500::decode(frame,length,r)!=ei6500::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"alarm_counter",r.alarm_counter};
    output.values[1]={"removed_counter",r.removed_counter};
    output.values[2]={"test_button_counter",r.test_button_counter};
    output.values[3]={"dust_level_counter",r.dust_level_counter};
    output.value_count=4;return PureDecodeStatus::Ok;
  }
  // Apator E-ITN40 native proprietary CI A0 (unencrypted revisions 09/0D).
  if(driver_name && std::strcmp(driver_name,"apatoreitn40")==0) {
    if(!output.values || output.value_capacity<8)
      return PureDecodeStatus::DecodeFailure;
    if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
       wmbus_pure::publication::Decision::Allowed)
      return PureDecodeStatus::UnsupportedTransport;
    apatoreitn40::Reading r{};
    if(apatoreitn40::decode(frame,length,r)!=apatoreitn40::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_hca",r.current_hca};
    output.values[1]={"previous_hca",r.previous_hca};
    output.values[2]={"previous_2y_hca",r.previous_2y_hca};
    output.values[3]={"previous_3y_hca",r.previous_3y_hca};
    output.values[4]={"current_month_hca",r.current_month_hca};
    output.values[5]={"previous_month_hca",r.previous_month_hca};
    output.values[6]={"temp_room_c",r.temp_room_c};
    output.values[7]={"temp_radiator_c",r.temp_radiator_c};
    output.value_count=8;return PureDecodeStatus::Ok;
  }
  // Source-backed dual-ID long-TPL profiles are ONLY authorized with
  // separately configured DLL sender + verified embedded TPL meter ID.
  // The original single-ID publication policy is never relaxed.
  if(driver_name && std::strcmp(driver_name,"elf")==0) {
    if(!output.values || output.value_capacity<5 || !expected_dll_id)
      return PureDecodeStatus::DecodeFailure;
    elf::Reading r{};
    if(elf::decode(frame,length,configured_id,expected_dll_id,r)!=elf::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.values[1]={"total_volume_m3",r.total_volume_m3};
    output.values[2]={"total_energy_consumption_at_date_kwh",
                       r.total_energy_consumption_at_date_kwh};
    output.values[3]={"flow_temperature_c",r.flow_temperature_c};
    output.values[4]={"return_temperature_c",r.return_temperature_c};
    output.value_count=5;return PureDecodeStatus::Ok;
  }
  if(driver_name && std::strcmp(driver_name,"engelmann_faw")==0) {
    if(!output.values || output.value_capacity<3 || !expected_dll_id)
      return PureDecodeStatus::DecodeFailure;
    engelmann_faw::Reading r{};
    if(engelmann_faw::decode(frame,length,configured_id,expected_dll_id,r)!=
        engelmann_faw::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"consumption_at_reporting_date_m3",
                       r.consumption_at_reporting_date_m3};
    output.values[1]={"consumption_1_months_ago_m3",
                       r.consumption_1_months_ago_m3};
    output.values[2]={"consumption_2_months_ago_m3",
                       r.consumption_2_months_ago_m3};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // Apator E-ITN (not E-ITN40): source-bounded A0 and nested B6,
  // with real proprietary HCA and Q8.8 average temperatures.
  if(driver_name && std::strcmp(driver_name,"apatoreitn")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    apatoreitn::Reading r{};
    if(apatoreitn::decode(frame,length,r)!=apatoreitn::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"current_hca",r.current_hca};
    output.values[1]={"previous_hca",r.previous_hca};
    output.values[2]={"temp_room_avg_c",r.temp_room_avg_c};
    output.values[3]={"temp_room_prev_avg_c",r.temp_room_prev_avg_c};
    output.value_count=4;return PureDecodeStatus::Ok;
  }
  // Gran Systems CI78 is an exact, source-verified NO-TPL application
  // profile. It is NEVER a permission to accept arbitrary CI78 packets.
  if(driver_name && std::strcmp(driver_name,"gransystems")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    gransystems::Reading r{};
    if(gransystems::decode(frame,length,r)!=gransystems::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_energy_consumption_kwh",r.total_energy_consumption_kwh};
    output.values[1]={"total_energy_consumption_tariff_1_kwh",
                      r.total_energy_consumption_tariff_1_kwh};
    output.values[2]={"total_energy_consumption_tariff_2_kwh",
                      r.total_energy_consumption_tariff_2_kwh};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // The A1 Compact5 fixtures remain deliberately unsupported.
  if(driver_name && std::strcmp(driver_name,"compact5")==0) {
    if(!output.values || output.value_capacity<3)
      return PureDecodeStatus::DecodeFailure;
    compact5::Reading r{};
    if(compact5::decode(frame,length,r)!=compact5::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_kwh",r.total_kwh};
    output.values[1]={"current_kwh",r.current_kwh};
    output.values[2]={"previous_kwh",r.previous_kwh};
    output.value_count=3;return PureDecodeStatus::Ok;
  }
  // Vario451: 3 XMQ energy values plus 2 hidden raw counters.
  if(driver_name && std::strcmp(driver_name,"vario451")==0) {
    if(!output.values || output.value_capacity<5)
      return PureDecodeStatus::DecodeFailure;
    vario451::Reading r{};
    if(vario451::decode(frame,length,r)!=vario451::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_kwh",r.total_kwh};
    output.values[1]={"current_kwh",r.current_kwh};
    output.values[2]={"previous_kwh",r.previous_kwh};
    output.values[3]={"prev_raw_counter",static_cast<double>(r.prev_raw_counter)};
    output.values[4]={"curr_raw_counter",static_cast<double>(r.curr_raw_counter)};
    output.value_count=5;return PureDecodeStatus::Ok;
  }
  // Native no-key Ultraheat (LUG) short TPL standard records.
  if(driver_name && std::strcmp(driver_name,"ultraheat")==0) {
    if(!output.values || output.value_capacity<6)
      return PureDecodeStatus::DecodeFailure;
    ultraheat::Reading r{};
    if(ultraheat::decode(frame,length,r)!=ultraheat::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[output.value_count++]={"heat_kwh",r.heat_kwh};
    if(r.volume) output.values[output.value_count++]={"volume_m3",r.volume_m3};
    if(r.power) output.values[output.value_count++]={"power_kw",r.power_kw};
    if(r.flow) output.values[output.value_count++]={"flow_m3h",r.flow_m3h};
    if(r.flow_temp) output.values[output.value_count++]={"flow_c",r.flow_c};
    if(r.return_temp) output.values[output.value_count++]={"return_c",r.return_c};
    return PureDecodeStatus::Ok;
  }
  // Native Sontex868 full source-verified HCA, scalar and vendor fields.
  if(driver_name && std::strcmp(driver_name,"sontex868")==0) {
    if(!output.values || output.value_capacity<13)
      return PureDecodeStatus::DecodeFailure;
    sontex868::Reading r{};
    if(sontex868::decode(frame,length,r)!=sontex868::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[output.value_count++]={"current_consumption_hca",r.current_hca};
    if(r.previous) output.values[output.value_count++]={"consumption_at_set_date_hca",r.previous_hca};
    if(r.flow) output.values[output.value_count++]={"current_temp_c",r.flow_c};
    if(r.room) output.values[output.value_count++]={"current_room_temp_c",r.room_c};
    if(r.max_flow) output.values[output.value_count++]={"max_temp_c",r.max_flow_c};
    if(r.previous_max_flow) output.values[output.value_count++]={"max_temp_previous_period_c",r.previous_max_flow_c};
    output.values[output.value_count++]={"storage_48_consumption_hca",r.storage_48_consumption_hca};
    output.values[output.value_count++]={"tariff_1_duration_minutes",r.tariff_1_duration_minutes};
    output.values[output.value_count++]={"tariff_1_cumulation_counter",r.tariff_1_cumulation_counter};
    output.values[output.value_count++]={"parameter_activation_state_raw",r.parameter_activation_state_raw};
    output.values[output.value_count++]={"manufacturer_ff2d_value",r.manufacturer_ff2d_value};
    output.values[output.value_count++]={"manufacturer_ff2d_tariff_2_value",r.manufacturer_ff2d_tariff_2_value};
    output.values[output.value_count++]={"manufacturer_ff2c_raw",r.manufacturer_ff2c_raw};
    return PureDecodeStatus::Ok;
  }
  // Supercal 739: every XMQ measurement, including storage-20 targets.
  if(driver_name && std::strcmp(driver_name,"supercal")==0) {
    if(!output.values || output.value_capacity<8)
      return PureDecodeStatus::DecodeFailure;
    supercal::Reading r{};
    if(supercal::decode(frame,length,r)!=supercal::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[output.value_count++]={"total_kwh",r.energy_kwh};
    output.values[output.value_count++]={"total_m3",r.volume_m3};
    if(r.power) output.values[output.value_count++]={"power_kw",r.power_kw};
    if(r.flow) output.values[output.value_count++]={"volume_flow_m3h",r.flow_m3h};
    if(r.flow_temp) output.values[output.value_count++]={"flow_temperature_c",r.flow_c};
    if(r.return_temp) output.values[output.value_count++]={"return_temperature_c",r.return_c};
    output.values[output.value_count++]={"target_kwh",r.target_kwh};
    output.values[output.value_count++]={"target_m3",r.target_m3};
    return PureDecodeStatus::Ok;
  }
  // Axioma Q400: only strictly unencrypted short-TPL standard fields.
  if(driver_name && std::strcmp(driver_name,"q400")==0) {
    if(!output.values || output.value_capacity<4)
      return PureDecodeStatus::DecodeFailure;
    q400::Reading r{};
    if(q400::decode(frame,length,r)!=q400::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[output.value_count++]={"total_m3",r.total_m3};
    if(r.flow) output.values[output.value_count++]={"volume_flow_m3h",r.flow_m3h};
    if(r.temperature) output.values[output.value_count++]={"flow_temperature_c",r.temperature_c};
    if(r.battery) output.values[output.value_count++]={"battery_pct",r.battery_pct};
    return PureDecodeStatus::Ok;
  }
  // Batch-2 native application decoding: a real receiver-code path,
  // but deliberately absent from the YAML/compiled SUPPORTED manifest.
  // Caller has already verified the complete DLL CRC and configured ID.
  // Only independently proven clear transport is accepted here; neither
  // original Mode-5 Minomess/Apator ciphertext nor AES-CTR ELL is plaintext.
  const bool is_minomess=driver_name &&
      std::strcmp(driver_name,"minomess")==0;
  const bool is_kamwater=driver_name &&
      std::strcmp(driver_name,"kamwater")==0;
  const bool is_apator=driver_name &&
      std::strcmp(driver_name,"apator162")==0;
  if(is_minomess || is_kamwater || is_apator) {
    if(!output.values || output.value_capacity<2)
      return PureDecodeStatus::DecodeFailure;
    if(is_kamwater) {
      const bool known_kam=
          (header.manufacturer==0x2C2D &&
           ((header.version==0x1B &&
             (header.device_type==0x06 || header.device_type==0x16)) ||
            (header.version==0x1D && header.device_type==0x16))) ||
          (header.manufacturer==0x2C37 &&
           ((header.version==0x3A && header.device_type==0x16) ||
            (header.version==0x3C &&
             (header.device_type==0x16 || header.device_type==0x06)) ||
            (header.version==0x42 &&
             (header.device_type==0x16 || header.device_type==0x06))));
      if(!known_kam || header.ci!=0x8D)
        return PureDecodeStatus::UnsupportedTransport;
      kamwater::pipeline::Output reading{};
      // ELL-II security mode 0 + payload CRC + compact signature/data
      // CRC are checked inside the existing native clear pipeline.
      // Mode 1 AES-CTR is explicitly rejected; no guessed decryption.
      if(kamwater::pipeline::decode_clear_ell_ii(
             frame+10,length-10,{true,true},reading)
         !=kamwater::pipeline::Status::Ok)
        return PureDecodeStatus::DecodeFailure;
      output.values[0]={"total_m3",reading.reading.total_m3};
      output.value_count=1;
      if(reading.reading.has_target)
        output.values[output.value_count++]={"target_m3",
                                             reading.reading.target_m3};
      return PureDecodeStatus::Ok;
    }
    const auto identity=wmbus_pure::identity::inspect(frame,length);
    if(identity.status!=wmbus_pure::identity::Status::Ok ||
       identity.application_offset>=length)
      return PureDecodeStatus::UnsupportedTransport;
    if(is_minomess) {
      // Standard long TPL meter behind a separate DLL radio converter:
      // caller must explicitly configure both addresses independently.
      // Mode 5 from original NOKEY post-decrypted fixtures is NOT clear RF.
      if(header.ci!=0x72 || !identity.has_tpl ||
         !wmbus_pure::identity::same_id(identity.tpl,configured_id) ||
         identity.tpl.manufacturer!=0x6A49 ||
         !((identity.tpl.version==0x00 && identity.tpl.device_type==0x07) ||
           (identity.tpl.version==0x01 &&
            (identity.tpl.device_type==0x06 ||
             identity.tpl.device_type==0x16))) ||
         wmbus_pure::tpl_security::inspect(frame,length).state!=
             wmbus_pure::tpl_security::State::NotEncrypted)
        return PureDecodeStatus::UnsupportedTransport;
    } else {
      // Apator short/long TPL must pass the existing strict cleartext
      // admission gate; no converter exception for this driver.
      if(wmbus_pure::publication::inspect(
            frame,length,header.meter_id)!=
         wmbus_pure::publication::Decision::Allowed)
        return PureDecodeStatus::UnsupportedTransport;
    }
    const uint8_t *application=frame+identity.application_offset;
    const size_t app_length=length-identity.application_offset;
    if(is_minomess) {
      // Keep strict full Minomess history/status shape checks.
      while(application<frame+length && *application==0x2F) {
        ++application;
      }
      minomess::Reading reading{};
      if(minomess::decode_clear_application(
             application,static_cast<size_t>(frame+length-application),
             reading)!=minomess::Status::Ok)
        return PureDecodeStatus::DecodeFailure;
      output.values[0]={"total_m3",reading.total_m3};
      output.value_count=1;
      if(reading.has_target)
        output.values[output.value_count++]={"target_m3",reading.target_m3};
      return PureDecodeStatus::Ok;
    }
    if(header.manufacturer!=0x0601 || header.version!=0x05 ||
       (header.device_type!=0x06 && header.device_type!=0x07))
      return PureDecodeStatus::UnknownDriver;
    apator162::Reading reading{};
    if(apator162::decode_clear_manufacturer(
           application,app_length,reading)!=apator162::Status::Ok)
      return PureDecodeStatus::DecodeFailure;
    output.values[0]={"total_m3",reading.total_m3};
    output.value_count=1;
    return PureDecodeStatus::Ok;
  }
  // This path accepts only the two supported Techem manufacturer CIs.
  // A valid DLL header does NOT prove that a TPL payload is plaintext.
  // In particular CI=72/7A must be processed by a separate, security-aware
  // transport pipeline, never by the generated manufacturer decoders.
  if (header.ci != 0xA0 && header.ci != 0xA2)
    return PureDecodeStatus::UnsupportedTransport;
  const auto *descriptor = find_decode_descriptor(driver_name, header.ci);
  // The upstream Techem 0F HCA fixture uses CI=A2. Allow this precise
  // variant only for the configured heat driver, matching manufacturer,
  // medium, version and the manufacturer-specific 0F prefix.
  // Do not widen the catalog's generic A2 driver selection.
  if (!descriptor && driver_name &&
      std::strcmp(driver_name, "fhkvdataiii") == 0 &&
      header.manufacturer == 0x5068 && header.device_type == 0x80 &&
      header.version == 0x94 && header.ci == 0xA2 &&
      header.application_length > 0 && header.application[0] == 0x0F)
    descriptor = find_decode_descriptor("fhkvdataiii", 0xA0);
  if (!descriptor) return PureDecodeStatus::UnknownDriver;
  if (decode_driver_payload(descriptor->tables, header.application,
                            header.application_length, captures,
                            capture_capacity, output) != DriverStatus::Ok)
    return PureDecodeStatus::DecodeFailure;
  // Preserve original XMQ output names, and expose the official
  // wmbusmeters JSON names as optional aliases for ESPHome sensor bindings.
  // Alias creation is bounded; existing readings are never overwritten.
  if (driver_name && std::strcmp(driver_name, "mkradio4") == 0 && output.values) {
    const size_t original_count = output.value_count;
    for (size_t i = 0; i < original_count; ++i) {
      const char *alias = nullptr;
      if (output.values[i].name &&
          std::strcmp(output.values[i].name, "total") == 0)
        alias = "total_m3";
      else if (output.values[i].name &&
               std::strcmp(output.values[i].name, "target") == 0)
        alias = "target_m3";
      if (alias && output.value_count < output.value_capacity) {
        output.values[output.value_count++] = {alias, output.values[i].value};
      }
    }
  }
  return PureDecodeStatus::Ok;
}
}  // namespace xmq_portable
