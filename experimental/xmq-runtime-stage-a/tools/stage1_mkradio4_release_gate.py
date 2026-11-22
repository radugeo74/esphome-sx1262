#!/usr/bin/env python3
"""Private local Stage-1 SX1262 XMQ mkradio4 canary gate; never flashes.

Only difference versus the running private configuration (apart from audited
external source declarations) is one read-only XMQ shadow observer with
enabled=true and target_meter=the ID of exactly one EXISTING mkradio4.
Do not copy a private resolved YAML, firmware or log into GitHub.
"""
import argparse
from contextlib import redirect_stdout
from copy import deepcopy
from io import StringIO
from pathlib import Path
import subprocess
import sys

import yaml

from canary_release_gate import (
    require_live_t1_c1_profile, require_pinned_canary_sources)
from preflight_canary import check, validate_and_read
from verify_rollback import GateError, verify


def verify_stage1_config(reference, candidate):
    require_live_t1_c1_profile(reference)
    require_live_t1_c1_profile(candidate)
    probe = candidate.get("xmq_shadow_probe")
    if not isinstance(probe, dict) or set(probe) != {
        "id", "enabled", "target_meter"
    } or probe.get("enabled") is not True:
        raise ValueError("Stage 1 requires exactly one explicitly enabled target probe")
    target = probe.get("target_meter")
    if not isinstance(target, str) or not target.strip():
        raise ValueError("Stage 1 must target an existing meter component ID")
    reference_meters = reference.get("wmbus_meter")
    selected = [m for m in reference_meters if m.get("id") == target]
    if len(selected) != 1 or selected[0].get("type") != "mkradio4":
        raise ValueError("Stage 1 target must be exactly one existing mkradio4")
    # Reuse the *existing* strict legacy parity checker without weakening it.
    # Copy only the local dictionary; never alter the user's private source.
    comparison_candidate = deepcopy(candidate)
    comparison_candidate["xmq_shadow_probe"] = {
        "id": probe["id"], "enabled": False
    }
    # Suppress routine preflight output that describes the temporary disabled
    # validation copy; no resolved private YAML is written to any file.
    with redirect_stdout(StringIO()):
        check(reference, comparison_candidate)
    require_pinned_canary_sources(candidate)
    return True


def release_stage1(reference, candidate, fallback_bin, canary_bin, manifest,
                   *, resolved_fixtures=False):
    if reference.resolve() == candidate.resolve():
        raise ValueError("Stage 1 reference and candidate must be separate")
    if fallback_bin.resolve() == canary_bin.resolve():
        raise ValueError("Stage 1 fallback and candidate binaries must differ")
    old = validate_and_read(reference, resolved_fixtures)
    new = validate_and_read(candidate, resolved_fixtures)
    verify_stage1_config(old, new)
    print("XMQ_STAGE1_CONFIG_OK: one selected legacy mkradio4; T1_C1 and all private fields unchanged")
    print("XMQ_STAGE1_SOURCE_OK: Git sources pinned; local sources require separate provenance review")
    verify(fallback_bin, canary_bin, manifest)
    print("XMQ_STAGE1_IMAGES_OK: private rollback manifest created; NO FLASH")
    print("MANUAL_APPROVAL_REQUIRED: known-good rollback image, USB recovery, and live HA continuity")


def main():
    p = argparse.ArgumentParser(
        description="Local-only one-mkradio4 XMQ Stage 1 release gate; NEVER flashes")
    for arg in ("reference", "candidate", "fallback-bin", "canary-bin", "manifest"):
        p.add_argument("--" + arg, type=Path, required=True)
    # Only tests may use already-resolved synthetic fixtures.
    p.add_argument("--resolved-fixtures", action="store_true",
                   help=argparse.SUPPRESS)
    a = p.parse_args()
    try:
        release_stage1(a.reference, a.candidate, a.fallback_bin, a.canary_bin,
                       a.manifest, resolved_fixtures=a.resolved_fixtures)
    except (ValueError, GateError, OSError, UnicodeError,
            subprocess.TimeoutExpired, yaml.YAMLError):
        # Under no circumstances echo private IDs, paths, passwords or keys.
        print("XMQ_STAGE1_RELEASE_BLOCKED: local parity, source, or rollback check failed",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
