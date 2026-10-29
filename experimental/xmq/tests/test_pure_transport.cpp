#include "../../../components/wmbus_radio/pure_transport.h"
#include <cassert>
using namespace wmbus_pure;
int main() {
  Header h{};
  TransportEnvelope out{};
  const uint8_t sh[]{0x12,0x34,0xAB,0xCD,0x99};
  h.ci=0x7A; h.application=sh; h.application_length=sizeof(sh);
  assert(inspect_transport(h,out)==TransportStatus::Ok);
  assert(out.kind==TransportKind::Short && out.access_number==0x12);
  assert(out.status==0x34 && out.configuration==0xCDAB);
  assert(out.payload_size==1 && out.payload[0]==0x99);
  h.application_length=3;
  assert(inspect_transport(h,out)==TransportStatus::Truncated);
  assert(out.payload==nullptr);
  const uint8_t lg[]{1,2,3,4,5,6,7,8,0x11,0x22,0x44,0x33,0x55};
  h.ci=0x72;h.application=lg;h.application_length=sizeof(lg);
  assert(inspect_transport(h,out)==TransportStatus::Ok);
  assert(out.kind==TransportKind::Long && out.access_number==0x11);
  assert(out.status==0x22 && out.configuration==0x3344);
  assert(out.payload_size==1 && out.payload[0]==0x55);
  h.application_length=11;
  assert(inspect_transport(h,out)==TransportStatus::Truncated);
  const uint8_t specific[]{0xDE,0xAD};
  h.ci=0xA0;h.application=specific;h.application_length=2;
  assert(inspect_transport(h,out)==TransportStatus::Ok);
  assert(out.kind==TransportKind::ManufacturerSpecific && out.payload_size==2);
  assert(out.payload[0]==0xDE && out.configuration==0);
  h.ci=0xFF;
  assert(inspect_transport(h,out)==TransportStatus::UnsupportedCi);
  h.ci=0x7A;h.application=nullptr;h.application_length=5;
  assert(inspect_transport(h,out)==TransportStatus::Truncated);
}
