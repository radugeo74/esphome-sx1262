#!/usr/bin/env python3
"""Test privacy and aggregation of the offline XMQ shadow-log parser."""
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("summarize_shadow_log", root / "summarize_shadow_log.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
fake_id = "98765432"
fake_payload = "2F446850FACEBEEF"
lines = [
    "[I][wmbus:313]: Native SX126x verified T1/A (48 bytes, RSSI -55 dBm)",
    "[I][wmbus:313]: Native SX126x verified C1/A (55 bytes, RSSI -80 dBm)",
    "[W][wmbus_meter:053]: Frame link mode C1 not supported by meter mkradio4-" + fake_id,
    "[W][wmbus:125]: Unhandled meter address: " + fake_id,
    "[W][wmbus:127]: https://wmbusmeters.org/analyze/" + fake_payload,
    "[I][xmq_shadow_probe:125]: XMQ shadow hourly observation budget exhausted (max 128, read-only)",
    "[I][xmq_shadow_probe:124]: XMQ shadow hourly observation budget renewed (max 128, read-only)",
    "[I][xmq_shadow_probe:106]: Live XMQ shadow: driver=mkradio4 "
    "disposition=1 compared=2 mismatches=0 matched=1 different=0 "
    "observed=1 captured=1 rejected=0 changed_numeric=2 "
    "matched_numeric_dv=0 unmatched_numeric_dv=2 "
    "provenance=not_proven min_free_heap=164940",
    "[I][xmq_shadow_probe:106]: Live XMQ shadow: driver=fhkvdataiii "
    "disposition=2 compared=4 mismatches=1 matched=0 different=1 "
    "observed=2 captured=2 rejected=0 changed_numeric=1 "
    "matched_numeric_dv=1 unmatched_numeric_dv=3 "
    "provenance=not_proven min_free_heap=160000",
]
counts, drivers = module.summarize(lines)
assert counts["verified_T1_A"] == 1
assert counts["verified_C1_A"] == 1
assert counts["c1_link_rejected"] == 1
assert counts["shadow_budget_exhausted"] == 1
assert counts["shadow_budget_renewed"] == 1
assert drivers["mkradio4"]["matches"] == 1
assert drivers["mkradio4"]["compared"] == 2
assert drivers["mkradio4"]["unmatched_numeric_dv"] == 2
assert drivers["fhkvdataiii"]["differences"] == 1
assert drivers["fhkvdataiii"]["mismatches"] == 1
assert drivers["fhkvdataiii"]["observed"] == 2
summary = module.format_summary(counts, drivers)
assert fake_id not in summary and fake_payload not in summary
assert "provenance" in summary and "NOT proof" in summary
assert "matched_numeric_dv=0" in summary
assert "shadow_budget: exhausted=1 renewed=1" in summary
assert "no observed comparisons" in module.format_summary(*module.summarize([]))

# The firmware restarts its observed/captured/rejected snapshots every hour.
# Do not let an exhausted prior window mask the smaller renewed value.
two_windows = [
    "[I][xmq_shadow_probe]: Live XMQ shadow: driver=mkradio4 disposition=1 "
    "compared=2 mismatches=0 observed=128 captured=120 rejected=8 provenance=not_proven",
    "[I][xmq_shadow_probe]: XMQ shadow hourly observation budget renewed (max 128, read-only)",
    "[I][xmq_shadow_probe]: Live XMQ shadow: driver=mkradio4 disposition=1 "
    "compared=2 mismatches=0 observed=1 captured=1 rejected=0 provenance=not_proven",
]
window_counts, window_drivers = module.summarize(two_windows)
assert window_counts["shadow_budget_renewed"] == 1
assert window_drivers["mkradio4"]["observations"] == 2
assert window_drivers["mkradio4"]["matches"] == 2
assert window_drivers["mkradio4"]["observed"] == 1
assert window_drivers["mkradio4"]["captured"] == 1
assert window_drivers["mkradio4"]["rejected"] == 0

print("SHADOW_LOCAL_LOG_SUMMARY_OK: aggregated synthetic T1/C1 and XMQ diagnostics; IDs/payload never printed")
