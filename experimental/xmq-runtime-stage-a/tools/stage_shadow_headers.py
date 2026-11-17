#!/usr/bin/env python3
"""Build-only preparation: stage original XMQ headers next to the ESPHome probe.

ESPHome's native ESP-IDF build copies external component sources into its own
build tree. The native builder does not reliably honor external -I paths.
This script makes the component self-contained for the *lab compile only*
without duplicating or editing any original XMQ header in Git.
"""
from pathlib import Path
from shutil import copy2
import hashlib

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "components" / "xmq_shadow_probe"


def main() -> None:
    headers = sorted(ROOT.glob("xmq_*.h"))
    if not headers:
        raise RuntimeError("No original Stage A XMQ headers found")
    DEST.mkdir(parents=True, exist_ok=True)
    for source in headers:
        target = DEST / source.name
        original = source.read_bytes()
        if source.name == "xmq_reader.h":
            # ESPHome generates esphome.h with every component header,
            # including this one DIRECTLY. Protect the staged copy itself.
            # Keep parser code otherwise byte-for-byte identical.
            prefix = (
                "#ifdef error\n"
                "#pragma push_macro(\"error\")\n"
                "#undef error\n"
                "#define XMQ_READER_RESTORE_ERROR_MACRO\n"
                "#endif\n"
            )
            suffix = (
                "\n#ifdef XMQ_READER_RESTORE_ERROR_MACRO\n"
                "#pragma pop_macro(\"error\")\n"
                "#undef XMQ_READER_RESTORE_ERROR_MACRO\n"
                "#endif\n"
            )
            target.write_bytes(prefix.encode() + original + suffix.encode())
        else:
            copy2(source, target)
        checksum = hashlib.sha256(original).hexdigest()[:12]
        print(f"STAGED {source.name} source_sha256={checksum}")
    print(f"XMQ_STAGING_OK {len(headers)} headers; reader has build-only macro guard")


if __name__ == "__main__":
    main()
