#pragma once
// EN 13757 TPL security mode 5 CBC IV constructor.
// The caller must select the effective eight-byte meter address (DLL or TPL)
// and supply its exact on-wire order. Never log keys or decrypted payloads.
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace wmbus_pure {
enum class Mode5IvStatus : uint8_t { Ok, InvalidInput };
inline Mode5IvStatus build_mode5_iv(const uint8_t *address, size_t address_length,
                                    uint8_t access_number, uint8_t *iv,
                                    size_t iv_length) {
  if (!address || address_length != 8 || !iv || iv_length != 16)
    return Mode5IvStatus::InvalidInput;
  std::memcpy(iv, address, 8);
  std::memset(iv + 8, access_number, 8);
  return Mode5IvStatus::Ok;
}
}  // namespace wmbus_pure
