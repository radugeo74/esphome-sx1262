#include <cassert>
#include <cmath>
#include <cstdio>
#include "../../../components/wmbus_meter/xmq_field_parity.h"
int main() {
  using namespace xmq_portable;
  size_t count=0;
  const ParityField *heat=parity_fields("fhkvdataiii",count);
  assert(heat && count==4);
  const ParityField *water=parity_fields("mkradio4",count);
  assert(water && count==2);
  assert(parity_fields("unknown",count)==nullptr && count==0);
  CalcValue values[2]{{"current_hca", 12.0}, {"temp_room_c", 21.95}};
  DecodedPayload decoded{values,2,2,nullptr,0,0};
  double xmq=0;
  assert(compare_numeric_field(decoded, heat[0], 12., true, xmq)==FieldParity::Equal);
  assert(compare_numeric_field(decoded, heat[0], 13., true, xmq)==FieldParity::Different);
  assert(compare_numeric_field(decoded, heat[1], 0., true, xmq)==FieldParity::MissingXmq);
  assert(compare_numeric_field(decoded, heat[0], 0., false, xmq)==FieldParity::MissingLegacy);
  assert(compare_numeric_field(decoded, heat[0], NAN, true, xmq)==FieldParity::MissingLegacy);
  assert(compare_numeric_field(decoded, heat[2], 21.9500001, true, xmq)==FieldParity::Equal);
  assert(compare_numeric_field(decoded, heat[2], 21.9, true, xmq)==FieldParity::Different);
  std::puts("XMQ numeric field parity PASS");
}
