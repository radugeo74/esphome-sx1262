#!/usr/bin/env python3
"""Synthetic-only regression for privacy-safe Stage 6 telemetry aggregation."""
import importlib.util
from pathlib import Path

source = Path(__file__).with_name("summarize_stage6_timing.py")
spec = importlib.util.spec_from_file_location("stage6_summary", source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

secret = "98765432"
payload = "AABBFF012345"
lines = [
    f"[I] meter={secret} raw={payload} XMQ_STAGE6_PIPELINE legacy_decode_us=100 snapshot_after_us=20 observer_us=300 selected_meter_only=1",
    "[I] XMQ_STAGE6_PUBLISH callback_us=90 selected_meter_only=1 deferred=1",
    "[I] XMQ_STAGE45_REUSE reused=1 eligible=1 stage1_compare_us=30 fallback=legacy",
    "[I] XMQ_STAGE4_T1_DIRECT_DIAGNOSTIC stage5_classify_us=9 stage45_total_us=8801",
    "[W] Component sx126x took a long time for an operation (55 ms)",
    "[I] XMQ_STAGE6_PIPELINE legacy_decode_us=200 snapshot_after_us=30 observer_us=400",
    "[I] XMQ_STAGE6_PIPELINE legacy_decode_us=999999999999999999 snapshot_after_us=40",
    "[I] telemetry XMQ_STAGE6_PUBLISH callback_us=garbage",
]
counts, samples = module.summarize(lines)
output = module.report(counts, samples)
assert counts["pipeline"] == 3
assert counts["publish"] == 2
assert counts["stage45_reused"] == counts["stage45_eligible"] == 1
assert counts["sx126x_loop_warnings"] == 1
assert counts["invalid_timing"] == 1
assert samples["legacy_decode_us"] == [100, 200]
assert samples["snapshot_after_us"] == [20, 30, 40]
assert samples["observer_us"] == [300, 400]
assert "legacy_decode_us: n=2 min_us=100 median_us=150" in output
assert secret not in output and payload not in output
assert "cannot be attributed" in output
assert "callback_us: n=1" in output
assert "observer_us: n=0" in module.report(*module.summarize([]))
print("STAGE6_TIMING_LOCAL_SUMMARY_OK")
