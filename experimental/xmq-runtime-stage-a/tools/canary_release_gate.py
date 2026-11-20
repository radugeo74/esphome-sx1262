#!/usr/bin/env python3
"""Local-only, fail-closed SX1262 canary release gate. NEVER flashes.

Checks expanded private YAML parity before validating two ESP32-S3 OTA app
images and writing a private SHA-256 manifest. Does not print YAML, credentials,
meter IDs, AES keys or device addresses. Final human approval is mandatory.
"""
import argparse
from pathlib import Path
import sys

from preflight_canary import check, validate_and_read
from verify_rollback import GateError, verify


def release_gate(reference, candidate, fallback_bin, canary_bin, manifest,
                 *, resolved_fixtures=False):
    # Never read or expose raw config details in a log. Both YAML inputs must
    # be distinct: a canary must not silently replace its fallback source.
    if reference.resolve() == candidate.resolve():
        raise ValueError("reference and candidate YAML must be separate files")
    if fallback_bin.resolve() == canary_bin.resolve():
        raise ValueError("fallback and canary firmware must be separate files")
    original = validate_and_read(reference, resolved_fixtures)
    updated = validate_and_read(candidate, resolved_fixtures)
    check(original, updated)
    print("CANARY_CONFIG_PARITY_OK: confidential fields and operational topology unchanged")
    verify(fallback_bin, canary_bin, manifest)
    print("CANARY_RELEASE_MANIFEST_OK: private manifest written, no flashing performed")
    print("MANUAL_APPROVAL_REQUIRED: verify external source revisions, hardware, USB recovery, T1/C1 plan and backup provenance")


def main():
    parser = argparse.ArgumentParser(description="Offline SX1262 canary preparation; NEVER flashes")
    parser.add_argument("--reference", type=Path, required=True,
                        help="private known-good ESPHome YAML")
    parser.add_argument("--candidate", type=Path, required=True,
                        help="private canary YAML, shadow explicitly disabled")
    parser.add_argument("--fallback-bin", type=Path, required=True,
                        help="actual previously working application .bin")
    parser.add_argument("--canary-bin", type=Path, required=True,
                        help="locally compiled private canary application .bin")
    parser.add_argument("--manifest", type=Path, required=True,
                        help="new local manifest JSON path (must not exist)")
    parser.add_argument("--resolved-fixtures", action="store_true",
                        help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        release_gate(args.reference, args.candidate, args.fallback_bin,
                     args.canary_bin, args.manifest,
                     resolved_fixtures=args.resolved_fixtures)
    except (ValueError, GateError, OSError, UnicodeError, TimeoutError) as exc:
        # Error details from ESPHome, operating system, and YAML can contain
        # private paths/values. Print only an invariant error marker.
        print("CANARY_RELEASE_BLOCKED: preflight or offline rollback verification failed",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
