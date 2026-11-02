#pragma once
// Bounded transport-layer envelope reader. It does not decrypt or classify
// encryption modes: the configuration word remains opaque at this stage.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"

namespace wmbus_pure {
enum class TransportKind : uint8_t { Short, Long, ManufacturerSpecific, Unsupported };
enum class TransportStatus : uint8_t { Ok, Truncated, UnsupportedCi };
struct TransportEnvelope {
  TransportKind kind{TransportKind::Unsupported};
  uint8_t ci{0};
  uint8_t access_number{0};
  uint8_t status{0};
  uint16_t configuration{0};
  const uint8_t *payload{nullptr};
  size_t payload_size{0};
};
inline TransportStatus inspect_transport(const Header &header, TransportEnvelope &out) {
  out = {};
  out.ci = header.ci;
  const uint8_t *data = header.application;
  const size_t length = header.application_length;
  if (!data && length != 0) return TransportStatus::Truncated;
  size_t offset = 0;
  if (header.ci == 0x7A) {
    out.kind = TransportKind::Short;
    offset = 4; // ACC, STS, CONFIG (little-endian)
  } else if (header.ci == 0x72) {
    out.kind = TransportKind::Long;
    offset = 12; // ID(4), manufacturer(2), version, media, ACC, STS, CONFIG
  } else if (header.ci == 0xA0) {
    out.kind = TransportKind::ManufacturerSpecific;
    // No generic TPL header can be assumed for this CI.
  } else {
    return TransportStatus::UnsupportedCi;
  }
  if (length < offset) return TransportStatus::Truncated;
  if (offset) {
    const size_t base = offset - 4;
    out.access_number = data[base];
    out.status = data[base + 1];
    out.configuration = static_cast<uint16_t>(data[base + 2]) |
                        (static_cast<uint16_t>(data[base + 3]) << 8);
  }
  out.payload = data ? data + offset : nullptr;
  out.payload_size = length - offset;
  return TransportStatus::Ok;
}
} // namespace wmbus_pure
