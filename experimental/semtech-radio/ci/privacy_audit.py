#!/usr/bin/env python3
"""Privacy scan of every remote branch HEAD. Never print sensitive matches."""
import re
import subprocess
import sys

def git(*args):
    return subprocess.check_output(["git", *args])

# Real identifiers observed in previous user captures, decimal and BCD forms.
# This script contains match patterns only: never log or reproduce the values.
DECIMAL = re.compile(rb"(?<![0-9])(?:24273[0-9]{3}|22413[0-9]{3}|22414032|736087[0-9]{2})(?![0-9])")
HEX_BYTES = re.compile(
    rb"(?:0x)?(?:32|67|15)\s*,\s*(?:0x)?(?:40|30|37)\s*,\s*"
    rb"(?:0x)?(?:41|27)\s*,\s*(?:0x)?(?:22|24)",
    re.IGNORECASE,
)
# Known raw meter identifiers embedded in unseparated radio telegram hex strings.
HEX_INLINE = re.compile(rb"(?:32404122|67302724|15374122)", re.IGNORECASE)
CAPTURE_WORDS = re.compile(
    rb"\b(?:actual capture|real capture|captured heltec|"
    rb"recorded from (?:the )?(?:known|real) meter|real_water)\b",
    re.IGNORECASE,
)
SUFFIXES = (".cpp", ".cc", ".c", ".h", ".hpp", ".py", ".yaml", ".yml",
            ".md", ".txt", ".json", ".xmq", ".sh", ".ini", ".toml")
IGNORE = {"experimental/semtech-radio/ci/privacy_audit.py",
          ".github/workflows/native-sx126x-adapter.yml"}

def main():
    git("fetch", "--no-tags", "origin", "+refs/heads/*:refs/remotes/origin/*")
    refs = git("for-each-ref", "--format=%(refname)", "refs/remotes/origin/").decode().splitlines()
    seen = set()
    findings = []
    scanned = 0
    for ref in refs:
        for record in git("ls-tree", "-r", "-z", ref).split(b"\0"):
            if not record:
                continue
            info, path_bytes = record.split(b"\t", 1)
            kind, blob_sha = info.split()[1:]
            if kind != b"blob":
                continue
            path = path_bytes.decode("utf-8", "replace")
            if not path.endswith(SUFFIXES) or path in IGNORE:
                continue
            sha = blob_sha.decode()
            scanned += 1
            if sha in seen:
                continue
            seen.add(sha)
            raw = git("cat-file", "blob", sha)
            reasons = []
            if DECIMAL.search(raw): reasons.append("known decimal ID family")
            if HEX_BYTES.search(raw) or HEX_INLINE.search(raw):
                reasons.append("known BCD hex pattern")
            if CAPTURE_WORDS.search(raw):
                reasons.append("captured-data marker (manual review)")
            if reasons:
                findings.append((path, ", ".join(reasons)))
    print(f"Audited {len(refs)} refs, {scanned} file references, {len(seen)} unique text blobs.")
    for path, reason in findings:
        print(f"REVIEW: {path} [{reason}]")
    if findings:
        print(f"FAIL: {len(findings)} suspicious blob versions. Output contains paths only.")
        return 1
    print("PASS: no matches to configured signatures in branch HEADs; Git history is not audited.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
