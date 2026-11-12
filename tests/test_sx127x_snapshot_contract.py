#!/usr/bin/env python3
"""Verify register snapshot is captured during setup, not from dump_config."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
header = (root / "components/sx127x/sx127x.h").read_text()
source = (root / "components/sx127x/sx127x.cpp").read_text()

assert "struct WmbusFskRegisters" in header
assert "bool captured{false};" in header
assert "bool matches_expected{false};" in header
setup = source.split("void SX127x::configure_fsk_ook_()", 1)[1].split("void SX127x::configure_lora_()", 1)[0]
assert "read_register_(REG_AFC_BW)" in setup
assert "this->wmbus_fsk_registers_ = {rx_bw, afc_bw, rx_config, afc_fei," in setup
dump = source.split("void SX127x::dump_config()", 1)[1]
assert "regs.captured" in dump
assert "regs.matches_expected" in dump
assert "regs.afc_bw" in dump
assert "read_register_(" not in dump, "Never read SX1276 SPI from dump_config during RX"
print("PASS SX1276: registers cached at setup, dump_config reads cache without SPI")
