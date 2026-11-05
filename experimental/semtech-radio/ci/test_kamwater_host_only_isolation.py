#!/usr/bin/env python3
"""Fail CI if experimental host-only AES/OpenSSL implementation leaks into ESPHome.
This is an isolation check, not a claim that runtime crypto is validated.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[3]
components = root / "components" / "wmbus_native_sx126x"
host_only = root / "experimental" / "semtech-radio" / "host_only"
assert components.is_dir(), "Missing ESPHome native component directory"
assert host_only.is_dir(), "Missing host-only test directory"

for filename in (
    "kamwater_ell_ctr_iv.h",
    "kamwater_ell_ctr_openssl.h",
    "kamwater_verified_dll_ingress.h",
    "oms_tpl_ccm_openssl.h",
    "kamwater_secure_ingress.h",
):
    assert (host_only / filename).is_file(), f"Host-only module missing: {filename}"
    assert not (components / filename).exists(), (
        f"Host-only module accidentally copied into ESPHome component: {filename}"
    )

for path in components.rglob("*"):
    if path.suffix not in (".h", ".hpp", ".cc", ".cpp", ".c", ".py"):
        continue
    content = path.read_text(encoding="utf-8", errors="replace")
    # Only block executable inclusions/references. Descriptive comments can
    # mention the host-only tests without changing the runtime dependency graph.
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith(("#include", "import ", "from ")):
            assert "kamwater_ell_ctr_openssl" not in stripped, (
                f"Host OpenSSL AES CTR import in ESPHome component: {path}"
            )
            assert "oms_tpl_ccm_openssl" not in stripped, (
                f"Host OpenSSL CCM import in ESPHome component: {path}"
            )
            assert "kamwater_secure_ingress" not in stripped, (
                f"Host secure ingress imported by ESPHome component: {path}"
            )
            assert "host_only/" not in stripped, (
                f"Host-only module included by ESPHome component: {path}"
            )
            assert "<openssl/" not in stripped, (
                f"OpenSSL header included by ESPHome component: {path}"
            )
print("PASS: Kamwater AES-CTR and DLL ingress remain host-only")
