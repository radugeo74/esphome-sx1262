#!/usr/bin/env python3
"""Summarize local ESPHome XMQ shadow logs without printing meter IDs or payloads.

Read-only offline diagnostic. Logs are provided as a local positional file.
Only aggregate counters are printed; no telemetry or data is sent anywhere.
A 'matched' shadow comparison is NOT proof of setter-level provenance.
"""
import argparse
import collections
import pathlib
import re

SHADOW = re.compile(r"Live XMQ shadow: driver=(\w+)\s+(.*)")
FIELD = re.compile(r"([a-z_]+)=([\w.-]+)")
VERIFIED = re.compile(r"Native SX126x verified (T1/A|C1/A)")
COUNTERS = (
    "compared", "mismatches", "observed", "captured", "rejected",
    "changed_numeric", "unchanged_numeric",
    "matched_numeric_dv", "unmatched_numeric_dv",
    "matched_date_dv", "unmatched_date_dv", "calendar_refs",
)

def summarize(lines):
    counts = collections.Counter()
    drivers = collections.defaultdict(collections.Counter)
    for line in lines:
        counts["shadow_budget_exhausted"] += (
            "XMQ shadow hourly observation budget exhausted" in line
        )
        renewed = "XMQ shadow hourly observation budget renewed" in line
        counts["shadow_budget_renewed"] += renewed
        if renewed:
            # These three firmware counters restart at every hourly window.
            # Preserve event totals but clear window-scoped snapshots.
            for stats in drivers.values():
                for key in ("observed", "captured", "rejected"):
                    stats[key] = 0
        verified = VERIFIED.search(line)
        if verified:
            counts["verified_" + verified.group(1).replace("/", "_")] += 1
        counts["c1_link_rejected"] += "Frame link mode C1 not supported" in line
        counts["crc_c1_rejected"] += "C1/A CRC rejected" in line
        counts["t1_symbols_rejected"] += "T1 3-of-6 symbol rejected" in line
        counts["handled"] += "Telegram handled by 1 handlers" in line
        counts["unhandled"] += "Telegram not handled by any handler" in line
        m = SHADOW.search(line)
        if not m:
            continue
        driver, details = m.groups()
        fields = dict(FIELD.findall(details))
        if driver not in ("mkradio4", "fhkvdataiii"):
            driver = "other"
        stats = drivers[driver]
        stats["observations"] += 1
        for key in COUNTERS:
            try:
                value = int(fields.get(key, "0"))
            except ValueError:
                continue
            # observed/captured/rejected are firmware-wide snapshots for
            # the current window, not per-driver or lifetime totals.
            if key in ("observed", "captured", "rejected"):
                stats[key] = max(stats[key], value)
            else:
                stats[key] += value
        disposition = fields.get("disposition", "")
        if disposition == "1":
            stats["matches"] += 1
        elif disposition == "2":
            stats["differences"] += 1
        else:
            stats["other_dispositions"] += 1
        if fields.get("provenance") == "not_proven":
            stats["unproven_observations"] += 1
        # A successful numeric snapshot comparison is NOT proof of
        # same-telegram writes. Track missing DV attribution separately.
        if int(fields.get("compared", "0")) > 0 and (
            fields.get("provenance") != "proven"
        ):
            stats["comparison_without_provenance"] += 1
    return counts, drivers

def format_summary(counts, drivers):
    out = [
        "LOCAL_XMQ_LOG_SUMMARY (aggregate only; no private meter data)",
        "radio: " + " ".join(f"{k}={counts[k]}" for k in
            ("verified_T1_A", "verified_C1_A", "crc_c1_rejected",
             "t1_symbols_rejected", "c1_link_rejected", "handled", "unhandled")),
    ]
    out.append("shadow_budget: exhausted={} renewed={}".format(
        counts["shadow_budget_exhausted"], counts["shadow_budget_renewed"]))
    if not drivers:
        out.append("shadow: no observed comparisons")
    for driver in sorted(drivers):
        stats = drivers[driver]
        out.append(
            f"shadow driver={driver} observations={stats['observations']} "
            f"matches={stats['matches']} differences={stats['differences']} "
            f"compared_fields={stats['compared']} mismatches={stats['mismatches']} "
            f"matched_numeric_dv={stats['matched_numeric_dv']} "
            f"unmatched_numeric_dv={stats['unmatched_numeric_dv']} "
            f"unproven_observations={stats['unproven_observations']}"
        )
    total_matches = sum(s["matches"] for s in drivers.values())
    total_differences = sum(s["differences"] for s in drivers.values())
    compared = sum(s["compared"] for s in drivers.values())
    calendar_refs = sum(s["calendar_refs"] for s in drivers.values())
    unproven = sum(s["comparison_without_provenance"] for s in drivers.values())
    out.append(
        f"live_evidence: matches={total_matches} differences={total_differences} "
        f"numeric_and_date_fields_compared={compared} "
        f"calendar_refs={calendar_refs} "
        f"comparisons_without_setter_provenance={unproven}"
    )
    if total_matches and calendar_refs == 0:
        out.append("GAP: no calendar references compared in this capture")
    if unproven:
        out.append("GAP: legacy setter-level attribution NOT established in live capture")
    out.append("VERDICT: snapshot match is NOT proof of per-telegram setter provenance or XMQ replacement readiness")
    return "\n".join(out)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=pathlib.Path, help="local ESPHome log file")
    args = parser.parse_args()
    with args.log.open(encoding="utf-8", errors="replace") as src:
        print(format_summary(*summarize(src)))

if __name__ == "__main__":
    main()
