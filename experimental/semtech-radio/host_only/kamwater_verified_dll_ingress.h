#pragma once
// Isolated host-only Kamwater ingress: verify the complete DLL frame and
// match its address against an explicitly configured meter before admitting
// clear ELL II / TPL79 compact data. No live dispatch, decryption or publish.
// The configured identity is a filter, not cryptographic authentication.
#include <cstddef>
#include <cstdint>
#include "../../../components/wmbus_radio/pure_complete_frame.h"
#include "../../../components/wmbus_native_sx126x/kamwater_clear_pipeline.h"
namespace xmq_portable { namespace kamwater { namespace ingress {
struct ExpectedMeter {
  uint16_t manufacturer{0};
  uint32_t meter_id{0};
  uint8_t version{0};
  uint8_t device_type{0};
};
// Shared host-only exact configuration filter for clear/encrypted tests.
// The caller must separately verify the DLL CRC before trusting this header.
inline bool matches_expected(const wmbus_pure::Header &h,
                             const ExpectedMeter &expected) {
  return h.manufacturer==expected.manufacturer &&
         h.meter_id==expected.meter_id &&
         h.version==expected.version &&
         h.device_type==expected.device_type;
}
enum class Status : uint8_t {
  Ok, DllRejected, IdentityMismatch, WrongCi, ClearPipelineRejected
};
struct Output {
  kamwater::Reading reading{};
  int profile_index{-1};
};
inline Status decode_verified_dll(
    const uint8_t *raw, size_t raw_length,
    wmbus_pure::CrcFrameFormat frame_format,
    const ExpectedMeter &expected, Output &out) {
  out={};
  wmbus_pure::CompleteFrame verified{};
  if(wmbus_pure::verify_complete_frame(
      raw,raw_length,frame_format,verified)!=wmbus_pure::CompleteStatus::Ok)
    return Status::DllRejected;
  const auto &h=verified.header;
  if(!matches_expected(h,expected)) return Status::IdentityMismatch;
  if(h.ci!=0x8D) return Status::WrongCi;
  // Header.application begins AFTER CI; the ELL parser needs CI itself.
  pipeline::Output decoded{};
  if(pipeline::decode_clear_ell_ii_tpl79(
      verified.bytes.data()+10,verified.bytes.size()-10,
      pipeline::Preconditions{true,true},decoded)!=pipeline::Status::Ok)
    return Status::ClearPipelineRejected;
  out.reading=decoded.reading;
  out.profile_index=decoded.profile_index;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::ingress
