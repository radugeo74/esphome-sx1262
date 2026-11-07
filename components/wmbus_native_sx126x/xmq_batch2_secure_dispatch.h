#pragma once
// Native driver entrypoint for configured Apator162 encrypted short TPL.
// This is usable only AFTER the official radio owner has verified DLL CRC.
// It is not registered for YAML/live RF while batch-2 remains pending.
// No key storage, no sensor publication, and no legacy wmbusmeters decoder.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "driver_runtime.h"
#include "oms_mode5_mbedtls.h"
#include "apator162_ixml_fields.h"
#include "pure_meter_match.h"

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
}} // namespace xmq_portable::batch2_secure
