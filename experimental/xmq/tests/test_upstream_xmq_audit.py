#!/usr/bin/env python3
"""Offline regression of native source synchronisation safety boundaries."""
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental" / "xmq" / "tools"))
from audit_upstream_drivers import (  # noqa: E402
    blob_sha, categories, compare_sources, count_examples, markdown_report,
)


def run():
    all_names = categories()
    assert len(all_names) == 52
    assert len(set(name for name, _ in all_names)) == 52
    assert sum(cat == "XMQ_IXML_GENERATED" for _, cat in all_names) == 2
    assert sum(cat == "NATIVE_CPP_WITH_XMQ_REFERENCE" for _, cat in all_names) == 43
    assert sum(cat == "NATIVE_CONFIGURED_SPECIAL" for _, cat in all_names) == 3
    assert sum(cat == "NATIVE_AES_SPECIAL" for _, cat in all_names) == 4

    original = ROOT / "experimental" / "xmq" / "drivers"
    with TemporaryDirectory() as temporary:
        root = Path(temporary)
        local = root / "local"
        remote = root / "upstream"
        local.mkdir()
        remote.mkdir()
        selection = [
            ("fhkvdataiii", "XMQ_IXML_GENERATED"),
            ("munia", "NATIVE_CPP_WITH_XMQ_REFERENCE"),
        ]
        for name, _ in selection:
            src = (original / f"{name}.xmq").read_bytes()
            (local / f"{name}.xmq").write_bytes(src)
            (remote / f"{name}.xmq").write_bytes(src)
            assert blob_sha(src) != "0" * 40
        assert count_examples((local / "fhkvdataiii.xmq").read_bytes(),
                              "fhkvdataiii") >= 3
        identical = compare_sources(local, remote, selection)
        assert all(x["status"] == "IDENTICAL" for x in identical)
        old = (local / "munia.xmq").read_bytes()
        (remote / "munia.xmq").write_bytes(old + b"\n// test-only update\n")
        changes = compare_sources(local, remote, selection)
        assert changes[0]["status"] == "IDENTICAL"
        assert changes[1]["status"] == "UPSTREAM_CHANGED"
        assert changes[1]["local_examples"] == 2
        assert changes[1]["upstream_examples"] == 2
        assert (local / "munia.xmq").read_bytes() == old
        report = markdown_report(changes, "test-commit-sha")
        assert "munia" in report and "test-commit-sha" in report
        assert "not** automatically compatible" in report
        (remote / "munia.xmq").unlink()
        missing = compare_sources(local, remote, selection)
        assert missing[1]["status"] == "MISSING_UPSTREAM"

    print("PASS native 52-driver upstream XMQ audit; no auto-promotion")


if __name__ == "__main__":
    run()
