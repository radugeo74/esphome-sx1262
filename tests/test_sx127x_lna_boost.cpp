// SX1276 RegLna HF boost is bit-mask-only and fully opt-in.
#include <cstdint>
#include <cstdio>
#include "components/sx127x/sx127x_reg.h"

using namespace esphome::sx127x;

static_assert(REG_LNA == 0x0C);
static_assert(LNA_BOOST_HF_MASK == 0x03);
static_assert(LNA_BOOST_HF_ON == 0x03);
static_assert(sx127x_lna_with_hf_boost(0x00) == 0x03);
static_assert(sx127x_lna_with_hf_boost(0x20) == 0x23);
static_assert(sx127x_lna_with_hf_boost(0x80) == 0x83);
static_assert(sx127x_lna_with_hf_boost(0xAC) == 0xAF);
static_assert(sx127x_lna_with_hf_boost(0xFF) == 0xFF);
static_assert(sx127x_lna_with_hf_boost(0x23) == 0x23);

int main() {
  for (unsigned v = 0; v < 256; ++v) {
    const auto before = static_cast<uint8_t>(v);
    const auto after = sx127x_lna_with_hf_boost(before);
    if ((before & 0xFC) != (after & 0xFC) ||
        (after & LNA_BOOST_HF_MASK) != LNA_BOOST_HF_ON)
      return 1;
  }
  std::puts("PASS SX1276 HF boost: only low 2 bits written, all 256 values preserve remaining bits");
}
