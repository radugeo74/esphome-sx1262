#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_auto_dispatch.h"
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
  auto selected = auto_driver::resolve("auto", signatures, signature_count, 0x5068, 0x95, 0x62);
  assert(selected.status == auto_driver::Status::Ready);
  assert(std::strcmp(selected.driver, "mkradio4") == 0);
  auto override_driver = auto_driver::resolve("fhkvdataiii", signatures, signature_count, 0, 0, 0);
  assert(override_driver.status == auto_driver::Status::Ready);
  assert(std::strcmp(override_driver.driver, "fhkvdataiii") == 0);
  auto not_compiled = auto_driver::resolve("auto", signatures, signature_count, 0x5068, 0x95, 0x37);
  assert(not_compiled.status == auto_driver::Status::NotCompiled);
  assert(not_compiled.driver == nullptr);
  // New upstream XMQ definitions: identification is available even though
  // their payload decoders have not yet been ported into the native firmware.
  const auto iperl = select_driver(signatures, signature_count, 0x4CAE, 0x68, 0x07);
  assert(iperl.status == DriverSelectionStatus::Found);
  assert(std::strcmp(iperl.driver, "iperl") == 0);
  const auto hydrus = select_driver(signatures, signature_count, 0x11A5, 0x70, 0x07);
  assert(hydrus.status == DriverSelectionStatus::Found);
  assert(std::strcmp(hydrus.driver, "hydrus") == 0);
  const auto pending = auto_driver::resolve("auto", signatures, signature_count, 0x4CAE, 0x68, 0x07);
  assert(pending.status == auto_driver::Status::NotCompiled && pending.driver == nullptr);
  // A different TPL MVT must not silently be assigned to a DLL candidate.
  auto unknown = select_driver(signatures, signature_count, 0x11A4, 0xFE, 0x07);
  assert(unknown.status == DriverSelectionStatus::Unknown);
  auto missing = auto_driver::resolve("auto", signatures, signature_count, 0x11A4, 0xFE, 0x07);
  assert(missing.status == auto_driver::Status::Unknown && missing.driver == nullptr);
  // Ambiguous matches must never select arbitrary drivers.
  constexpr DriverSignature collision[] = {
      {0x5068, 0x95, 0x37, "mkradio4a"},
      {0x5068, 0x95, 0x37, "other"}
  };
  auto conflict = select_driver(collision, 2, 0x5068, 0x95, 0x37);
  assert(conflict.status == DriverSelectionStatus::Ambiguous);
  assert(conflict.driver == nullptr);
  auto ambiguous = auto_driver::resolve("auto", collision, 2, 0x5068, 0x95, 0x37);
  assert(ambiguous.status == auto_driver::Status::Ambiguous && ambiguous.driver == nullptr);
  // Duplicate signature with same driver is not ambiguous.
  constexpr DriverSignature duplicate[] = {
      {0x5068, 0x95, 0x37, "mkradio4a"},
      {0x5068, 0x95, 0x37, "mkradio4a"}
  };
  auto same = select_driver(duplicate, 2, 0x5068, 0x95, 0x37);
  assert(same.status == DriverSelectionStatus::Found);
  return 0;
}
