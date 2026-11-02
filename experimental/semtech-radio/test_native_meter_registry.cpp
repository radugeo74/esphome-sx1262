#include "../../components/wmbus_native_sx126x/native_meter_registry.h"
#include <cassert>
#include <string>
#include <vector>
using namespace esphome::wmbus_native_sx126x;
int main() {
  const std::vector<std::string> ids={"31000001","00000001"};
  verified::Result frame{};
  frame.status=verified::Status::BadBoundary;
  frame.frame.header.meter_id=31000001;
  assert(registry::classify(frame,ids)==registry::Kind::Invalid);
  frame.status=verified::Status::BadCrcOrHeader;
  assert(registry::classify(frame,ids)==registry::Kind::Invalid);
  frame.status=verified::Status::Ok;
  assert(registry::classify(frame,ids)==registry::Kind::Configured);
  frame.frame.header.meter_id=31000002;
  assert(registry::classify(frame,ids)==registry::Kind::Discovered);
  assert(registry::classify(frame,{"31ABC001"})==registry::Kind::Discovered);
  assert(registry::classify(frame,{})==registry::Kind::Discovered);
  return 0;
}
