#!/usr/bin/env python3
"""Regression: host-only AES helpers must never leak into ESPHome staging."""
import importlib.util
import tempfile
from pathlib import Path

script = Path(__file__).with_name("stage_shadow_headers.py")
spec = importlib.util.spec_from_file_location("xmq_stage_shadow_headers_test", script)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory(prefix="xmq-stage-allowlist-") as tmp:
    root = Path(tmp)
    dst = root / "components" / "xmq_shadow_probe"
    dst.mkdir(parents=True)
    (root / "xmq_shadow_safe.h").write_text("#pragma once\n")
    (root / "xmq_tpl5_cbc.h").write_text('#include "aes.h"\n')
    (root / "xmq_tpl5_direct_header.h").write_text('#include "xmq_tpl5_cbc.h"\n')
    (root / "xmq_tpl5_identity_gate.h").write_text("#pragma once\\n")
    (root / "xmq_tpl5_live_admission.h").write_text("#pragma once\\n")
    # Simulate cached generated output from an older staging invocation.
    (dst / "xmq_tpl5_cbc.h").write_text('#include "aes.h"\n')
    (dst / "xmq_tpl5_direct_header.h").write_text('#include "xmq_tpl5_cbc.h"\n')
    (dst / "xmq_tpl5_identity_gate.h").write_text("#pragma once\\n")
    (dst / "xmq_tpl5_live_admission.h").write_text("#pragma once\\n")
    module.ROOT = root
    module.DEST = dst
    module.main()
    assert (dst / "xmq_shadow_safe.h").exists()
    assert not (dst / "xmq_tpl5_cbc.h").exists()
    assert not (dst / "xmq_tpl5_direct_header.h").exists()
    assert not (dst / "xmq_tpl5_identity_gate.h").exists()
    assert not (dst / "xmq_tpl5_live_admission.h").exists()
    assert (root / "xmq_tpl5_cbc.h").exists(), "host-only source must not be deleted"
print("PASS: ESPHome XMQ staging excludes host-only CBC dependencies and stale files")
