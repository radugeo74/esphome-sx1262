#pragma once
// Experimental configured Kamwater CCM profile-D fixture field mapping.
// This is an in-memory, TRANSACTIONAL decoder, not a sensor publisher.
// No ESPHome receiver owns or includes it; source #10 subset only.
// Transport authentication and configured DLL identity are mandatory.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include "oms_ccm_verified_ingress.h"
#include "../../../../components/wmbus_native_sx126x/kamwater_clear_records.h"
#include "../../../../components/wmbus_native_sx126x/kamwater_numeric_semantics.h"
#include "../../../semtech-radio/host_only/kamwater_authenticated_tpl_status.h"

namespace xmq_portable { namespace kamwater { namespace configured_fields {
enum class Status : uint8_t {
  Ok, IngressRejected, ApplicationRejected, UnsupportedStatus
};
struct Reading {
  bool has_total{false};
  double total_m3{0};
  bool has_target{false};
  double target_m3{0};
  std::string status{};
  std::string acoustic_noise{};
  uint32_t authenticated_counter{0}; // diagnostics, never sensor value
};
inline Status decode_format_b(
    const uint8_t *frame, size_t length,
    const oms_ccm_idf_probe::ingress::ConfiguredMeter &configured,
    const uint8_t key[16], Reading &out) {
  out={};
  oms_ccm_idf_probe::Output authenticated{};
  if (oms_ccm_idf_probe::ingress::decode_format_b(
          frame,length,configured,key,authenticated)
      != oms_ccm_idf_probe::ingress::Status::Ok)
    return Status::IngressRejected;
  Reading staged{};
  kamwater::Reading volumes{};
  if (kamwater::decode_clear_records(authenticated.clear_application.data(),
                                    authenticated.clear_application.size(),
                                    volumes)!=kamwater::Status::Ok ||
      !std::isfinite(volumes.total_m3) || !volumes.has_target ||
      !std::isfinite(volumes.target_m3))
    return Status::ApplicationRejected;
  numeric_semantics::Reading diagnostic{};
  if (numeric_semantics::decode(authenticated.clear_application.data(),
                                authenticated.clear_application.size(),
                                diagnostic)!=numeric_semantics::Status::Ok ||
      !diagnostic.has_acoustic_noise)
    return Status::ApplicationRejected;
  if (authenticated_status::decode_original_ccm_status(
          authenticated.tpl_status,staged.status)!=
      authenticated_status::Status::Ok)
    return Status::UnsupportedStatus;
  staged.has_total=true;
  staged.total_m3=volumes.total_m3;
  staged.has_target=true;
  staged.target_m3=volumes.target_m3;
  staged.acoustic_noise=diagnostic.acoustic_noise;
  staged.authenticated_counter=authenticated.counter;
  out=std::move(staged);
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::configured_fields
