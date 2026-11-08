#pragma once
// Shared configured secure-meter decode -> mapped-output delivery gate.
// The firmware calls evaluate() ONLY with a fully DLL/CRC-verified,
// CRC-stripped packet; host integration tests must verify the same ingress.
// Recognition-only AUTO never reaches this keyed path, and a matching
// secure binding consumes even an unsuccessful frame (no clear fallback).
// No ESPhome, radio, synthetic meter secrets, or persistence dependencies.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "secure_meter_binding.h"
#include "xmq_batch2_secure_dispatch.h"

namespace esphome { namespace wmbus_native_sx126x { namespace secure_delivery {

enum class Outcome : uint8_t { Unmatched, Rejected, Ready };

struct Result {
  Outcome outcome{Outcome::Unmatched};
  const secure_bindings::Entry *binding{nullptr};
  xmq_portable::batch2_secure::Status decode_status{
      xmq_portable::batch2_secure::Status::EncryptedDecodeRejected};
  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  size_t value_count{0};
  size_t date_count{0};
  const char *authenticated_status{nullptr};
};

// 'sender_id' is the independently verified DLL meter identity. In the
// MK Radio4a/Minomess converter case it is NOT the target TPL meter ID.
// The existing driver independently validates both identities and AES.
inline Outcome evaluate(const uint8_t *frame, size_t length,
                        const std::string &sender_id,
                        const std::vector<secure_bindings::Entry> &bindings,
                        Result &out) {
  out=Result{};
  if (!frame || length<11 || bindings.empty()) return out.outcome;
  for (const auto &entry : bindings) {
    if (entry.dll_id != sender_id) continue;
    out.binding=&entry;
    out.outcome=Outcome::Rejected; // matched key always consumes the frame
    xmq_portable::DecodedPayload decoded{
        out.values,24,0,out.dates,2,0};
    out.decode_status=xmq_portable::batch2_secure::decode_configured(
        frame,length,entry.id.c_str(),entry.driver.c_str(),
        entry.key.data(),decoded,&out.authenticated_status,
        entry.dll_id.c_str());
    if (out.decode_status!=xmq_portable::batch2_secure::Status::Ok ||
        decoded.value_count==0 || decoded.value_count>24 ||
        decoded.date_count>2) {
      // Never expose partial stale or unauthenticated fields to a sensor.
      const auto failed=out.decode_status;
      out=Result{};
      out.binding=&entry;
      out.outcome=Outcome::Rejected;
      out.decode_status=failed;
      return out.outcome;
    }
    out.value_count=decoded.value_count;
    out.date_count=decoded.date_count;
    out.outcome=Outcome::Ready;
    return out.outcome;
  }
  return out.outcome;
}

// These callbacks are the same mapped field/value boundaries used by HA:
// numeric(field,value), text(field,value), rssi(value). A caller binds them
// ONLY to configured sensor IDs. The helper never creates a sensor and it
// never publishes a failed or an unidentified packet.
template<typename NumericSink,typename TextSink,typename RssiSink>
inline void emit_mapped(const Result &result,float rssi,
                        NumericSink numeric,TextSink text,RssiSink rssi_sink) {
  if(result.outcome!=Outcome::Ready || !result.binding) return;
  for(size_t i=0;i<result.value_count;++i) {
    const auto &value=result.values[i];
    if(value.name && std::isfinite(value.value))
      numeric(value.name,value.value);
  }
  if(result.binding->driver=="mkradio4a" &&
     result.date_count==1) {
    const auto &date=result.dates[0];
    if(date.year>=1 && date.year<=9999 &&
       date.month>=1 && date.month<=12 &&
       date.day>=1 &&
       date.day<=xmq_portable::days_in_month(date.year,date.month)) {
      char formatted[16]{};
      std::snprintf(formatted,sizeof(formatted),"%04d-%02d-%02d",
                    date.year,date.month,date.day);
      text("target_date",formatted);
    }
  }
  if(result.authenticated_status) text("status",result.authenticated_status);
  rssi_sink(rssi);
}

}}}  // namespace esphome::wmbus_native_sx126x::secure_delivery
