#!/usr/bin/env python3
"""Regenerate the exact native XMQ catalog used by ESPHome and host CI.

Do not scan arbitrary XMQ files into firmware: an unreviewed driver remains
metadata-only until the manifest declares a verified decoder.
"""
from pathlib import Path
from native_driver_specs import validate, catalog_fragment, compiled_fragment
from generate_ixml_cpp import generate

def build(root: Path):
    xmq = root / "experimental" / "xmq"
    component = root / "components" / "wmbus_native_sx126x"
    sources = validate(xmq)
    generated = generate(sources)
    outputs = {
        "xmq_generated.h": generated,
        "xmq_native_descriptors.inc": catalog_fragment(),
        "xmq_native_compiled.h": compiled_fragment(),
    }
    for name, contents in outputs.items():
        (component / name).write_text(contents, encoding="utf-8")
    return outputs

if __name__ == "__main__":
    build(Path(__file__).resolve().parents[3])
