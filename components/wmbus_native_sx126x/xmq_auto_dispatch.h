#pragma once
// AUTO chooses a driver only for configured meters, from verified DLL metadata.
// It never guesses across ambiguity or claims that metadata is a decryption key.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "pure_driver_registry.h"
namespace wmbus_pure { namespace auto_driver {
#include "xmq_native_compiled.h"
// Passive recognition is intentionally independent of decoder availability.
// A positive match is NOT a permission to decrypt, select a key, publish
// sensors, or bypass TPL/ELL security. The live AUTO decoder still calls
// resolve(), which returns NotCompiled for secure-only drivers.
enum class RecognitionStatus : uint8_t { Candidate, Unknown, Ambiguous, Invalid };
struct Recognition {
  RecognitionStatus status{RecognitionStatus::Invalid};
  const char *driver{nullptr};
  bool compiled_plaintext{false};
};
inline Recognition identify(const DriverSignature *signatures, size_t count,
                             uint16_t manufacturer, uint8_t version,
                             uint8_t type) {
  const auto match=select_driver(signatures,count,manufacturer,version,type);
  if(match.status==DriverSelectionStatus::Ambiguous)
    return {RecognitionStatus::Ambiguous,nullptr,false};
  if(match.status==DriverSelectionStatus::Unknown)
    return {RecognitionStatus::Unknown,nullptr,false};
  if(match.status!=DriverSelectionStatus::Found || !match.driver)
    return {RecognitionStatus::Invalid,nullptr,false};
  return {RecognitionStatus::Candidate,match.driver,
          native_xmq_driver_compiled(match.driver)};
}
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
  // The generated compiled-driver list is derived from the same manifest
  // as XMQ descriptor generation and ESPHome schema validation.
  if (!native_xmq_driver_compiled(match.driver))
    return {Status::NotCompiled, nullptr};
  return {Status::Ready, match.driver};
}
} }
