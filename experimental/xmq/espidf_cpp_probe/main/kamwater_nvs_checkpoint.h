#pragma once
// ESP-IDF PROBE ONLY. Never included by the native SX126x receiver.
// Real ESP-IDF NVS adapter for the portable authenticated CCM watermark.
// Caller must initialize an appropriate NVS partition and explicitly
// provision/validate a checkpoint before loading. NO automatic erasure,
// self-provisioning, key epoch reset, or writing on each received frame.
// NVS integrity/CRC is not a malicious rollback defense.
#include <cstddef>
#include <cstdint>
#include "esp_err.h"
#include "kamwater_replay_checkpoint.h"

namespace xmq_portable { namespace kamwater { namespace nvs_probe {
using Meter=checkpoint::Meter;
using Session=checkpoint::Session;
enum class Status : uint8_t {
  Ok, InvalidArgument, Missing, OpenFailed, ReadFailed,
  CheckpointRejected, StaleRecord, SetFailed, CommitFailed, ReadbackFailed
};
struct Result {
  Status status{Status::InvalidArgument};
  esp_err_t esp_error{ESP_OK};
  checkpoint::Status checkpoint_status{checkpoint::Status::NotReady};
};
constexpr const char *kNamespace="kw_ccm_probe";
// The key is 14 ASCII chars: "kw" + manufacturer hex + eight-digit BCD ID.
// Meter version, medium and key epoch are checked inside the stored blob.
// NVS key and namespace have a 15-character maximum.
bool make_key(const Meter &configured, char (&out)[16]);
// Read ONLY. On any failure session is left unready; missing NVS
// partition/namespace/value never causes a reset or default watermark.
Result load(Session &session,const Meter &configured,uint32_t epoch);
// Validates the canonical encoded snapshot against configured meter and
// epoch BEFORE writing. NVS set_blob + commit + independent readback.
// Must be serialized by its caller, and invoked only at an explicit
// persistence boundary. No live packet code calls this implementation.
Result store(const uint8_t *bytes,size_t length,
             const Meter &configured,uint32_t epoch);
// Inert link anchor to ensure ESP-IDF build/link compiles the backend.
// Does not open NVS, use keys, or write flash.
const char *backend_name();
}}} // namespace xmq_portable::kamwater::nvs_probe
