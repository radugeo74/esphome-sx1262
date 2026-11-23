#!/usr/bin/env python3
"""Aggregate selected-meter Stage 6 timing logs locally (no IDs, payloads or values).

Use: python tools/summarize_stage6_timing.py /path/to/heltec.log
The observed phases are NOT a complete radio-loop timing budget; independent
line timestamps and SX126x warnings do not establish a causal relationship.
"""
import argparse
import pathlib
import re
import statistics

MARKERS = {
    "legacy_decode_us": "XMQ_STAGE6_PIPELINE",
    "snapshot_after_us": "XMQ_STAGE6_PIPELINE",
    "observer_us": "XMQ_STAGE6_PIPELINE",
    "callback_us": "XMQ_STAGE6_PUBLISH",
    "stage5_classify_us": "XMQ_STAGE4_T1_DIRECT_DIAGNOSTIC",
    "stage45_total_us": "XMQ_STAGE4_T1_DIRECT_DIAGNOSTIC",
    "stage1_compare_us": "XMQ_STAGE45_REUSE",
}
NUM = re.compile(r"(?<![\w])([a-z][a-z0-9_]+)=(\d+)(?![\w])")
MAX_US = 60_000_000


def summarize(lines):
    samples = {key: [] for key in MARKERS}
    counts = {"pipeline": 0, "publish": 0, "stage45_reused": 0,
              "stage45_eligible": 0, "sx126x_loop_warnings": 0,
              "invalid_timing": 0}
    for line in lines:
        if "sx126x" in line.lower() and "took a long time" in line.lower():
            counts["sx126x_loop_warnings"] += 1
        marker = next((m for m in set(MARKERS.values()) if m in line), None)
        if marker is None:
            continue
        fields = dict(NUM.findall(line.split(marker, 1)[1]))
        if marker == "XMQ_STAGE6_PIPELINE":
            counts["pipeline"] += 1
        if marker == "XMQ_STAGE6_PUBLISH":
            counts["publish"] += 1
        if marker == "XMQ_STAGE45_REUSE":
            counts["stage45_reused"] += fields.get("reused") == "1"
            counts["stage45_eligible"] += fields.get("eligible") == "1"
        for key, owner in MARKERS.items():
            if owner != marker or key not in fields:
                continue
            value = int(fields[key])
            if value > MAX_US:
                counts["invalid_timing"] += 1
            else:
                samples[key].append(value)
    return counts, samples


def report(counts, samples):
    lines = ["STAGE6_LOCAL_TIMING (aggregate only; no IDs, values or payloads)",
             "pipeline_frames={} publish_callbacks={} stage45_eligible={} stage45_reused={} sx126x_loop_warnings={} invalid_timing={}".format(
                 counts["pipeline"], counts["publish"], counts["stage45_eligible"],
                 counts["stage45_reused"], counts["sx126x_loop_warnings"],
                 counts["invalid_timing"])]
    for key in MARKERS:
        values = sorted(samples[key])
        if not values:
            lines.append(f"{key}: n=0")
            continue
        p95 = values[(95 * (len(values) - 1) + 99) // 100]
        lines.append(f"{key}: n={len(values)} min_us={values[0]} median_us={statistics.median(values):g} p95_nearest_us={p95} max_us={values[-1]}")
    lines.append("LIMIT: phase durations exclude parts of radio processing; warnings cannot be attributed to XMQ from aggregate timings alone.")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    with args.log.open(encoding="utf-8", errors="replace") as source:
        print(report(*summarize(source)))


if __name__ == "__main__":
    main()
