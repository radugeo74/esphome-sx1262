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

# The keyless IV/counter algorithm was deliberately promoted to shared
# native C++ during Stage A. Its historical host path forwards to it.
# A native IV calculator is NOT a decryptor or an RF admission path.
iv_native = components / "kamwater_ell_ctr_iv.h"
iv_host = host_only / "kamwater_ell_ctr_iv.h"
assert iv_native.is_file() and iv_host.is_file()
iv_source = iv_native.read_text(encoding="utf-8")
iv_forwarder = iv_host.read_text(encoding="utf-8")
assert "inline Status derive_iv(" in iv_source
assert "inline void increment_counter(" in iv_source
assert '#include "../../../components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h"' in iv_forwarder
assert "inline Status derive_iv(" not in iv_forwarder
assert "inline void increment_counter(" not in iv_forwarder
assert "#include <openssl/" not in iv_source
assert "EVP_" not in iv_source

# Status mapping has one shared native implementation now. Its former
# host-only location must remain a forwarding compatibility include,
# never a second algorithm or OpenSSL dependency.
native_status = components / "kamwater_authenticated_tpl_status.h"
host_status = host_only / "kamwater_authenticated_tpl_status.h"
assert native_status.is_file() and host_status.is_file()
status_code = native_status.read_text(encoding="utf-8")
host_status_code = host_status.read_text(encoding="utf-8")
assert "decode_original_ccm_status(" in status_code
assert '#include "../../../components/wmbus_native_sx126x/kamwater_authenticated_tpl_status.h"' in host_status_code
assert "decode_original_ccm_status(" not in host_status_code
assert "#include <openssl/" not in status_code

for filename in (
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
print("PASS: native Kamwater IV and authenticated status shared; OpenSSL and host verified-DLL ingress isolated")
