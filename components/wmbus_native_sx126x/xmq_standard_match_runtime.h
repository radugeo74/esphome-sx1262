#pragma once
// Shared, source-generated XMQ standard DIF/VIF field matcher.
// The XMQ generator supplies selectors and output names; this runtime
// only converts independently validated standard record values.
// Not a security gate, RF receiver, AES decoder or dynamic XMQ interpreter.
#include <cmath>
#include "standard_dif_vif.h"
#include "xmq_generated_standard_selectors.h"

namespace xmq_portable { namespace xmq_standard {
inline bool scaled(const xmq_generated_standard::Rule &selector,
                   const standard::Record &r,double &value) {
  if(!r.numeric || r.function || r.storage || r.tariff || r.subunit ||
     r.modifier_count || r.full_vif<selector.vif_first ||
     r.full_vif>selector.vif_last) return false;
  if(selector.kind==xmq_generated_standard::Kind::RelativeHumidity) {
    if(r.vif_extension_count!=1) return false;
    value=r.value/10.0;
    return std::isfinite(value);
  }
  if(r.vif_extension_count!=0) return false;
  const auto kind=selector.kind;
  if(kind!=xmq_generated_standard::Kind::Volume &&
     kind!=xmq_generated_standard::Kind::VolumeFlow &&
     kind!=xmq_generated_standard::Kind::TemperatureC) return false;
  return standard::to_canonical(r.vif,r.value,value);
}
}} // namespace xmq_portable::xmq_standard
