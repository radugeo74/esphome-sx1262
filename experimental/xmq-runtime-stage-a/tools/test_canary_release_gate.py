#!/usr/bin/env python3
"""Synthetic-only regression tests for the offline canary release workflow."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from canary_release_gate import release_gate


def run_tests():
    with TemporaryDirectory(prefix="xmq-canary-release-gate-") as temp:
        folder = Path(temp)
        original = folder / "working.yaml"
        candidate = folder / "canary.yaml"
        fallback = folder / "fallback.bin"
        newbin = folder / "canary.bin"
        manifest = folder / "new-manifest.json"
        for file in (original, candidate, fallback, newbin):
            file.write_text("synthetic-file-only")
        events = []

        def validate(path, fixture):
            events.append(("validate", path.name))
            return {"file": path.name}

        def compare(a, b):
            events.append(("compare", a["file"], b["file"]))

        def images(a, b, out):
            events.append(("images", a.name, b.name, out.name))

        with patch("canary_release_gate.validate_and_read", side_effect=validate), \
             patch("canary_release_gate.check", side_effect=compare), \
             patch("canary_release_gate.verify", side_effect=images):
            release_gate(original, candidate, fallback, newbin, manifest,
                         resolved_fixtures=True)
        assert events == [
            ("validate", "working.yaml"),
            ("validate", "canary.yaml"),
            ("compare", "working.yaml", "canary.yaml"),
            ("images", "fallback.bin", "canary.bin", "new-manifest.json")
        ], events

        events.clear()
        with patch("canary_release_gate.validate_and_read", side_effect=validate), \
             patch("canary_release_gate.check", side_effect=ValueError("synthetic mismatch")), \
             patch("canary_release_gate.verify", side_effect=images):
            try:
                release_gate(original, candidate, fallback, newbin, manifest,
                             resolved_fixtures=True)
                raise AssertionError("changed config accepted")
            except ValueError as e:
                assert str(e) == "synthetic mismatch"
        assert not any(event[0] == "images" for event in events), events

        events.clear()
        with patch("canary_release_gate.validate_and_read", side_effect=validate):
            try:
                release_gate(original, original, fallback, newbin, manifest,
                             resolved_fixtures=True)
                raise AssertionError("identical reference/canary accepted")
            except ValueError:
                pass
            try:
                release_gate(original, candidate, fallback, fallback, manifest,
                             resolved_fixtures=True)
                raise AssertionError("identical fallback/candidate app path accepted")
            except ValueError:
                pass
        assert not events, "identical paths must fail before reading private YAML"
        assert not manifest.exists(), "synthetic tests must not emit a manifest"
        print("CANARY_RELEASE_GATE_OK: mandatory YAML parity before image approval, fail closed, no secrets or flashing")


if __name__ == "__main__":
    run_tests()
