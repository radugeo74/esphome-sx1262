#!/usr/bin/env python3
"""Check bundled GitHub-installable XMQ driver texts against original sources.

The local stage-A build and direct GitHub external_components build must decode
with the exact same public XMQ rules. No live keys or device data are read.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
bundled = (root / "components/xmq_shadow_probe/xmq_driver_sources.h").read_text()
for driver in ("mkradio4", "mkradio4a", "fhkvdataiii"):
    original = (root / "upstream/src" / f"{driver}.xmq").read_text()
    start = f'inline const char *source_{driver}() {{ return R"XMQSOURCE('
    end = ')XMQSOURCE"; }'
    assert bundled.count(start) == 1, f"missing or duplicate {driver} source"
    payload = bundled.split(start, 1)[1].split(end, 1)[0]
    # Only a single delimiting newline is introduced around the verbatim XMQ
    # source; preserve ALL original source syntax including field matchers.
    assert payload.rstrip("\n") == original.rstrip("\n"), (
        f"bundled {driver} XMQ differs from original upstream source"
    )
print("XMQ_BUNDLED_DRIVER_SOURCES_OK: original mkradio4/mkradio4a/fhkvdataiii rules")
