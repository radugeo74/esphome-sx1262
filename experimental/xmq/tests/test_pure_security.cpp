#include "../../../components/wmbus_radio/pure_security.h"
#include <cassert>
using namespace wmbus_pure;
int main() {
  TransportEnvelope t{};
  t.kind=TransportKind::Short;
  t.configuration=0x0000;
  auto s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::NoSecurity && s.mode==0);
  t.configuration=0x05A0;
  s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::AesCbcIvMode5 && s.mode==5 && s.encrypted_blocks==10);
  assert(!s.extended_configuration_required);
  t.kind=TransportKind::Long;t.configuration=0x0750;
  s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::AesCbcNoIvMode7 && s.encrypted_blocks==5);
  assert(s.extended_configuration_required);
  t.configuration=0x0F00;
  s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::Unsupported);
  t.kind=TransportKind::ManufacturerSpecific;
  s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::NotApplicable);
  t.kind=TransportKind::Unsupported;
  s=inspect_tpl_security(t);
  assert(s.kind==SecurityKind::NotApplicable);
}
