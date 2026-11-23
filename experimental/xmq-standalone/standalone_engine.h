#pragma once
// Experimental no-legacy XMQ application processor. NO radio, no wmbus_common,
// no Telegram::parse / handleTelegram, no legacy snapshot and no HA writer.
// The independent transport/security adaptor must establish trusted plaintext.
#include "../xmq-runtime-stage-a/xmq_primary_readiness.h"
#include "../xmq-runtime-stage-a/components/xmq_shadow_probe/xmq_driver_sources.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xmq_standalone {
enum class Family : std::uint8_t { Heat, Water };
enum class Trust : std::uint8_t { Unknown, Ciphertext, VerifiedCleartext };
struct Input {
  Family family;
  xmq_runtime::Mvt inner_mvt;
  xmq_runtime::ApplicationEncoding encoding;
  std::vector<std::uint8_t> application;
  Trust trust{Trust::Unknown};
};
inline bool decode(const Input &input, xmq_runtime::XmqFrameResult &out) noexcept {
  out = {};
  if (input.trust != Trust::VerifiedCleartext) return false;
  try {
    if (input.family == Family::Heat) {
      static const xmq_runtime::DriverRegistry registry({
          xmq_runtime::load_driver(xmq_runtime::source_fhkvdataiii())});
      return xmq_runtime::decode_primary_candidate(
          registry, input.inner_mvt, input.encoding, input.application,
          "fhkvdataiii",
          {"current_hca", "previous_hca", "temp_room_c", "temp_radiator_c"},
          {}, out);
    }
    if (input.family == Family::Water) {
      static const xmq_runtime::DriverRegistry registry({
          xmq_runtime::load_driver(xmq_runtime::source_mkradio4())});
      return xmq_runtime::decode_primary_candidate(
          registry, input.inner_mvt, input.encoding, input.application,
          "mkradio4", {"target_m3", "total_m3"}, {}, out);
    }
  } catch (...) {
    out = {};
  }
  return false;
}
} // namespace xmq_standalone
