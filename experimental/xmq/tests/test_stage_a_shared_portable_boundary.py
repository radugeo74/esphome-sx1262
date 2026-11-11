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
assert "CONFIGURED_NATIVE = (\"minomess\", \"kamwater\", \"apator162\")" in names
assert "PORTABLE = (" in names
assert all(f'"{driver}"' in names for driver in ("ultraheat","sontex868","supercal","q400","elvsense","lansenrp","qsmoke","qcaloric","rfmamb","sensostar","evo868","supercom587","hcae2","bfw240radio","qwaterv2","hydrodigit","itron","qualcosonic","lse_08","waterstarm","lse_07_17","munia","qheat_55_us","maddalena","itronheat","ehzp","qheatv2","c5isf","microclima","abbb23","ei6500","apatoreitn40","elf","engelmann_faw","apatoreitn","gransystems"))
# Explicit header allowlist is now required: a wildcard copy silently
# replaced xmq_decode_catalog.h with the offline legacy comparison catalog.
native_component_source = component.read_text(encoding="utf-8")
assert "for name in PORTABLE_RUNTIME_HEADERS:" in native_component_source
assert "if target.read_bytes() != source.read_bytes():" in native_component_source
assert 'for header in (xmq / "portable_cpp").glob("*.h")' not in native_component_source
# The build now stages this allowlisted portable helper in the native
# component source to satisfy ESP-IDF CMake and ESPHome external components.
# Its single canonical implementation remains portable_cpp: a native copy
# is valid ONLY when its bytes are identical (no forked algorithm).
staged=root/"components/wmbus_native_sx126x/apator162_ixml_fields.h"
assert staged.is_file(), "Native ESPHome source staging is missing"
assert staged.read_bytes()==shared.read_bytes(), (
    "Native staged Apator162 header differs from canonical portable source")
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
# Stage B now has explicit *clear-transport-only* application branches
# in the actual native decode catalog. Preserve the Stage A isolation:
# no decrypt-by-guessing and no production auto-registration.
catalog_branch=native_catalog.split("enum class PureDecodeStatus",1)[1]
for driver in ("kamwater","minomess","apator162"):
    assert f'std::strcmp(driver_name,"{driver}")==0' in catalog_branch, driver
assert "decode_clear_ell_ii(" in catalog_branch
clear_pipeline=(root/"components/wmbus_native_sx126x/kamwater_clear_pipeline.h").read_text(encoding="utf-8")
assert "decode_clear_ell_ii_tpl79(" in clear_pipeline
assert "ell::inspect_ell_ii(" in clear_pipeline
assert "kamwater::decode_clear_records(" in clear_pipeline
assert "Status::EllRejected" in clear_pipeline
assert "wmbus_pure::publication::inspect(" in catalog_branch
assert "minomess::decode_clear_application(" in catalog_branch
assert "apator162::decode_clear_manufacturer(" in catalog_branch
assert "kamwater::ell_ctr::decrypt" not in catalog_branch
assert "oms_ccm_mbedtls_probe" not in native_catalog
assert "oms_tpl_cbc" not in native_catalog
assert "CONFIGURED_NATIVE = (\"minomess\", \"kamwater\", \"apator162\")" in names
assert all(f'"{driver}"' in names for driver in ("ultraheat","sontex868","supercal","q400","elvsense","lansenrp","qsmoke","qcaloric","rfmamb","sensostar","evo868","supercom587","hcae2","bfw240radio","qwaterv2","hydrodigit","itron","qualcosonic","lse_08","waterstarm","lse_07_17","munia","qheat_55_us","maddalena","itronheat","ehzp","qheatv2","c5isf","microclima","abbb23","ei6500","apatoreitn40","elf","engelmann_faw","apatoreitn","gransystems"))
# Stage B explicitly binds all three encrypted decoders ONLY through an
# opt-in secure_meters list and a CRC-verified SX1262 callback. Do not
# rewrite the earlier, proven plaintext publication gate.
assert "minomess::wired::decode" not in native_runtime
assert "publish_secure_batch2_(" in native_runtime
assert "WMBUS_NATIVE_BATCH2_SECURE_ENABLED" in native_runtime
# Audit point 4: the production receiver and host end-to-end tests share
# the *same* configured secure decode and mapped-field emission helper.
# A source relocation must not be confused with crypto-path removal.
delivery=(root/"components/wmbus_native_sx126x/secure_meter_delivery.h").read_text(encoding="utf-8")
assert '#include "secure_meter_delivery.h"' in native_runtime
assert "secure_delivery::evaluate(" in native_runtime
assert "secure_delivery::emit_mapped(" in native_runtime
assert "batch2_secure::decode_configured(" in delivery
assert "out.outcome=Outcome::Rejected" in delivery
assert "if(result.outcome!=Outcome::Ready" in delivery
assert "self->publish_secure_batch2_(" in native_runtime
# No-key decoding is explicitly configured, never AUTO key trial.
assert "self->publish_plain_batch2_(" in native_runtime
assert '#include "xmq_batch2_plain_dispatch.h"' in native_runtime
plain_dispatch=(root/"components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h").read_text(encoding="utf-8")
assert "decode_configured_plain_frame(" in plain_dispatch
assert 'std::strcmp(name,"minomess")==0' in plain_dispatch
assert "is_explicit_driver(driver)" in plain_dispatch
assert "if(!frame || !meter_id || !dll_id" in plain_dispatch
assert "publication::inspect(" in native_runtime
assert 'secure_meters' in component.read_text(encoding="utf-8")
assert 'add_secure_meter(' in component.read_text(encoding="utf-8")
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
assert 'CONFIGURED_NATIVE = ("minomess", "kamwater", "apator162")' in native_manifest
assert 'GENERATED = (' in native_manifest
assert 'CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE' in native_manifest
assert "kamwater_ell_ctr_iv.h" not in native_runtime

