#!/usr/bin/env python3
"""Native XMQ manifest regression; no personal telegrams or meter IDs."""
from pathlib import Path
import re
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental" / "xmq" / "tools"))
from native_driver_specs import SUPPORTED, CONFIGURED_NATIVE, CONFIGURABLE, GENERATED, PORTABLE, SECURE_ONLY, validate, catalog_fragment, compiled_fragment
from build_native_generated import build
from tempfile import TemporaryDirectory
from generate_ixml_cpp import generate
from generate_clear_profiles import generate as generate_clear_profiles, SPECS
from generate_vario451_ixml import generate as generate_vario451_ixml

def main():
    assert SUPPORTED == tuple(d.name for d in GENERATED) + PORTABLE
    assert len(SUPPORTED) == len(set(SUPPORTED))
    assert CONFIGURED_NATIVE == ("minomess", "kamwater", "apator162")
    assert CONFIGURABLE == SUPPORTED + CONFIGURED_NATIVE
    assert validate(ROOT / "experimental" / "xmq")
    generated = build(ROOT)
    catalog = generated["xmq_native_descriptors.h"]
    compiled = generated["xmq_native_compiled.h"]
    assert catalog == catalog_fragment()
    assert compiled == compiled_fragment()
    for driver in SUPPORTED:
        assert f'std::strcmp(name, "{driver}") == 0' in compiled
    for driver in GENERATED:
        assert f'generated_ixml::{driver.name}::rules' in catalog
        assert f'namespace {driver.name} ' in generated["xmq_generated.h"]
    assert tuple(SPECS) == ("munia","cma12w","lansendw","lansenpu","supercom587")
    assert generated["xmq_generated_clear_profiles.h"] == generate_clear_profiles(
        ROOT / "experimental/xmq/drivers")
    vario = generated["xmq_generated_vario451.h"]
    assert vario == generate_vario451_ixml(
        ROOT / "experimental/xmq/drivers/vario451.xmq")
    assert "previous_payload_offset = 3;" in vario
    assert "current_payload_offset = 7;" in vario
    assert "kwh_divisor = 3.6;" in vario
    vario_decoder = (ROOT / "components/wmbus_native_sx126x/vario451_plain.h").read_text()
    assert '#include "xmq_generated_vario451.h"' in vario_decoder
    assert "xmq_generated_vario451::previous_payload_offset" in vario_decoder
    assert "xmq_generated_vario451::current_payload_offset" in vario_decoder
    assert "xmq_generated_vario451::kwh_divisor" in vario_decoder
    # Compiler must fail closed on changed IXML, MVT, scaling, DIF/VIF
    # or source fixture; this does not authorize additional RF profiles.
    source = (ROOT / "experimental/xmq/drivers/vario451.xmq").read_text()
    mutations = (
        ("hex, hex, hex, hex, @DV_PREV",
         "hex, hex, hex, hex, hex, @DV_PREV"),
        ("curr_raw_counter / 3.6 counter * 1 kwh",
         "curr_raw_counter / 3.5 counter * 1 kwh"),
        ("mvt = TCH,27,C3", "mvt = TCH,27,04"),
        ("difvifkey = 027E", "difvifkey = 027D"),
        ("129F25383300A86226", "129F25383300A86326"),
    )
    with TemporaryDirectory() as tmp:
        fake = Path(tmp) / "vario451.xmq"
        for original, replacement in mutations:
            assert original in source, original
            fake.write_text(source.replace(original, replacement, 1))
            try:
                generate_vario451_ixml(fake)
            except ValueError:
                pass
            else:
                raise AssertionError("Vario451 generator admitted changed XMQ: " + original)
    supercom_generated = generated["xmq_generated_clear_profiles.h"]
    assert "namespace supercom587 {" in supercom_generated
    assert "inline constexpr size_t verified_frame_bytes = 163;" in supercom_generated
    assert "15, // total_m3" in supercom_generated
    assert "123, // software_version" in supercom_generated
    assert "158, // status" in supercom_generated
    text_router = (ROOT / "components/wmbus_native_sx126x/xmq_plain_text_fields.h").read_text()
    assert 'append(out,"software_version",reading.software_version);' in text_router
    native_yaml = (ROOT / "components/wmbus_native_sx126x/__init__.py").read_text()
    assert '"software_version"' in native_yaml  # binding exposed as ESPHome text_sensor
    for driver in SPECS:
        header = (ROOT / "components/wmbus_native_sx126x" / f"{driver}_plain.h").read_text()
        assert '#include "xmq_generated_clear_profiles.h"' in header
        assert f"xmq_generated_clear::{driver}::models" in header
        assert f"xmq_generated_clear::{driver}::rules" in header
    assert "lansenpu" in SUPPORTED
    assert 'std::strcmp(name, "lansenpu")' in compiled
    for experimental in CONFIGURED_NATIVE:
        assert experimental in CONFIGURABLE
        assert experimental not in SUPPORTED
        assert f'std::strcmp(name, "{experimental}")' not in compiled
    assert SECURE_ONLY == ("mkradio4a", "topaseskr", "lansenth", "lansensm")
    assert "mkradio4a" not in CONFIGURABLE
    assert "topaseskr" not in CONFIGURABLE
    assert "lansenth" not in CONFIGURABLE and "lansensm" not in CONFIGURABLE
    assert "mkradio4a" not in compiled
    assert "topaseskr" not in compiled  # not generic plaintext/AUTO
    assert "lansenth" not in compiled and "lansensm" not in compiled
    assert set(CONFIGURABLE).isdisjoint(SECURE_ONLY)
    native_secure = (ROOT / "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h").read_text()
    assert 'std::strcmp(driver,"mkradio4a")==0' in native_secure
    assert "decode_mkradio4a_mode5(" in native_secure
    assert "decode_topaseskr_mode5(" in native_secure
    native_yaml = (ROOT / "components/wmbus_native_sx126x/__init__.py").read_text()
    assert 'cv.one_of("minomess", "kamwater", "apator162", "mkradio4a"' in native_yaml
    # A successfully compiled YAML must not silently lose secure_meters
    # bindings at runtime. Cross-check the EXACT YAML and C++ allowlists.
    cxx_bindings = (ROOT / "components/wmbus_native_sx126x/secure_meter_binding.h").read_text()
    receiver = (ROOT / "components/wmbus_native_sx126x/native_receiver_probe.h").read_text()
    yaml_match = re.search(
        r"cv.Required\(CONF_DRIVER\): cv.one_of\(([^\n]+)", native_yaml)
    cxx_match = re.search(
        r"inline bool supported_driver\([^)]*\) \{(.*?)\n\}",
        cxx_bindings, re.S)
    assert yaml_match and cxx_match
    yaml_drivers = set(re.findall(r'"([a-z0-9]+)"', yaml_match.group(1)))
    runtime_drivers = set(re.findall(r'driver == "([a-z0-9]+)"', cxx_match.group(1)))
    assert yaml_drivers == runtime_drivers == set(CONFIGURED_NATIVE + SECURE_ONLY), (
        yaml_drivers, runtime_drivers)
    assert "secure_bindings::add(this->secure_meters_,id,driver,key_hex,dll_id)" in receiver
    assert 'driver != "minomess" && driver != "mkradio4a"' in cxx_bindings
    assert 'entry[CONF_DRIVER] not in ("minomess", "mkradio4a")' in native_yaml
    native = (ROOT / "components" / "wmbus_native_sx126x" / "__init__.py").read_text()
    assert 'components" / "wmbus_radio' not in native
    assert "validate_driver_name" in native
    assert "from native_driver_specs import CONFIGURABLE" in native
    # A new driver containing unsupported numeric operators must fail
    # compilation rather than silently omit a measurement field.
    official = (ROOT / "experimental/xmq/drivers/mkradio4.xmq").read_text()
    assert "prev_raw_counter * 1m3" in official
    with TemporaryDirectory() as tmp:
        invalid = Path(tmp) / "mkradio4.xmq"
        invalid.write_text(official.replace(
            "prev_raw_counter * 1m3", "prev_raw_counter / 1m3"))
        try:
            generate([invalid])
        except ValueError as exc:
            assert "unsupported calculation" in str(exc)
        else:
            raise AssertionError("Unsupported XMQ arithmetic was silently accepted")
    print("PASS native driver manifest matches generated decode and AUTO catalog")

if __name__ == "__main__":
    main()
