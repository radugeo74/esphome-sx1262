#pragma once
// Mode 5 preparation: select the right on-wire meter address, validate
// encrypted-block count, and build the IV. No plaintext is published here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "pure_frame_header.h"
#include "pure_transport.h"
#include "pure_security.h"
#include "pure_mode5_iv.h"

namespace wmbus_pure {
enum class Mode5PrepareStatus : uint8_t {
  Ok, UnsupportedSecurity, InvalidHeader, InvalidBlockCount, TruncatedPayload
};
struct Mode5Prepared {
  uint8_t iv[16]{};
  const uint8_t *encrypted{nullptr};
  size_t encrypted_size{0};
  const uint8_t *unencrypted_tail{nullptr};
  size_t unencrypted_tail_size{0};
};
inline Mode5PrepareStatus prepare_mode5(const uint8_t *frame, size_t size,
                                         const Header &header,
                                         const TransportEnvelope &transport,
                                         Mode5Prepared &out) {
  out = {};
  if (inspect_tpl_security(transport).kind != SecurityKind::AesCbcIvMode5)
    return Mode5PrepareStatus::UnsupportedSecurity;
  if (!frame || size < 11 || header.application != frame + 11 ||
      header.application_length != size - 11 || !transport.payload)
    return Mode5PrepareStatus::InvalidHeader;
  const auto security = inspect_tpl_security(transport);
  if (security.encrypted_blocks == 0) return Mode5PrepareStatus::InvalidBlockCount;
  const size_t encrypted_size = static_cast<size_t>(security.encrypted_blocks) * 16;
  if (transport.payload_size < encrypted_size) return Mode5PrepareStatus::TruncatedPayload;
  uint8_t address[8]{};
  if (transport.kind == TransportKind::Short) {
    // DLL: manufacturer(2), ID(4), version, media, in transmitted order.
    std::memcpy(address, frame + 2, 8);
  } else if (transport.kind == TransportKind::Long) {
    // TPL long address is ID(4), manufacturer(2), version, media.
    // CBC IV uses manufacturer first, followed by ID, version, media.
    if (header.application_length < 12)
      return Mode5PrepareStatus::InvalidHeader;
    const uint8_t *tpl = header.application;
    std::memcpy(address, tpl + 4, 2);
    std::memcpy(address + 2, tpl, 4);
    std::memcpy(address + 6, tpl + 6, 2);
  } else {
    return Mode5PrepareStatus::UnsupportedSecurity;
  }
  if (build_mode5_iv(address, sizeof(address), transport.access_number,
                     out.iv, sizeof(out.iv)) != Mode5IvStatus::Ok)
    return Mode5PrepareStatus::InvalidHeader;
  out.encrypted = transport.payload;
  out.encrypted_size = encrypted_size;
  out.unencrypted_tail = transport.payload + encrypted_size;
  out.unencrypted_tail_size = transport.payload_size - encrypted_size;
  return Mode5PrepareStatus::Ok;
}
} // namespace wmbus_pure
