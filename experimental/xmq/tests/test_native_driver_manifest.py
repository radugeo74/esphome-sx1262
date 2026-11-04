#!/usr/bin/env python3
"""Native XMQ manifest regression; no personal telegrams or meter IDs."""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental" / "xmq" / "tools"))
from native_driver_specs import SUPPORTED, GENERATED, PORTABLE, validate, catalog_fragment, compiled_fragment
from build_native_generated import build

def main():
    assert SUPPORTED == tuple(d.name for d in GENERATED) + PORTABLE
    assert len(SUPPORTED) == len(set(SUPPORTED))
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
    assert "mkradio4a" not in compiled  # converter is identification-only
    native = (ROOT / "components" / "wmbus_native_sx126x" / "__init__.py").read_text()
    assert 'components" / "wmbus_radio' not in native
    assert "validate_driver_name" in native
    print("PASS native driver manifest matches generated decode and AUTO catalog")

if __name__ == "__main__":
    main()
