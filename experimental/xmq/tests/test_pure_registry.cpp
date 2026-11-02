#include "pure_driver_registry.h"
#include "generated_driver_registry.h"
#include <cassert>
#include <cstring>
using namespace wmbus_pure;
int main() {
  using namespace wmbus_pure::generated;
  auto heat = select_driver(signatures, signature_count, 0x5068, 0x69, 0x80);
  assert(heat.status == DriverSelectionStatus::Found);
  assert(std::strcmp(heat.driver, "fhkvdataiii") == 0);
  auto water = select_driver(signatures, signature_count, 0x5068, 0x95, 0x62);
  assert(water.status == DriverSelectionStatus::Found);
  assert(std::strcmp(water.driver, "mkradio4") == 0);
  assert(select_driver(signatures, signature_count, 0x5068, 0xFF, 0x62).status ==
         DriverSelectionStatus::Unknown);
  const DriverSignature ambiguous[] = {
      {0x5068, 0x69, 0x80, "driver_a"},
      {0x5068, 0x69, 0x80, "driver_b"}
  };
  assert(select_driver(ambiguous, 2, 0x5068, 0x69, 0x80).status ==
         DriverSelectionStatus::Ambiguous);
  assert(select_driver(nullptr, 1, 0x5068, 0x69, 0x80).status ==
         DriverSelectionStatus::InvalidArguments);
  // Duplicate identical entries must not trigger false ambiguity.
  const DriverSignature duplicates[] = {
      {0x5068, 0x69, 0x80, "driver_a"},
      {0x5068, 0x69, 0x80, "driver_a"}
  };
  assert(select_driver(duplicates, 2, 0x5068, 0x69, 0x80).status ==
         DriverSelectionStatus::Found);
}
