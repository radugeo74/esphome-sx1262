#pragma once
// Portable, narrow already-clear TPL 0x7A status combination for the two public
// kamwater.xmq status-propagation fixtures. Input is already-clear TPL.
// No DLL/ELL verification, radio dispatch, or sensor publication.
// The official #13/#14 status byte 02 means generic TPL ERROR; other
// status bytes/configurations are deliberately unsupported here.
#include <cstddef>
#include <cstdint>
#include <string>
#include "kamwater_status_semantics.h"
namespace xmq_portable { namespace kamwater { namespace tpl_status {
enum class Status : uint8_t {
  Ok, Invalid, UnsupportedTpl, UnsupportedConfiguration,
  UnsupportedTplStatus, InvalidApplication
};
struct Reading {
  std::string status;
  bool has_durations{false};
  status_semantics::Reading durations{};
};
inline Status decode_clear_short_tpl(const uint8_t *tpl,size_t size,
                                     Reading &out) {
  out={};
  if(!tpl || size<6) return Status::Invalid;
  if(tpl[0]!=0x7A) return Status::UnsupportedTpl;
  // Official 0x7A unencrypted short header: CI, ACC, STATUS, CFG(2 LE).
  if(tpl[3]!=0 || tpl[4]!=0) return Status::UnsupportedConfiguration;
  // Only the exact published 'generic error' status is currently proven.
  if(tpl[2]!=0x02) return Status::UnsupportedTplStatus;
  status_semantics::Reading app{};
  const auto parsed=status_semantics::decode(tpl+5,size-5,app);
  if(parsed==status_semantics::Status::Ambiguous ||
     parsed==status_semantics::Status::Invalid)
    return Status::InvalidApplication;
  Reading candidate{};
  if(parsed==status_semantics::Status::Ok) {
    candidate.has_durations=true;
    candidate.durations=app;
    candidate.status=app.status=="OK"?"ERROR":app.status+" ERROR";
  } else {
    // Upstream #13 carries no application status, only TPL ERROR.
    candidate.status="ERROR";
  }
  out=candidate;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::tpl_status
