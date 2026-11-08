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
  // Passive identification and live plaintext decode eligibility are
  // independent. Secure-only drivers are recognized WITHOUT any AES key,
  // but the existing live resolver must never authorize AUTO decoding.
  const auto mk_meta=auto_driver::identify(signatures,signature_count,
                                           0x5068,0x95,0x37);
  const auto topas_meta=auto_driver::identify(signatures,signature_count,
                                              0x05B4,0xF1,0x06);
  assert(mk_meta.status==auto_driver::RecognitionStatus::Candidate &&
         std::strcmp(mk_meta.driver,"mkradio4a")==0 &&
         !mk_meta.compiled_plaintext);
  assert(topas_meta.status==auto_driver::RecognitionStatus::Candidate &&
         std::strcmp(topas_meta.driver,"topaseskr")==0 &&
         !topas_meta.compiled_plaintext);
  const auto topas_v7=auto_driver::identify(signatures,signature_count,
                                            0x05B4,0xF1,0x07);
  assert(topas_v7.status==auto_driver::RecognitionStatus::Candidate &&
         std::strcmp(topas_v7.driver,"topaseskr")==0 &&
         !topas_v7.compiled_plaintext);
  const auto ready_meta=auto_driver::identify(signatures,signature_count,
                                              0x5068,0x95,0x62);
  assert(ready_meta.status==auto_driver::RecognitionStatus::Candidate &&
         ready_meta.compiled_plaintext);
  assert(auto_driver::identify(signatures,signature_count,
                               0x1234,0xFF,0xFF).status==
         auto_driver::RecognitionStatus::Unknown);
  const auto topas_auto_decode=auto_driver::resolve("auto",signatures,
                                 signature_count,0x05B4,0xF1,0x06);
  assert(topas_auto_decode.status==auto_driver::Status::NotCompiled &&
         topas_auto_decode.driver==nullptr);
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
  const auto iperl_ready = auto_driver::resolve("auto", signatures, signature_count, 0x4CAE, 0x68, 0x07);
  assert(iperl_ready.status == auto_driver::Status::Ready);
  assert(std::strcmp(iperl_ready.driver, "iperl") == 0);
  const auto hydrus_ready = auto_driver::resolve("auto", signatures, signature_count, 0x11A5, 0x70, 0x07);
  assert(hydrus_ready.status == auto_driver::Status::Ready);
  assert(std::strcmp(hydrus_ready.driver, "hydrus") == 0);
  // Lansen: three original LAS MVTs may AUTO-select on configured meters.
  constexpr uint8_t lansen_v[] = {0x14,0x1B,0x0B};
  constexpr uint8_t lansen_t[] = {0x00,0x00,0x02};
  for(unsigned i=0;i<3;++i) {
    const auto selected_lansen=auto_driver::resolve(
        "auto",signatures,signature_count,0x3033,lansen_v[i],lansen_t[i]);
    assert(selected_lansen.status==auto_driver::Status::Ready);
    assert(std::strcmp(selected_lansen.driver,"lansenpu")==0);
  }
  // Earlier batch-2 families remain excluded from AUTO decode.
  const auto minomess_auto=auto_driver::resolve(
      "auto",signatures,signature_count,0x6A49,0x01,0x06);
  const auto kamwater_auto=auto_driver::resolve(
      "auto",signatures,signature_count,0x2C2D,0x1B,0x16);
  const auto apator_auto=auto_driver::resolve(
      "auto",signatures,signature_count,0x0601,0x05,0x06);
  assert(minomess_auto.status==auto_driver::Status::NotCompiled);
  assert(kamwater_auto.status==auto_driver::Status::NotCompiled);
  assert(apator_auto.status==auto_driver::Status::NotCompiled);
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
  assert(auto_driver::identify(collision,2,0x5068,0x95,0x37).status==
         auto_driver::RecognitionStatus::Ambiguous);
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
