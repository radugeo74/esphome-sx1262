#!/usr/bin/env python3
"""Stage A: one shared application parser, no new live decoder admission.
Structural guard only; source XMQ golden C++ tests and real ESP-IDF probe
run separately in their existing jobs.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[3]
shared=root/"experimental/xmq/portable_cpp/apator162_ixml_fields.h"
compat=root/"experimental/semtech-radio/host_only/apator162_ixml_fields.h"
probe=root/"experimental/xmq/espidf_cpp_probe/main/main.cpp"
component=root/"components/wmbus_native_sx126x/__init__.py"
manifest=root/"experimental/xmq/tools/native_driver_specs.py"
assert shared.is_file() and compat.is_file() and probe.is_file()
shared_text=shared.read_text(encoding="utf-8")
legacy=compat.read_text(encoding="utf-8")
cpp=probe.read_text(encoding="utf-8")
names=manifest.read_text(encoding="utf-8")
assert "inline Status decode_clear_manufacturer(" in shared_text
assert '#include "../../xmq/portable_cpp/apator162_ixml_fields.h"' in legacy
assert "decode_clear_manufacturer" not in legacy
assert '#include "apator162_ixml_fields.h"' in cpp
assert 'apator162_reading.total_m3 - 7.512' in cpp
assert "BATCH2_PENDING = (\"minomess\", \"kamwater\", \"apator162\")" in names
assert "PORTABLE = (\"iperl\", \"hydrus\")" in names
assert "for header in (xmq / \"portable_cpp\").glob(\"*.h\")" in component.read_text(encoding="utf-8")
assert not (root/"components/wmbus_native_sx126x/apator162_ixml_fields.h").exists(), (
    "Shared module should be copied at build time only, not committed as a second source")
# Kamwater status/numeric algorithms now have exactly one native header each.
cmake=(root/"experimental/xmq/espidf_cpp_probe/main/CMakeLists.txt").read_text(encoding="utf-8")
assert '"../../../../components/wmbus_native_sx126x"' in cmake
for name, symbol in (
    ("kamwater_status_semantics.h", "inline Status decode("),
    ("kamwater_numeric_semantics.h", "inline Status decode("),
):
    target=root/"components/wmbus_native_sx126x"/name
    alias=root/"experimental/semtech-radio/host_only"/name
    target_text=target.read_text(encoding="utf-8")
    alias_text=alias.read_text(encoding="utf-8")
    assert symbol in target_text
    assert '#include "standard_dif_vif.h"' in target_text
    assert f'#include "../../../components/wmbus_native_sx126x/{name}"' in alias_text
    assert "inline Status decode(" not in alias_text
    assert f'#include "{name}"' in cpp
assert "kw_status.status != \"DRY\"" in cpp
assert 'field.name=="max_flow_last_day_m3h"' in cpp
assert 'field.name=="min_flow_temperature_last_day_c"' in cpp
# Stage A consolidation: both limited transport-aware host helpers now share
# one native C++ source; neither is wired to the live receiver dispatcher.
for name, function in (
    ("kamwater_tpl_status_semantics.h", "decode_clear_short_tpl("),
    ("minomess_wired_frame.h", "inline Status decode("),
):
    impl=root/"components/wmbus_native_sx126x"/name
    compat=root/"experimental/semtech-radio/host_only"/name
    impl_txt=impl.read_text(encoding="utf-8")
    compat_txt=compat.read_text(encoding="utf-8")
    assert function in impl_txt, name
    assert f'#include "../../../components/wmbus_native_sx126x/{name}"' in compat_txt
    assert function not in compat_txt, "duplicate algorithm in host alias"
    assert f'#include "{name}"' in cpp, "ESP-IDF probe missing shared code"
native_runtime=(root/"components/wmbus_native_sx126x/native_receiver_probe.cpp").read_text(encoding="utf-8")
native_catalog=(root/"components/wmbus_native_sx126x/xmq_decode_catalog.h").read_text(encoding="utf-8")
assert "kamwater::tpl_status::decode_clear_short_tpl" not in native_runtime
assert "minomess::wired::decode" not in native_runtime
assert '"kamwater"' not in native_catalog.split("enum class PureDecodeStatus")[1]
assert '"minomess"' not in native_catalog.split("enum class PureDecodeStatus")[1]
assert "kw_tpl.status!=\"DRY ERROR\"" in cpp
assert "wired_reading.gateway_rssi_dbm!=-86" in cpp
# Keyless ELL IV and counter have one shared source; OpenSSL stays host-only.
iv=root/"components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h"
iv_alias=root/"experimental/semtech-radio/host_only/kamwater_ell_ctr_iv.h"
openssl=root/"experimental/semtech-radio/host_only/kamwater_ell_ctr_openssl.h"
iv_text=iv.read_text(encoding="utf-8")
iv_alias_text=iv_alias.read_text(encoding="utf-8")
openssl_text=openssl.read_text(encoding="utf-8")
assert "inline Status derive_iv(" in iv_text
assert "inline void increment_counter(" in iv_text
assert '#include "../../../components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h"' in iv_alias_text
assert "inline Status derive_iv(" not in iv_alias_text
assert "inline void increment_counter(" not in openssl_text
assert '#include "kamwater_ell_ctr_iv.h"' in openssl_text
assert "#include <openssl/" not in iv_text
assert "kamwater_ell_ctr_openssl.h" not in native_runtime
# Final Stage A boundary: shared keyless primitives are not linked to live RF.
assert "kamwater::ell_ctr::derive_iv" not in native_runtime
assert "kamwater::ell_ctr::increment_counter" not in native_runtime
assert "kamwater::ell_ctr::decrypt_ell_ii" not in native_runtime
assert "kamwater::ell_ctr::decrypt_ell_ii" not in native_catalog
# The ESP-IDF build explicitly checks host/source sharing for application
# parsers. IV remains guarded by its original host golden reference tests;
# no end-to-end on-device encrypted-frame validation is claimed.
native_manifest=(root/"experimental/xmq/tools/native_driver_specs.py").read_text(encoding="utf-8")
assert 'BATCH2_PENDING = ("minomess", "kamwater", "apator162")' in native_manifest
assert 'GENERATED = (' in native_manifest
assert "kamwater_ell_ctr_iv.h" not in native_runtime

# Stage B crypto boundary: ESP-IDF mbedTLS probe can compile and link,
# but the production radio shall not import or select the probe implementation.
ccm_probe=root/"experimental/xmq/espidf_cpp_probe/main/oms_ccm_mbedtls_probe.h"
ccm_probe_cpp=(root/"experimental/xmq/espidf_cpp_probe/main/main.cpp").read_text(encoding="utf-8")
ccm_probe_build=(root/"experimental/xmq/espidf_cpp_probe/main/CMakeLists.txt").read_text(encoding="utf-8")
ccm_probe_code=ccm_probe.read_text(encoding="utf-8")
assert '#include "oms_ccm_mbedtls_probe.h"' in ccm_probe_cpp
assert "REQUIRES log esp_system heap mbedtls" in ccm_probe_build
assert "mbedtls_cipher_cmac(" in ccm_probe_code
assert "mbedtls_ccm_auth_decrypt(" in ccm_probe_code
assert "out={};" in ccm_probe_code
for native in (root/"components/wmbus_native_sx126x").rglob("*"):
    if native.is_file() and native.suffix in (".cpp",".cc",".h",".hpp",".py"):
        lines=native.read_text(encoding="utf-8",errors="replace").splitlines()
        assert not any(
            line.lstrip().startswith(("#include","from ","import ")) and
            "oms_ccm_mbedtls_probe" in line for line in lines
        ), f"probe-only CCM imported in active ESPHome: {native}"
print("PASS stage A: same Apator162, Kamwater status/numeric/TPL and wired Minomess algorithms for host and ESP-IDF, "
      "pending driver remains unavailable for live YAML and RF publication")
