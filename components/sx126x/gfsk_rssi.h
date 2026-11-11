#pragma once
// Semtech SX1261/SX1262 GetPacketStatus (0x14), GFSK only.
// 3 response bytes AFTER SPI status:
//   [0] RxStatus; [1] RSSI at sync word; [2] average RSSI over packet.
// -raw/2.0 dBm for both. RssiAvg is latched at packet done (RxDone).
// Our progressive T1/C1 streaming receiver intentionally completes on FIFO
// length BEFORE a reliable RxDone. Publish RssiSync, never premature RssiAvg.
// Pure decode helper: SPI ownership and actual RF packet qualification stay
// exclusively in the official SX126x receiver.
#include <cstdint>
namespace esphome { namespace sx126x { namespace gfsk_rssi {

struct Measurement {
  uint8_t rx_status{0};
  uint8_t sync_raw{0};
  uint8_t average_raw{0};
  float sync_dbm{0};
  float average_dbm{0};
  // GFSK RxStatus bit 1 is 'packet received', not 'sync detected'.
  // Even if true, telemetry uses the sync sample for consistency.
  bool packet_received{false};
};
inline constexpr float dbm(uint8_t raw) {
  return -static_cast<float>(raw)/2.0f;
}
inline constexpr Measurement parse(const uint8_t *status) {
  return {status[0],status[1],status[2],
          dbm(status[1]),dbm(status[2]),
          (status[0]&0x02u)!=0};
}
}}}  // namespace esphome::sx126x::gfsk_rssi
