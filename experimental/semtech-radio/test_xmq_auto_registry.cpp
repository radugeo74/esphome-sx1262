#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_auto_resolver.h"
#include <cassert>
#include <cstring>
int main() {
  using namespace wmbus_pure;
  using namespace wmbus_pure::generated;
  // Public upstream detect.mvt fixtures, not real meter IDs.
  auto water = select_driver(signatures, signature_count, 0x5068, 0x95, 0x62);
  assert(water.status == DriverSelectionStatus::Found);
  assert(std::strcmp(water.driver, "mkradio4") == 0);
  auto converter = select_driver(signatures, signature_count, 0x5068, 0x95, 0x37);
  assert(converter.status == DriverSelectionStatus::Found);
  assert(std::strcmp(converter.driver, "mkradio4a") == 0);
  // Explicit bindings continue to work independently of header signatures.
  auto explicit_water = xmq_auto::resolve("mkradio4", signatures, signature_count, 0, 0, 0);
  assert(explicit_water.status == xmq_auto::Status::Selected);
  assert(std::strcmp(explicit_water.driver, "mkradio4") == 0);
  // The public Techem water signature may resolve automatically.
  auto auto_water = xmq_auto::resolve("auto", signatures, signature_count, 0x5068, 0x95, 0x62);
  assert(auto_water.status == xmq_auto::Status::Selected);
  assert(std::strcmp(auto_water.driver, "mkradio4") == 0);
  // A known signature without a compiled decoder must fail closed.
  auto auto_converter = xmq_auto::resolve("auto", signatures, signature_count, 0x5068, 0x95, 0x37);
  assert(auto_converter.status == xmq_auto::Status::Unsupported);
  assert(auto_converter.driver == nullptr);
  // A different TPL MVT must not silently be assigned to a DLL candidate.
  auto unknown = select_driver(signatures, signature_count, 0x11A4, 0xFE, 0x07);
  assert(unknown.status == DriverSelectionStatus::Unknown);
  // Ambiguous matches must never select arbitrary drivers.
  constexpr DriverSignature collision[] = {
      {0x5068, 0x95, 0x37, "mkradio4a"},
      {0x5068, 0x95, 0x37, "other"}
  };
  auto conflict = select_driver(collision, 2, 0x5068, 0x95, 0x37);
  assert(conflict.status == DriverSelectionStatus::Ambiguous);
  assert(conflict.driver == nullptr);
  auto ambiguous = xmq_auto::resolve("auto", collision, 2, 0x5068, 0x95, 0x37);
  assert(ambiguous.status == xmq_auto::Status::Ambiguous);
  assert(ambiguous.driver == nullptr);
  // Duplicate signature with same driver is not ambiguous.
  constexpr DriverSignature duplicate[] = {
      {0x5068, 0x95, 0x37, "mkradio4a"},
      {0x5068, 0x95, 0x37, "mkradio4a"}
  };
  auto same = select_driver(duplicate, 2, 0x5068, 0x95, 0x37);
  assert(same.status == DriverSelectionStatus::Found);
  return 0;
}
