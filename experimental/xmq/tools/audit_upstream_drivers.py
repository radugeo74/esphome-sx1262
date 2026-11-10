#!/usr/bin/env python3
"""Audit all native wM-Bus XMQ source snapshots against official wmbusmeters.

Offline, deterministic and read-only by default. The user/CI must first
check out the upstream wmbusmeters drivers/src tree at a known commit.

Changing an XMQ file is NOT equivalent to supporting its new semantics in
an ESP32 native decoder. Stage changed files separately and require full CI,
fixture parity, security review and a conscious firmware build before use.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

from native_driver_specs import GENERATED, PORTABLE, CONFIGURED_NATIVE, SECURE_ONLY
from portable_xmq import blocks, parse, validate_driver, values


def blob_sha(data: bytes) -> str:
    """Git blob identity, matching git/trees API object SHAs."""
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def categories():
    groups = (
        ("XMQ_IXML_GENERATED", tuple(d.name for d in GENERATED)),
        ("NATIVE_CPP_WITH_XMQ_REFERENCE", PORTABLE),
        ("NATIVE_CONFIGURED_SPECIAL", CONFIGURED_NATIVE),
        ("NATIVE_AES_SPECIAL", SECURE_ONLY),
    )
    indexed = [(name, category) for category, group in groups for name in group]
    if len(indexed) != len(set(name for name, _ in indexed)):
        raise ValueError("Duplicate native driver in manifest")
    return indexed


def count_examples(data: bytes, name: str) -> int:
    """Validate upstream XMQ structure, including name and test inventory."""
    parsed = validate_driver(parse(data.decode("utf-8")))
    if values(parsed, "name") != [name]:
        raise ValueError(f"{name}: upstream XMQ identifies a different driver")
    suites = blocks(parsed, "tests")
    return sum(len(blocks(s["entries"], "test")) for s in suites)


def compare_sources(local_dir: Path, upstream_dir: Path, selected=None):
    """No writes. Changed source != compatible firmware. Never infer it."""
    catalog = categories() if selected is None else selected
    results = []
    for name, category in catalog:
        local_path = local_dir / f"{name}.xmq"
        remote_path = upstream_dir / f"{name}.xmq"
        item = {"driver": name, "native_implementation": category}
        if not local_path.is_file():
            item["status"] = "MISSING_LOCAL"
        elif not remote_path.is_file():
            item["status"] = "MISSING_UPSTREAM"
        else:
            local_data = local_path.read_bytes()
            upstream_data = remote_path.read_bytes()
            item["local_blob_sha"] = blob_sha(local_data)
            item["upstream_blob_sha"] = blob_sha(upstream_data)
            item["status"] = (
                "IDENTICAL" if local_data == upstream_data else "UPSTREAM_CHANGED"
            )
            if item["status"] != "IDENTICAL":
                item["local_examples"] = count_examples(local_data, name)
                item["upstream_examples"] = count_examples(upstream_data, name)
        results.append(item)
    return results


def markdown_report(records, revision: str) -> str:
    same = sum(r["status"] == "IDENTICAL" for r in records)
    changed = [r for r in records if r["status"] != "IDENTICAL"]
    lines = [
        "# Native XMQ source compatibility audit",
        "",
        f"Upstream revision: `{revision}`",
        f"Identical source files: **{same}/{len(records)}**",
        f"Changed or missing: **{len(changed)}**",
        "",
        "A changed source is **not** automatically compatible with native ESPHome C++.",
        "Source updates must be staged, rebuilt, regression-tested and approved.",
        "",
    ]
    if changed:
        lines.extend([
            "| Driver | Native implementation | Source status | Fixture count |",
            "| --- | --- | --- | --- |",
        ])
        for item in changed:
            counts = ("{local_examples} → {upstream_examples}".format(**item)
                      if "upstream_examples" in item else "n/a")
            lines.append(
                f"| `{item['driver']}` | {item['native_implementation']} | "
                f"{item['status']} | {counts} |"
            )
    else:
        lines.append("All 52 pinned XMQ sources match the chosen upstream revision.")
    return "\n".join(lines) + "\n"


def run(args):
    project = Path(__file__).resolve().parents[3]
    local = project / "experimental" / "xmq" / "drivers"
    upstream = args.upstream_dir
    if not upstream.is_dir():
        raise ValueError(f"Missing upstream drivers/src checkout: {upstream}")
    records = compare_sources(local, upstream)
    changed = [r for r in records if r["status"] != "IDENTICAL"]
    payload = {
        "upstream_revision": args.upstream_revision,
        "total": len(records),
        "identical": len(records) - len(changed),
        "changed": len(changed),
        "can_update_firmware_without_review": False,
        "drivers": records,
    }
    if args.stage_dir:
        stage = args.stage_dir.resolve()
        if stage == local.resolve() or local.resolve() in stage.parents:
            raise ValueError("Cannot stage upstream files inside live XMQ directory")
        stage.mkdir(parents=True, exist_ok=True)
        for item in changed:
            if item["status"] != "UPSTREAM_CHANGED":
                continue
            name = item["driver"]
            shutil.copyfile(upstream / f"{name}.xmq", stage / f"{name}.xmq")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2) + "\n")
    report = markdown_report(records, args.upstream_revision)
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(report, encoding="utf-8")
    print(report, end="")
    if args.fail_on_drift and changed:
        return 2
    return 0


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--upstream-dir", type=Path, required=True,
                   help="A separately cloned wmbusmeters/drivers/src directory")
    p.add_argument("--upstream-revision", default="unidentified",
                   help="Source commit SHA for reproducible audits")
    p.add_argument("--json", type=Path)
    p.add_argument("--markdown", type=Path)
    p.add_argument("--stage-dir", type=Path,
                   help="Copy changed upstream XMQ into review staging ONLY")
    p.add_argument("--fail-on-drift", action="store_true",
                   help="Fail a monitoring job, not a firmware build")
    raise SystemExit(run(p.parse_args()))
