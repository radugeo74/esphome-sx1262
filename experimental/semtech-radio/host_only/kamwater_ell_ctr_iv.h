#pragma once
// Host-only, keyless ELL AES-CTR IV derivation following wmbusmeters
// src/wmbus_utils.cc decrypt_ELL_AES_CTR. Not a decryptor and never a
// reason to admit or publish AES-CTR application data.
#include <cstddef>
#include <cstdint>
namespace xmq_portable { namespace kamwater { namespace ell_ctr {
enum class Status : uint8_t { Ok, InvalidInput, UnsupportedCi, NotAesCtr };
struct Iv { uint8_t bytes[16]{}; };
inline Status derive_iv(const uint8_t manufacturer_le[2],
                        const uint8_t meter_address[6],
                        const uint8_t *ell, size_t ell_length, Iv &out) {
  out={};
  if(!manufacturer_le || !meter_address || !ell || ell_length<7)
    return Status::InvalidInput;
  if(ell[0]!=0x8D) return Status::UnsupportedCi;
  // ELL II: CI, CC, ACC, 4-byte session number; payload CRC follows.
  const uint8_t security=static_cast<uint8_t>((ell[6]>>5)&0x07u);
  if(security!=1) return Status::NotAesCtr;
  Iv v{};
  v.bytes[0]=manufacturer_le[0];v.bytes[1]=manufacturer_le[1];
  for(size_t i=0;i<6;++i) v.bytes[2+i]=meter_address[i];
  // Repeated/hop flags must not alter the cryptographic IV.
  v.bytes[8]=static_cast<uint8_t>(ell[1]&static_cast<uint8_t>(~0x12u));
  for(size_t i=0;i<4;++i) v.bytes[9+i]=ell[3+i];
  // FN[2], BC[1] initialized to zero. Counter progression and
  // payload verification are intentionally NOT implemented here.
  out=v;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::ell_ctr
