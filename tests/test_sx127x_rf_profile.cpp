// Host-only regression for SX1276 FSK RF register values. No ESPHome runtime.
#include <cstdint>
#include <cstdio>
#include "components/sx127x/sx127x_reg.h"

using namespace esphome::sx127x;

constexpr uint32_t bandwidth_hz(uint8_t reg) {
  const uint8_t mant_sel = (reg >> 3) & 0x03;
  const uint32_t mant = mant_sel == 0 ? 16U : mant_sel == 1 ? 20U :
                        mant_sel == 2 ? 24U : 0U;
  return mant == 0 ? 0 : 32000000U / (mant * (1U << ((reg & 0x07U) + 2U)));
}

static_assert(REG_RX_CONFIG == 0x0D, "RegRxConfig address must stay FSK/OOK");
static_assert(REG_RSSI_THRESH == 0x10, "RegRssiThresh register address changed");
static_assert(REG_RX_BW == 0x12, "RegRxBw register address changed");
static_assert(REG_AFC_BW == 0x13, "RegAfcBw register address changed");
static_assert(REG_AFC_FEI == 0x1A, "RegAfcFei register address changed");
static_assert(REG_PREAMBLE_DETECT == 0x1F, "RegPreambleDetect register address changed");
static_assert(REG_SYNC_CONFIG == 0x27, "RegSyncConfig register address changed");

static_assert(RX_BW_200_0 == 0x09, "RX 200-kHz encoding changed");
static_assert(RX_BW_250_0 == 0x01, "RX 250-kHz encoding changed");
static_assert(AFC_BW_333_3 == 0x10, "AFC 333-kHz encoding changed");
static_assert(bandwidth_hz(RX_BW_200_0) == 200000, "RX 200 kHz is not 200 kHz");
static_assert(bandwidth_hz(RX_BW_250_0) == 250000, "RX 250 kHz is not 250 kHz");
static_assert(bandwidth_hz(AFC_BW_333_3) == 333333, "AFC 0x10 must mean 333.3 kHz");
static_assert(bandwidth_hz(0x0B) == 50000, "AFC reset/default is ~50 kHz");
static_assert((AFC_AUTO_ON | AGC_AUTO_ON | TRIGGER_PREAMBLE) == 0x1E,
              "AFC+AGC triggered from preamble changed");
static_assert(AFC_AUTO_CLEAR_ON == 0x01, "AfcAutoClearOn is bit 0, not AGC start bit 4");

int main() {
  std::puts("PASS SX1276: FSK RX 250k/200k, AFC 333k and AFCAutoClear register encodings");
}
