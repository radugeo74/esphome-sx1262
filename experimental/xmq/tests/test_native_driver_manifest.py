#!/usr/bin/env python3
"""Native XMQ manifest regression; no personal telegrams or meter IDs."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental" / "xmq" / "tools"))
from native_driver_specs import SUPPORTED, CONFIGURED_NATIVE, CONFIGURABLE, GENERATED, PORTABLE, SECURE_ONLY, validate, catalog_fragment, compiled_fragment
from build_native_generated import build
from tempfile import TemporaryDirectory
from generate_ixml_cpp import generate

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
    assert "lansenpu" in SUPPORTED
    assert 'std::strcmp(name, "lansenpu")' in compiled
    for experimental in CONFIGURED_NATIVE:
        assert experimental in CONFIGURABLE
        assert experimental not in SUPPORTED
        assert f'std::strcmp(name, "{experimental}")' not in compiled
    assert SECURE_ONLY == ("mkradio4a",)
    assert "mkradio4a" not in CONFIGURABLE
    assert "mkradio4a" not in compiled  # not generic plaintext/AUTO
    assert set(CONFIGURABLE).isdisjoint(SECURE_ONLY)
    native_secure = (ROOT / "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h").read_text()
    assert 'std::strcmp(driver,"mkradio4a")==0' in native_secure
    assert "decode_mkradio4a_mode5(" in native_secure
    native_yaml = (ROOT / "components/wmbus_native_sx126x/__init__.py").read_text()
    assert 'cv.one_of("minomess", "kamwater", "apator162", "mkradio4a"' in native_yaml
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
