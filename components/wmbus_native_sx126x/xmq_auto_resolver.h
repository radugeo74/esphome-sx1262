#pragma once
// Read-only driver resolution. Auto never guesses across ambiguous signatures
// and never routes to a decoder that is not compiled into this firmware.
#include <cstdint>
#include <cstring>
#include "pure_driver_registry.h"

namespace wmbus_pure {
namespace xmq_auto {
enum class Status : uint8_t { Selected, Unknown, Ambiguous, Unsupported, Invalid };
struct Result { Status status; const char *driver; };
inline bool compiled_decoder(const char *name) {
  return name && (std::strcmp(name, "mkradio4") == 0 ||
                  std::strcmp(name, "fhkvdataiii") == 0);
}
inline Result resolve(const char *requested,
                      const DriverSignature *signatures, size_t count,
                      uint16_t manufacturer, uint8_t version, uint8_t medium) {
  if (!requested || !requested[0]) return {Status::Invalid, nullptr};
  if (std::strcmp(requested, "auto") != 0) {
    return compiled_decoder(requested)
        ? Result{Status::Selected, requested}
        : Result{Status::Unsupported, nullptr};
  }
  const auto match = select_driver(signatures, count, manufacturer, version, medium);
  if (match.status == DriverSelectionStatus::Ambiguous)
    return {Status::Ambiguous, nullptr};
  if (match.status == DriverSelectionStatus::Unknown)
    return {Status::Unknown, nullptr};
  if (match.status != DriverSelectionStatus::Found || !match.driver)
    return {Status::Invalid, nullptr};
  return compiled_decoder(match.driver)
      ? Result{Status::Selected, match.driver}
      : Result{Status::Unsupported, nullptr};
}
}  // namespace xmq_auto
}  // namespace wmbus_pure
