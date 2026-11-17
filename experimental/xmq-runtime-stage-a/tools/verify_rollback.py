#!/usr/bin/env python3
"""Offline release gate. NEVER flashes or reads a device.

Requires an already-saved, known-good OTA application .bin AND an independently
compiled private canary .bin. Checks basic ESP32-S3 app-image shape, app
partition budget, non-identity and writes a local digest manifest with mode
0600. Does not prove that the rollback image is actually tested on hardware.

Do not use bootloader.bin, partitions.bin, merged images, CI dummy builds, or
encrypted images as either input. Input files and manifest remain local.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import tempfile

ESP_IMAGE_MAGIC = 0xE9
ESP32_S3_CHIP_ID = 9
# Conservative limit from the current tested Heltec ESPHome 8 MB layout.
# Any different real partition layout must be checked separately.
MAX_APP_IMAGE_SIZE = 0x3C0000
MAX_SEGMENTS = 16
MAX_FILE = 16 * 1024 * 1024

class GateError(Exception):
    pass

def image_info(path: Path):
    if not path.is_file() or path.is_symlink():
        raise GateError("missing or unsafe firmware file")
    size = path.stat().st_size
    if size < 256 or size > MAX_APP_IMAGE_SIZE or size > MAX_FILE:
        raise GateError("firmware image outside allowed application size")
    h = hashlib.sha256()
    with path.open("rb") as fp:
        head = fp.read(24)
        if len(head) != 24 or head[0] != ESP_IMAGE_MAGIC:
            raise GateError("invalid ESP32 application image signature")
        segments = head[1]
        if not 1 <= segments <= MAX_SEGMENTS:
            raise GateError("invalid ESP image segment count")
        # IDF image header stores chip_id little-endian at byte offset 12.
        chip_id = int.from_bytes(head[12:14], "little")
        if chip_id != ESP32_S3_CHIP_ID:
            raise GateError("firmware image target is not ESP32-S3")
        h.update(head)
        while chunk := fp.read(65536):
            h.update(chunk)
    return {"sha256": h.hexdigest(), "bytes": size, "chip": "esp32s3"}

def verify(reference, candidate, output):
    old = image_info(reference)
    new = image_info(candidate)
    if old["sha256"] == new["sha256"]:
        raise GateError("candidate and fallback images are identical")
    if output.is_symlink():
        raise GateError("manifest destination is a symlink")
    if output.exists():
        raise GateError("manifest already exists; refuse overwrite")
    if not output.parent.is_dir():
        raise GateError("manifest directory does not exist")
    manifest = {
        "format": "sx1262-canary-offline-release-gate-v1",
        "state": "build-artifacts-verified-only-NOT-authorized-to-flash",
        "fallback": old,
        "canary": new,
        "must_confirm_manually": [
            "fallback binary is the actual last known-good firmware",
            "both binaries correspond to the exact Heltec hardware and partitions",
            "private YAML parity preflight succeeded",
            "real Wi-Fi, API, OTA, meter IDs and AES secrets preserved",
            "external-component sources manually reviewed",
            "recovery by USB and physical access tested",
            "SX1262 T1/C1 receive behavior validated before replacing stable setup",
        ],
    }
    blob = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    # Atomic exclusive creation, private permission. No overwrite/symlinks.
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    fd = os.open(output, flags, 0o600)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(blob)
    except BaseException:
        output.unlink(missing_ok=True)
        raise
    print("ROLLBACK_GATE_OK: two different ESP32-S3 images within declared partition limit")
    print("MANIFEST_CREATED: contains hashes and sizes only, no secrets")
    print("NO_FLASH: rollback provenance and device recovery remain manual gates")
    return manifest

def main():
    ap = argparse.ArgumentParser(description="Offline SX1262 canary/rollback image gate")
    ap.add_argument("--fallback", type=Path, required=True)
    ap.add_argument("--candidate", type=Path, required=True)
    ap.add_argument("--manifest", type=Path, required=True)
    args = ap.parse_args()
    try:
        verify(args.fallback, args.candidate, args.manifest)
    except (GateError, OSError) as exc:
        # Fixed error strings only; never reveal private file paths.
        safe = str(exc) if isinstance(exc, GateError) else "local file operation failed"
        print("ROLLBACK_GATE_BLOCKED: " + safe, file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
