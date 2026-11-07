#pragma once
// Read-only recognition of the three manually selectable batch-2 drivers.
// ONLY call with independently verified DLL/CRC-stripped T1/C1 frames.
// Checks original XMQ detect.mvt metadata, including a long-TPL meter
// behind a different DLL converter. Never reads AES keys, attempts a
// decode, registers a meter or approves Home Assistant publication.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "native_identity_probe.h"
#include "pure_driver_registry.h"

namespace wmbus_pure { namespace batch2_identify {

enum class State : uint8_t { Unknown, Candidate, Ambiguous, Invalid };
enum class Source : uint8_t { None, Dll, Tpl, Ell };
struct Result {
  State state{State::Invalid};
  Source source{Source::None};
  const char *driver{nullptr};
  uint32_t meter_id{0};  // target from the selected header, if decimal BCD
  bool decimal_bcd{false};
  bool different_dll_id{false};
};

inline bool is_batch2(const char *name) {
  return name && (std::strcmp(name,"minomess")==0 ||
                  std::strcmp(name,"kamwater")==0 ||
                  std::strcmp(name,"apator162")==0);
}

inline uint32_t decimal_id(const identity::Address &address) {
  uint32_t id=0;
  for(int i=3;i>=0;--i) {
    id=id*100u+static_cast<uint32_t>((address.id_le[i]>>4)*10u+
                                      (address.id_le[i]&15u));
  }
  return id;
}

inline DriverSelection select(const DriverSignature *signatures,size_t count,
                              const identity::Address &address) {
  // This uses the FULL XMQ registry: collisions with other driver names
  // are ambiguous rather than silently promoted to batch-2.
  return select_driver(signatures,count,address.manufacturer,
                       address.version,address.device_type);
}

inline bool has_batch2_signature(const DriverSignature *signatures,
                                 size_t count,
                                 const identity::Address &address) {
  for(size_t i=0;i<count;++i) {
    const auto &s=signatures[i];
    if(s.manufacturer==address.manufacturer &&
       s.version==address.version &&
       s.device_type==address.device_type &&
       is_batch2(s.driver))
      return true;
  }
  return false;
}

inline Result inspect(const uint8_t *frame,size_t length,
                      const DriverSignature *signatures,size_t count) {
  Result result{};
  if (!frame || !signatures || count==0 || length<11) return result;
  const auto parsed=identity::inspect(frame,length);
  if(parsed.status!=identity::Status::Ok) return result;
  const auto dll=select(signatures,count,parsed.dll);
  const auto tpl=parsed.has_tpl
      ? select(signatures,count,parsed.tpl)
      : DriverSelection{DriverSelectionStatus::Unknown,nullptr,0};
  const auto ell=parsed.has_ell
      ? select(signatures,count,parsed.ell)
      : DriverSelection{DriverSelectionStatus::Unknown,nullptr,0};

  // Ambiguous MVT must never be described as a definitive model.
  if((dll.status==DriverSelectionStatus::Ambiguous &&
      has_batch2_signature(signatures,count,parsed.dll)) ||
     (tpl.status==DriverSelectionStatus::Ambiguous &&
      has_batch2_signature(signatures,count,parsed.tpl)) ||
     (ell.status==DriverSelectionStatus::Ambiguous &&
      has_batch2_signature(signatures,count,parsed.ell))) {
    result.state=State::Ambiguous;
    return result;
  }
  // Unrelated converter-DLL MVT collisions must not hide an
  // unambiguous Minomess identity in the actual long TPL.

  const bool dll_batch=dll.status==DriverSelectionStatus::Found &&
                       is_batch2(dll.driver);
  const bool tpl_batch=tpl.status==DriverSelectionStatus::Found &&
                       is_batch2(tpl.driver);
  const bool ell_batch=ell.status==DriverSelectionStatus::Found &&
                       is_batch2(ell.driver);
  if((tpl_batch && dll_batch && std::strcmp(tpl.driver,dll.driver)!=0) ||
     (ell_batch && dll_batch && std::strcmp(ell.driver,dll.driver)!=0) ||
     (tpl_batch && ell_batch && std::strcmp(tpl.driver,ell.driver)!=0)) {
    result.state=State::Ambiguous;
    return result;
  }

  const identity::Address *chosen=nullptr;
  if(tpl_batch) {
    chosen=&parsed.tpl;
    result.source=Source::Tpl;
    result.driver=tpl.driver;
  } else if(ell_batch) {
    chosen=&parsed.ell;
    result.source=Source::Ell;
    result.driver=ell.driver;
  } else if(dll_batch) {
    chosen=&parsed.dll;
    result.source=Source::Dll;
    result.driver=dll.driver;
  }
  if(!chosen) {
    result.state=State::Unknown;
    return result;
  }
  result.state=State::Candidate;
  result.decimal_bcd=chosen->decimal_bcd;
  if(chosen->decimal_bcd) result.meter_id=decimal_id(*chosen);
  result.different_dll_id=!identity::same_id_bytes(parsed.dll,*chosen);
  return result;
}
}} // namespace wmbus_pure::batch2_identify
