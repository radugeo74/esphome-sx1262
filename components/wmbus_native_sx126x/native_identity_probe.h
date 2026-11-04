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
  Address ell{};
  bool has_tpl{false};
  bool has_ell{false};
  bool has_ell_session{false};
  uint8_t ell_security_mode{0xFF};
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
  } else if (result.ci==0x8C || result.ci==0x8D ||
             result.ci==0x8E || result.ci==0x8F) {
    // Same fixed ELL I-IV layouts as original wmbus.cc parseELL:
    // CI+CC+ACC, optionally M(2)+ID(4)+V+T, then SN(4)+PL_CRC(2).
    // Only inspect identities and session security bits. ELL payload CRC
    // and downstream TPL have NOT been verified at this stage.
    const bool address=(result.ci==0x8E || result.ci==0x8F);
    const bool session=(result.ci==0x8D || result.ci==0x8F);
    const size_t required=13+(address?8:0)+(session?6:0);
    if (size < required) {result.status=Status::TooShort;return result;}
    size_t offset=13; // Skip CI, CC and ACC.
    if (address) {
      copy_address(result.ell,frame+offset,true);
      if (!valid_manufacturer(result.ell.manufacturer)) {
        result.status=Status::InvalidTplManufacturer;return result;
      }
      result.has_ell=true;
      offset+=8;
    }
    if (session) {
      const uint32_t sn=static_cast<uint32_t>(frame[offset]) |
          (static_cast<uint32_t>(frame[offset+1])<<8) |
          (static_cast<uint32_t>(frame[offset+2])<<16) |
          (static_cast<uint32_t>(frame[offset+3])<<24);
      result.has_ell_session=true;
      result.ell_security_mode=static_cast<uint8_t>(sn>>29);
      offset+=6; // Session number + payload CRC.
    }
    result.application_offset=offset;
  } else if (result.ci==0xA0 || result.ci==0xA2) {
    // Manufacturer-specific plaintext protocols: only DLL is known.
  } else {
    // ELL V (0x86), other variable layouts and unknown transports unsupported.
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
  bool ell_matched{false};
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
    out.ell_matched=out.ell_matched ||
                    (result.has_ell && same_id(result.ell,text));
  }
  if (dll && tpl) {
    out.source=!same_id_bytes(result.dll,result.tpl) ? ConfiguredSource::Conflict :
                                        ConfiguredSource::Both;
  } else if (dll) out.source=ConfiguredSource::Dll;
  else if (tpl) out.source=ConfiguredSource::Tpl;
  return out;
}
// Identification policy is advisory only. The closest available target
// address is preferred over a forwarding converter's DLL address.
// ELL payload CRC, TPL security, and real driver binding remain separate.
enum class CandidateSource : uint8_t {
  None, Dll, Tpl, Ell, Conflict, Unsupported
};
struct CandidateDecision {
  CandidateSource candidate{CandidateSource::Unsupported};
  bool configured_candidate{false};
  bool multiple_configured_ids{false};
  bool needs_payload_validation{true};
  bool permits_publication{false}; // Never authorize from metadata alone.
};
template<typename IdCollection>
inline CandidateDecision identify_candidate(
    const Result &result, const IdCollection &configured_ids) {
  CandidateDecision decision{};
  if (result.status!=Status::Ok) return decision;
  decision.candidate=result.has_tpl ? CandidateSource::Tpl :
      result.has_ell ? CandidateSource::Ell : CandidateSource::Dll;
  const Address &chosen=result.has_tpl ? result.tpl :
      result.has_ell ? result.ell : result.dll;
  bool dll_match=false, tpl_match=false, ell_match=false;
  for (const auto &id : configured_ids) {
    const char *configured=id.c_str();
    dll_match=dll_match || same_id(result.dll,configured);
    tpl_match=tpl_match ||
        (result.has_tpl && same_id(result.tpl,configured));
    ell_match=ell_match ||
        (result.has_ell && same_id(result.ell,configured));
    decision.configured_candidate=decision.configured_candidate ||
                                  same_id(chosen,configured);
  }
  // A single configured ID can legitimately occur in multiple headers.
  // Only distinct configured addresses can create conflicting bindings.
  const bool dll_tpl_conflict=dll_match && tpl_match &&
      !same_id_bytes(result.dll,result.tpl);
  const bool dll_ell_conflict=dll_match && ell_match &&
      !same_id_bytes(result.dll,result.ell);
  const bool tpl_ell_conflict=tpl_match && ell_match &&
      !same_id_bytes(result.tpl,result.ell);
  decision.multiple_configured_ids=dll_tpl_conflict ||
      dll_ell_conflict || tpl_ell_conflict;
  if (decision.multiple_configured_ids) {
    decision.candidate=CandidateSource::Conflict;
    decision.configured_candidate=false;
  }
  return decision;
}
}} // wmbus_pure::identity
