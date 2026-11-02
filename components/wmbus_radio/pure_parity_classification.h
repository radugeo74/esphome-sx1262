#pragma once
#include <cstdint>
#include "pure_complete_frame.h"

namespace wmbus_pure {
enum class ParityMismatch : uint8_t {
  None, BadCrcLegacyAccepted, InvalidHeaderLegacyAccepted,
  InvalidStructureLegacyAccepted, ContentMismatch, PureOnlyAccepted
};
inline ParityMismatch classify_parity(CompleteStatus status,
                                     bool legacy_accepted, bool identical) {
  const bool pure_accepted = status == CompleteStatus::Ok;
  if (pure_accepted && legacy_accepted)
    return identical ? ParityMismatch::None : ParityMismatch::ContentMismatch;
  if (pure_accepted) return ParityMismatch::PureOnlyAccepted;
  if (!legacy_accepted) return ParityMismatch::None;
  if (status == CompleteStatus::BadCrc) return ParityMismatch::BadCrcLegacyAccepted;
  if (status == CompleteStatus::UnsupportedHeader)
    return ParityMismatch::InvalidHeaderLegacyAccepted;
  return ParityMismatch::InvalidStructureLegacyAccepted;
}
} // namespace wmbus_pure
