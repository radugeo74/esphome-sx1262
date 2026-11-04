#pragma once
// AUTO chooses a driver only for configured meters, from verified DLL metadata.
// It never guesses across ambiguity or claims that metadata is a decryption key.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "pure_driver_registry.h"
namespace wmbus_pure { namespace auto_driver {
enum class Status : uint8_t { Ready, Unknown, Ambiguous, NotCompiled, Invalid };
struct Result { Status status; const char *driver; };
inline Result resolve(const char *requested, const DriverSignature *signatures,
                      size_t count, uint16_t manufacturer, uint8_t version,
                      uint8_t type) {
  if (!requested || !requested[0]) return {Status::Invalid, nullptr};
  if (std::strcmp(requested, "auto") != 0)
    return {Status::Ready, requested};  // existing explicit decode path
  auto match = select_driver(signatures, count, manufacturer, version, type);
  if (match.status == DriverSelectionStatus::Ambiguous)
    return {Status::Ambiguous, nullptr};
  if (match.status == DriverSelectionStatus::Unknown)
    return {Status::Unknown, nullptr};
  if (match.status != DriverSelectionStatus::Found || !match.driver)
    return {Status::Invalid, nullptr};
  // Only these decoders are currently generated and validated on Heltec.
  if (std::strcmp(match.driver, "mkradio4") != 0 &&
      std::strcmp(match.driver, "fhkvdataiii") != 0 &&
      std::strcmp(match.driver, "iperl") != 0 &&
      std::strcmp(match.driver, "hydrus") != 0)
    return {Status::NotCompiled, nullptr};
  return {Status::Ready, match.driver};
}
} }
