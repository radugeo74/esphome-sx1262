#!/usr/bin/env python3
"""Guard SX1276 opt-in HF boost and STANDBY readback against RX SPI races."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cpp = (root / "components/sx127x/sx127x.cpp").read_text()
header = (root / "components/sx127x/sx127x.h").read_text()
schema = (root / "components/sx127x/__init__.py").read_text()
normal_yaml = (root / "tests/lilygo-t3s3-sx1276-c1a-only-ci.yaml").read_text()
boost_yaml = (root / "tests/lilygo-t3s3-sx1276-c1a-lna-boost-ci.yaml").read_text()

assert 'bool wmbus_lna_boost_hf_{false};' in header
assert 'CONF_WMBUS_LNA_BOOST_HF = "wmbus_lna_boost_hf"' in schema
assert 'cv.Optional(CONF_WMBUS_LNA_BOOST_HF, default=False)' in schema
assert 'set_wmbus_lna_boost_hf(config[CONF_WMBUS_LNA_BOOST_HF])' in schema
assert 'if config[CONF_WMBUS_LNA_BOOST_HF] and not config[CONF_WMBUS_T1_MODE]:' in schema
assert 'wmbus_lna_boost_hf:' not in normal_yaml
assert 'wmbus_lna_boost_hf: true' in boost_yaml
assert 'sync_value: [0x54, 0xCD]' in boost_yaml
assert 'native_stream_mode: T1_C1' in boost_yaml
assert '\nsensor:' not in boost_yaml
assert 'name: lilygo-t3s3-c1a-lna-ci' in boost_yaml
assert "bool lna_standby_captured{false};" in header

configure = cpp.split('void SX127x::configure() {', 1)[1].split(
    'void SX127x::configure_fsk_ook_()', 1)[0]
a = configure.index('this->set_mode_standby();')
b = configure.index('const uint8_t before = this->read_register_(REG_LNA);')
c = configure.index('if (this->wmbus_lna_boost_hf_) {')
d = configure.index('this->write_register_(REG_LNA, sx127x_lna_with_hf_boost(before));')
e = configure.index('after = this->read_register_(REG_LNA);')
f = configure.index('this->set_mode_rx();')
assert a < b < c < d < e < f
assert 'this->wmbus_t1_mode_ && this->modulation_ == MOD_FSK && this->rx_start_' in configure
assert '(after & LNA_BOOST_HF_MASK) == LNA_BOOST_HF_ON' in configure
assert 'if (!boost_ok)' in configure and 'this->mark_failed();' in configure
assert configure.count('this->write_register_(REG_LNA,') == 1
assert 'lna_standby_before' in configure and 'lna_standby_after' in configure

task = cpp.split('void SX127x::wmbus_task_()', 1)[1].split('void SX127x::dump_config()', 1)[0]
dump = cpp.split('void SX127x::dump_config()', 1)[1]
assert 'REG_LNA' not in task and 'REG_LNA' not in cpp.split('void IRAM_ATTR SX127x::wmbus_fifo_intr_', 1)[1].split('void SX127x::wmbus_task_entry_', 1)[0]
assert 'read_register_(' not in dump and 'write_register_(' not in dump
assert 'STANDBY pre-RX RegLna[0C]' in dump
print('PASS SX1276 LNA: default OFF, opt-in only, STANDBY before RX, no RX SPI')
