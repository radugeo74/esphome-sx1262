#pragma once
// Native driver entrypoint for configured Apator162 encrypted short TPL.
// This is usable only AFTER the official radio owner has verified DLL CRC.
// It is not registered for YAML/live RF while batch-2 remains pending.
// No key storage, no sensor publication, and no legacy wmbusmeters decoder.
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include "driver_runtime.h"
#include "oms_mode5_mbedtls.h"
#include "apator162_ixml_fields.h"
#include "pure_meter_match.h"
#include "kamwater_ell_ctr_mbedtls.h"
#include "kamwater_tpl79_probe.h"
#include "kamwater_clear_records.h"
#include "minomess_clear_prefix.h"
#include "mkradio4a_plain.h"
#include "native_identity_probe.h"
#include "oms_ccm_mbedtls.h"
#include "kamwater_numeric_semantics.h"
#include "kamwater_authenticated_tpl_status.h"

namespace xmq_portable { namespace batch2_secure {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongMeter, WrongModel, MissingCapacity,
  EncryptedDecodeRejected, ApplicationRejected
};
inline Status decode_apator162_mode5(const uint8_t *frame,size_t length,
                                      const char *configured_id,
                                      const uint8_t key[16],
                                      DecodedPayload &output) {
  output.value_count=0;
  output.date_count=0;
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
      wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(header,configured_id)!=
      wmbus_pure::MatchStatus::Match)
    return Status::WrongMeter;
  // Official Apator162 detects APA/05/06 and APA/05/07.
  if(header.manufacturer!=0x0601 || header.version!=0x05 ||
     (header.device_type!=0x06 && header.device_type!=0x07))
    return Status::WrongModel;
  if(!output.values || output.value_capacity<1)
    return Status::MissingCapacity;
  oms_mode5::Output clear{};
  if(oms_mode5::decrypt_short_tpl(frame,length,key,clear)!=
      oms_mode5::Status::Ok)
    return Status::EncryptedDecodeRejected;
  apator162::Reading reading{};
  if(apator162::decode_clear_manufacturer(
         clear.clear_application.data(),clear.clear_application.size(),
         reading)!=apator162::Status::Ok ||
     !std::isfinite(reading.total_m3) || reading.total_m3<0)
    return Status::ApplicationRejected;
  output.values[0]={"total_m3",reading.total_m3};
  output.value_count=1;
  return Status::Ok;
}
// Kamwater KAM/KAW ELL-II AES-CTR to compact/full DIF/VIF values.
// Only the official model signatures are accepted. This is not a MAC;
// ELL CRC and compact DIF/VIF signature/data CRC provide error detection.
// Never infer the decryption key or publish values from an unknown meter.
inline Status decode_kamwater_ell_ctr(const uint8_t *frame,size_t length,
                                      const char *configured_id,
                                      const uint8_t key[16],
                                      DecodedPayload &output) {
  output.value_count=0;
  output.date_count=0;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(h,configured_id)!=
      wmbus_pure::MatchStatus::Match)
    return Status::WrongMeter;
  const bool known=
      (h.manufacturer==0x2C2D &&
       ((h.version==0x1B &&
          (h.device_type==0x06 || h.device_type==0x16)) ||
        (h.version==0x1D && h.device_type==0x16))) ||
      (h.manufacturer==0x2C37 &&
       ((h.version==0x3A && h.device_type==0x16) ||
        (h.version==0x3C &&
          (h.device_type==0x06 || h.device_type==0x16)) ||
        (h.version==0x42 &&
          (h.device_type==0x06 || h.device_type==0x16))));
  if(!known || h.ci!=0x8D) return Status::WrongModel;
  if(!output.values || output.value_capacity<2)
    return Status::MissingCapacity;
  std::vector<uint8_t> tpl{};
  if(kamwater::ell_ctr_mbedtls::decrypt_ell_ii(
         frame+2,frame+4,key,frame+10,length-10,tpl)!=
         kamwater::ell_ctr_mbedtls::Status::Ok)
    return Status::EncryptedDecodeRejected;
  kamwater::Reading reading{};
  if(tpl[0]==0x79) {
    uint8_t expanded[128]{};
    kamwater::tpl79::Reading profile{};
    if(kamwater::tpl79::verify(
           tpl.data(),tpl.size(),expanded,sizeof(expanded),profile)!=
         kamwater::tpl79::Status::Ok ||
       kamwater::decode_clear_records(
           expanded,profile.expanded_size,reading)!=kamwater::Status::Ok)
      return Status::ApplicationRejected;
  } else if(tpl[0]==0x78) {
    if(kamwater::decode_clear_records(
           tpl.data()+1,tpl.size()-1,reading)!=kamwater::Status::Ok)
      return Status::ApplicationRejected;
  } else return Status::ApplicationRejected;
  if(!std::isfinite(reading.total_m3) || reading.total_m3<0 ||
     (reading.has_target &&
      (!std::isfinite(reading.target_m3) || reading.target_m3<0)))
    return Status::ApplicationRejected;
  output.values[0]={"total_m3",reading.total_m3};
  output.value_count=1;
  if(reading.has_target)
    output.values[output.value_count++]={"target_m3",reading.target_m3};
  return Status::Ok;
}
// Narrow ORIGINAL Kamwater #10: OMS security profile D / short TPL,
// with CI=8C, CC=20, ACC=30 and an authenticated mode-10 TPL.
// Input must be a CRC-verified, CRC-stripped DLL frame from SX126x.
// The underlying shared mbedTLS CMAC/CCM code is also used by host tests.
// We only authorize the exact KAW/3C/16 model and authenticated TPL
// status observed in original public #10; extend with independent fixtures.
inline Status decode_kamwater_oms_ccm(const uint8_t *frame,size_t length,
                                     const char *configured_id,
                                     const uint8_t key[16],
                                     DecodedPayload &output,
                                     const char **out_status) {
  output.value_count=0;
  output.date_count=0;
  if(out_status) *out_status=nullptr;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
      wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(h,configured_id)!=
      wmbus_pure::MatchStatus::Match)
    return Status::WrongMeter;
  if(h.manufacturer!=0x2C37 || h.version!=0x3C ||
     h.device_type!=0x16 || h.ci!=0x8C ||
     length<14 || frame[11]!=0x20 ||
     frame[12]!=0x30 || frame[13]!=0x7A)
    return Status::WrongModel;
  if(!output.values || output.value_capacity<2)
    return Status::MissingCapacity;
  oms_ccm_idf_probe::Output clear{};
  if(oms_ccm_idf_probe::decode_short_tpl(
       frame+2,frame+4,h.version,h.device_type,key,
       frame+13,length-13,clear)!=oms_ccm_idf_probe::Status::Ok)
    return Status::EncryptedDecodeRejected;
  kamwater::Reading readings{};
  if(kamwater::decode_clear_records(
       clear.clear_application.data(),clear.clear_application.size(),
       readings)!=kamwater::Status::Ok ||
     !readings.has_target ||
     !std::isfinite(readings.total_m3) || readings.total_m3<0 ||
     !std::isfinite(readings.target_m3) || readings.target_m3<0)
    return Status::ApplicationRejected;
  kamwater::numeric_semantics::Reading diagnostic{};
  if(kamwater::numeric_semantics::decode(
       clear.clear_application.data(),clear.clear_application.size(),
       diagnostic)!=kamwater::numeric_semantics::Status::Ok ||
     !diagnostic.has_acoustic_noise)
    return Status::ApplicationRejected;
  std::string text{};
  if(kamwater::authenticated_status::decode_original_ccm_status(
       clear.tpl_status,text)!=
     kamwater::authenticated_status::Status::Ok ||
     text!="TEMPORARY_ERROR")
    return Status::ApplicationRejected;
  output.values[0]={"total_m3",readings.total_m3};
  output.values[1]={"target_m3",readings.target_m3};
  output.value_count=2;
  // A compile-time literal is handed to the sensor publisher only after
  // all cryptographic, identity, semantic, and status gates succeed.
  if(out_status) *out_status="TEMPORARY_ERROR";
  return Status::Ok;
}
// Minomess may be behind a separate converter DLL address. Require
// both IDs to be independently and explicitly bound: expected DLL ID
// authorizes the radio gateway; configured_id authorizes the nested TPL.
// Without an explicit expected DLL ID, both must still match.
// Never interpret an upstream NOKEY plaintext fixture as encrypted RF.
inline Status decode_minomess_mode5(const uint8_t *frame,size_t length,
                                     const char *configured_id,
                                     const uint8_t key[16],
                                     DecodedPayload &output,
                                     const char *expected_dll_id=nullptr) {
  output.value_count=0;
  output.date_count=0;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(
        h,expected_dll_id ? expected_dll_id : configured_id)!=
     wmbus_pure::MatchStatus::Match)
    return Status::WrongMeter;
  const auto identity=wmbus_pure::identity::inspect(frame,length);
  if(h.ci!=0x72 || identity.status!=wmbus_pure::identity::Status::Ok ||
     !identity.has_tpl ||
     !wmbus_pure::identity::same_id(identity.tpl,configured_id) ||
     identity.tpl.manufacturer!=0x6A49 ||
     !((identity.tpl.version==0x00 &&
        identity.tpl.device_type==0x07) ||
       (identity.tpl.version==0x01 &&
        (identity.tpl.device_type==0x06 ||
         identity.tpl.device_type==0x16))))
    return Status::WrongModel;
  if(!output.values || output.value_capacity<2)
    return Status::MissingCapacity;
  oms_mode5::Output clear{};
  if(oms_mode5::decrypt_long_tpl(frame,length,key,clear)!=
      oms_mode5::Status::Ok)
    return Status::EncryptedDecodeRejected;
  minomess::Reading reading{};
  if(minomess::decode_clear_application(
         clear.clear_application.data(),clear.clear_application.size(),
         reading)!=minomess::Status::Ok ||
     !std::isfinite(reading.total_m3) || reading.total_m3<0 ||
     (reading.has_target &&
      (!std::isfinite(reading.target_m3) || reading.target_m3<0)))
    return Status::ApplicationRejected;
  output.values[0]={"total_m3",reading.total_m3};
  output.value_count=1;
  if(reading.has_target)
    output.values[output.value_count++]={"target_m3",reading.target_m3};
  return Status::Ok;
}
// MK Radio 4a: TCH 95/37 radio-converter DLL carrying HYD FE/06
// long TPL. Public upstream NOKEY fixtures contain post-decrypt
// application data, not complete trustworthy on-air plaintext.
// This narrow encrypted path requires an explicitly configured AES key
// and BOTH identities. It consumes the original 3-block Mode-5 format;
// the five unencrypted trailing bytes are NOT application measurements.
// CBC has no authentication: marker + full original application grammar
// are required but are NOT a MAC.
inline Status decode_mkradio4a_mode5(const uint8_t *frame,size_t length,
                                    const char *configured_id,
                                    const uint8_t key[16],
                                    DecodedPayload &output,
                                    const char *expected_dll_id) {
  output.value_count=0;
  output.date_count=0;
  if(!frame || !configured_id || !expected_dll_id || !key)
    return Status::InvalidFrame;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok) return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(h,expected_dll_id)!=
     wmbus_pure::MatchStatus::Match) return Status::WrongMeter;
  if(h.manufacturer!=0x5068 || h.version!=0x95 ||
     h.device_type!=0x37 || h.ci!=0x72 ||
     length!=76 || frame[21]!=0x30 || frame[22]!=0x05)
    return Status::WrongModel;
  const auto address=wmbus_pure::identity::inspect(frame,length);
  if(address.status!=wmbus_pure::identity::Status::Ok ||
     !address.has_tpl || !address.tpl.decimal_bcd ||
     !wmbus_pure::identity::same_id(address.tpl,configured_id) ||
     !((address.tpl.manufacturer==0x2324 &&
        address.tpl.version==0xFE && address.tpl.device_type==0x06) ||
       (address.tpl.manufacturer==0x5CA8 &&
        address.tpl.version==0xFE && address.tpl.device_type==0x07)))
    return Status::WrongModel;
  if(!output.values || output.value_capacity<1 ||
     !output.dates || output.date_capacity<1)
    return Status::MissingCapacity;
  // Original upstream fixture #1 uses HYD/FE/06 (warm), while #2 uses
  // WEH/FE/07 (cold) behind the same TCH/95/37 DLL model.
  // The upstream frames have 3 encrypted blocks (48 bytes) followed by
  // a 5-byte non-application trailer. The existing shared long-TPL
  // decryptor accepts only the authenticated-by-structure block region.
  // Adapt a PRIVATE COPY; never change the verified original RF frame.
  std::vector<uint8_t> crypt(frame,frame+71);
  crypt[0]=static_cast<uint8_t>(crypt.size()-1);
  oms_mode5::Output clear{};
  if(oms_mode5::decrypt_long_tpl(
       crypt.data(),crypt.size(),key,clear)!=oms_mode5::Status::Ok)
    return Status::EncryptedDecodeRejected;
  mkradio4a::Reading water{};
  if(mkradio4a::decode_clear_application(
        clear.clear_application.data(),clear.clear_application.size(),
        water)!=mkradio4a::Status::Ok ||
     !std::isfinite(water.target_m3) || water.target_m3<0)
    return Status::ApplicationRejected;
  output.values[0]={"target_m3",water.target_m3};
  output.dates[0]={water.year,water.month,water.day};
  output.value_count=1;
  output.date_count=1;
  return Status::Ok;
}
// Explicit only: the configured batch-2 driver and its own key must
// already be bound to this meter in YAML. AUTO and trial-decryption are
// intentionally forbidden. Input is CRC-stripped AFTER verified::process.
inline Status decode_configured(const uint8_t *frame,size_t length,
                                const char *configured_id,
                                const char *driver,const uint8_t key[16],
                                DecodedPayload &output,
                                const char **out_status=nullptr,
                                const char *expected_dll_id=nullptr) {
  output.value_count=0;
  output.date_count=0;
  if(out_status) *out_status=nullptr;
  if(!driver || !key || !frame || length<11)
    return Status::EncryptedDecodeRejected;
  if(std::strcmp(driver,"mkradio4a")==0)
    return decode_mkradio4a_mode5(
        frame,length,configured_id,key,output,expected_dll_id);
  if(std::strcmp(driver,"apator162")==0)
    return decode_apator162_mode5(frame,length,configured_id,key,output);
  if(std::strcmp(driver,"minomess")==0)
    return decode_minomess_mode5(frame,length,configured_id,key,
                                  output,expected_dll_id);
  if(std::strcmp(driver,"kamwater")==0) {
    // Distinct ELL AES-CTR (8D) versus authenticated OMS CCM (8C).
    // Never retry a different security mode when authentication fails.
    if(frame[10]==0x8D)
      return decode_kamwater_ell_ctr(frame,length,configured_id,key,output);
    if(frame[10]==0x8C)
      return decode_kamwater_oms_ccm(frame,length,configured_id,key,
                                     output,out_status);
  }
  return Status::WrongModel;
}
}} // namespace xmq_portable::batch2_secure