# Stage B crypto boundary: ESP-IDF mbedTLS probe can compile and link,
# but the production radio shall not import or select the probe implementation.
ccm_probe=root/"experimental/xmq/espidf_cpp_probe/main/oms_ccm_mbedtls_probe.h"
ccm_probe_cpp=(root/"experimental/xmq/espidf_cpp_probe/main/main.cpp").read_text(encoding="utf-8")
ccm_probe_build=(root/"experimental/xmq/espidf_cpp_probe/main/CMakeLists.txt").read_text(encoding="utf-8")
ccm_probe_code=ccm_probe.read_text(encoding="utf-8")
shared_ccm=root/"components/wmbus_native_sx126x/oms_ccm_mbedtls.h"
shared_ccm_code=shared_ccm.read_text(encoding="utf-8")
assert '#include "oms_ccm_mbedtls_probe.h"' in ccm_probe_cpp
assert '#include "../../../../components/wmbus_native_sx126x/oms_ccm_mbedtls.h"' in ccm_probe_code
assert "REQUIRES log esp_system heap mbedtls" in ccm_probe_build
assert "mbedtls_cipher_cmac(" in shared_ccm_code
assert "mbedtls_ccm_auth_decrypt(" in shared_ccm_code
assert "out={};" in shared_ccm_code
assert "mbedtls_ccm_auth_decrypt(" not in ccm_probe_code
assert '#include "oms_ccm_mbedtls.h"' in (
    root/"components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
).read_text(encoding="utf-8")
for native in (root/"components/wmbus_native_sx126x").rglob("*"):
    if native.is_file() and native.suffix in (".cpp",".cc",".h",".hpp",".py"):
        lines=native.read_text(encoding="utf-8",errors="replace").splitlines()
        assert not any(
            line.lstrip().startswith(("#include","from ","import ")) and
            "oms_ccm_mbedtls_probe" in line for line in lines
        ), f"probe-only CCM imported in active ESPHome: {native}"
# Original probe crypto is now a compatibility include; the same
# implementation is compiled in the opt-in live receiver and ESP-IDF probe.
# Only a verified configured frame can reach the secure dispatcher.
# Stage B original public encrypted source fixture parity is isolated:
# CI compiles app_main but cannot claim it ran on a Heltec.
golden_header=root/"experimental/xmq/espidf_cpp_probe/main/kamwater_ccm_original_fixtures.h"
golden_text=golden_header.read_text(encoding="utf-8")
assert "valid_tpl[]" in golden_text
assert "invalid_tpl[]" in golden_text
assert "expected_clear[]" in golden_text
assert '#include "kamwater_ccm_original_fixtures.h"' in ccm_probe_cpp
assert "golden_expected" in ccm_probe_cpp
assert "golden_invalid" in ccm_probe_cpp
assert "wrong_ccm_id" in ccm_probe_cpp
assert "wrong_ccm_key" in ccm_probe_cpp
assert "tampered_ccm" in ccm_probe_cpp
assert "Status::BadTag" in ccm_probe_cpp
assert "Status::BadTag" in ccm_probe_cpp
print("PASS stage A: same Apator162, Kamwater status/numeric/TPL and wired Minomess algorithms for host and ESP-IDF, "
      "native drivers are configurable while live security/identity gates stay mandatory")
