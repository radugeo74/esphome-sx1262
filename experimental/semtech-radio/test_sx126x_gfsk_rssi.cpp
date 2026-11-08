// Audit point 7: Semtech GetPacketStatus GFSK layout, not generic
// LoRa or GFSK after-RxDone packet average. No RF hardware required.
#include "components/sx126x/gfsk_rssi.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

using esphome::sx126x::gfsk_rssi::Measurement;
using esphome::sx126x::gfsk_rssi::parse;
using esphome::sx126x::gfsk_rssi::dbm;

static bool near(float a,float b) {return std::fabs(a-b)<0.001f;}

int main() {
  // User-observed old average ~225 -> -112.5 dBm. Same packet can
  // have stronger RssiSync because average is not yet latched before RxDone.
  uint8_t sample[3]={0x00,105,225};
  const Measurement a=parse(sample);
  assert(a.sync_raw==105 && a.average_raw==225);
  assert(near(a.sync_dbm,-52.5f));
  assert(near(a.average_dbm,-112.5f));
  assert(!a.packet_received);
  // Distinct fields cannot silently be swapped: 151 -> -75.5.
  sample[1]=151;
  const auto b=parse(sample);
  assert(near(b.sync_dbm,-75.5f) && near(b.average_dbm,-112.5f));
  // Once the chip reports pkt_received, RssiAvg may be interpreted
  // as packet average, but the progressive stream remains sync-sourced.
  sample[0]=0x02;
  const auto c=parse(sample);
  assert(c.packet_received && near(c.sync_dbm,-75.5f));
  assert(near(c.average_dbm,-112.5f));
  // Raw RSSI is UNSIGNED. Casting to int8_t before conversion is wrong.
  assert(near(dbm(225),-112.5f));
  assert(near(dbm(0),0));
  assert(near(dbm(255),-127.5f));
  std::puts("PASS SX1262 GFSK RSSI: response[1] sync, [2] avg; "
            "unsigned raw/2, progressive RX chooses sync; no RF claims.");
}
