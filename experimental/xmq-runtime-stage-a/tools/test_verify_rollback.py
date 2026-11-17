#!/usr/bin/env python3
"""Synthetic-only tests: never handles any user firmware or secrets."""
import contextlib
import io
import json
from pathlib import Path
import stat
import tempfile
from verify_rollback import GateError, verify, MAX_APP_IMAGE_SIZE

def fake(path, suffix: bytes, *, chip=9, magic=0xe9, segments=1):
    header=bytearray(24)
    header[0]=magic
    header[1]=segments
    header[12:14]=chip.to_bytes(2, "little")
    path.write_bytes(header + (b"\x00" * 256) + suffix)

def blocked(old, new, manifest, label):
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            verify(old, new, manifest)
    except GateError:
        assert not manifest.exists(), label
        return
    raise AssertionError("unsafe rollback accepted: "+label)

with tempfile.TemporaryDirectory() as tmp:
    base=Path(tmp)
    old=base/"fallback.bin"
    new=base/"candidate.bin"
    manifest=base/"gate.json"
    fake(old,b"old")
    fake(new,b"new")
    with contextlib.redirect_stdout(io.StringIO()) as out:
        d=verify(old,new,manifest)
    assert "ROLLBACK_GATE_OK" in out.getvalue()
    assert d["fallback"]["sha256"] != d["canary"]["sha256"]
    assert d["fallback"]["chip"] == "esp32s3"
    assert d["state"].endswith("NOT-authorized-to-flash")
    assert set(json.loads(manifest.read_text())) == {
        "format","state","fallback","canary","must_confirm_manually"
    }
    assert stat.S_IMODE(manifest.stat().st_mode)==0o600
    blocked(old,new,manifest,"manifest overwrite")

    manifest.unlink()
    fake(new,b"old")
    blocked(old,new,manifest,"same binary")
    fake(new,b"new")
    fake(new,b"wrong-chip",chip=0)
    blocked(old,new,manifest,"wrong chip")
    fake(new,b"new")
    fake(old,b"wrong magic",magic=0)
    blocked(old,new,manifest,"wrong signature")
    fake(old,b"old")
    fake(new,b"zero segments",segments=0)
    blocked(old,new,manifest,"zero segments")
    fake(new,b"new")
    fake(new,b"over segments",segments=17)
    blocked(old,new,manifest,"excess segments")
    fake(new,b"new")
    new.write_bytes(b"too short")
    blocked(old,new,manifest,"short image")
    fake(new,b"new")
    new.write_bytes(b"\xE9"+b"\x00"*(MAX_APP_IMAGE_SIZE))
    blocked(old,new,manifest,"oversized image")
    fake(new,b"new")
    new.unlink()
    new.symlink_to(old)
    blocked(old,new,manifest,"symlink image")
    new.unlink()
    fake(new,b"new")
    manifest.symlink_to(old)
    blocked(old,new,manifest,"symlink manifest")
print("ROLLBACK_GATE_TEST_OK: 10 synthetic cases, SHA, ESP32-S3, size, no overwrite, 0600")
