#pragma once
// Read-only DLL/TPL identity inspection for a CRC-verified, CRC-stripped
// EN13757 frame. This does not select, decrypt or publish meter readings.
// The existing strict numeric DLL gate remains unchanged.
#include <cstddef>
#include <cstdint>
namespace wmbus_pure { namespace identity {
enum class Status : uint8_t { Ok, TooShort, InvalidLength, InvalidManufacturer,
                              UnsupportedCi, InvalidTplManufacturer };
struct Address {
  uint16_t manufacturer{0};
  uint8_t id_le[4]{};
  uint8_t version{0};
  uint8_t device_type{0};
  bool decimal_bcd{false};
};
struct Result {
  Status status{Status::TooShort};
  Address dll{};
  Address tpl{};
  bool has_tpl{false};
  uint8_t ci{0};
  size_t application_offset{0};
};
inline bool valid_manufacturer(uint16_t m) {
  return (m & 31u) >= 1 && (m & 31u) <= 26 &&
         ((m >> 5) & 31u) >= 1 && ((m >> 5) & 31u) <= 26 &&
         ((m >> 10) & 31u) >= 1 && ((m >> 10) & 31u) <= 26;
}
inline bool is_decimal_bcd(const uint8_t *id) {
  for (unsigned i=0; i<4; ++i)
    if ((id[i] & 15u)>9 || (id[i] >> 4)>9) return false;
  return true;
}
// The old wmbusmeters Address::decodeIdFirst preserves raw hexadecimal
// bytes, including non-BCD identifiers. We follow that read-only behavior.
inline void copy_address(Address &a,const uint8_t *data,bool mfct_first) {
  const size_t start=mfct_first ? 2 : 0;
  for (unsigned i=0;i<4;++i) a.id_le[i]=data[start+i];
  const size_t m=mfct_first ? 0 : 4;
  a.manufacturer=static_cast<uint16_t>(data[m]) |
                 (static_cast<uint16_t>(data[m+1])<<8);
  a.version=data[6]; a.device_type=data[7];
  a.decimal_bcd=is_decimal_bcd(a.id_le);
}
inline Result inspect(const uint8_t *frame,size_t size) {
  Result result{};
  if (!frame || size < 11) return result;
  if (size != static_cast<size_t>(frame[0])+1) {
    result.status=Status::InvalidLength;
    return result;
  }
  copy_address(result.dll,frame+2,true);
  if (!valid_manufacturer(result.dll.manufacturer)) {
    result.status=Status::InvalidManufacturer;
    return result;
  }
  result.ci=frame[10];
  result.application_offset=11;
  if (result.ci==0x72) {
    // TPL long header: CI, ID(4), M(2), version, medium, access,
    // status, config(2). The TPL ID may differ from the DLL converter.
    if (size < 23) {result.status=Status::TooShort;return result;}
    copy_address(result.tpl,frame+11,false);
    if (!valid_manufacturer(result.tpl.manufacturer)) {
      result.status=Status::InvalidTplManufacturer;return result;
    }
    result.has_tpl=true;
    result.application_offset=23;
  } else if (result.ci==0x7A) {
    // TPL short header carries no second meter identity.
    if (size < 15) {result.status=Status::TooShort;return result;}
    result.application_offset=15;
  } else if (result.ci==0xA0 || result.ci==0xA2) {
    // Manufacturer-specific plaintext protocols: only DLL is known.
  } else {
    // ELL and other transports require separate framing and validation.
    result.status=Status::UnsupportedCi;
    return result;
  }
  result.status=Status::Ok;
  return result;
}
inline bool same_id(const Address &a,const char *configured) {
  if (!configured) return false;
  for (unsigned i=0;i<8;++i) {
    const char c=configured[i];
    if (c=='\0') return false;
    const unsigned nib=(i%2==0) ? (a.id_le[3-i/2] >> 4) :
                                  (a.id_le[3-i/2] & 15u);
    if (nib<10) {if (c != char('0'+nib)) return false;}
    else if (c != char('A'+nib-10) && c != char('a'+nib-10))
      return false;
  }
  return configured[8]=='\0';
}
enum class Source : uint8_t { None, Dll, Tpl, Both, Ambiguous };
inline Source match(const Result &result, const char *configured) {
  if (result.status!=Status::Ok) return Source::None;
  const bool dll=same_id(result.dll,configured);
  const bool tpl=result.has_tpl && same_id(result.tpl,configured);
  if (dll && tpl) return Source::Both;
  if (dll && !tpl) return Source::Dll;
  if (tpl && !dll) return Source::Tpl;
  return Source::None;
}
// Pure, read-only configured-ID diagnostics. Never grant decode permission.
// Multiple configured identities in one telegram represent an ambiguity.
enum class ConfiguredSource : uint8_t {
  None, Dll, Tpl, Both, Conflict, Unsupported
};
struct ConfiguredResult {
  ConfiguredSource source{ConfiguredSource::Unsupported};
  bool distinct_addresses{false};
};
inline bool same_id_bytes(const Address &a,const Address &b) {
  for (unsigned i=0;i<4;++i)
    if (a.id_le[i]!=b.id_le[i]) return false;
  return true;
}
inline bool same_address(const Address &a,const Address &b) {
  return same_id_bytes(a,b) && a.manufacturer==b.manufacturer &&
         a.version==b.version && a.device_type==b.device_type;
}
template<typename IdCollection>
inline ConfiguredResult diagnose(const Result &result,
                                const IdCollection &configured_ids) {
  ConfiguredResult out{};
  if (result.status!=Status::Ok) return out;
  out.source=ConfiguredSource::None;
  out.distinct_addresses=result.has_tpl &&
                         !same_address(result.dll,result.tpl);
  bool dll=false, tpl=false;
  for (const auto &id : configured_ids) {
    const char *text=id.c_str();
    dll=dll || same_id(result.dll,text);
    tpl=tpl || (result.has_tpl && same_id(result.tpl,text));
  }
  if (dll && tpl) {
    out.source=!same_id_bytes(result.dll,result.tpl) ? ConfiguredSource::Conflict :
                                        ConfiguredSource::Both;
  } else if (dll) out.source=ConfiguredSource::Dll;
  else if (tpl) out.source=ConfiguredSource::Tpl;
  return out;
}
}} // wmbus_pure::identity
