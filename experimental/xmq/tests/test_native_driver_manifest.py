#!/usr/bin/env python3
"""Native XMQ manifest regression; no personal telegrams or meter IDs."""
from pathlib import Path
import re
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental" / "xmq" / "tools"))
from native_driver_specs import SUPPORTED, CONFIGURED_NATIVE, CONFIGURABLE, GENERATED, PORTABLE, SECURE_ONLY, validate, catalog_fragment, compiled_fragment
from build_native_generated import build
from tempfile import TemporaryDirectory
from generate_ixml_cpp import generate
from generate_clear_profiles import generate as generate_clear_profiles, SPECS
from generate_standard_selectors import generate as generate_standard_selectors, SOURCES
from generate_pulse_fields import generate as generate_pulse_fields
from generate_xmq_full_profiles import generate as generate_xmq_full_profiles
from generate_vario451_ixml import generate as generate_vario451_ixml
from generate_supercal_lse08 import generate as generate_supercal_lse08

def main():
    assert SUPPORTED == tuple(d.name for d in GENERATED) + PORTABLE
    assert len(SUPPORTED) == len(set(SUPPORTED))
    assert CONFIGURED_NATIVE == ("minomess", "kamwater", "apator162")
    assert CONFIGURABLE == SUPPORTED + CONFIGURED_NATIVE
    assert validate(ROOT / "experimental" / "xmq")
    generated = build(ROOT)
    catalog = generated["xmq_native_descriptors.h"]
    compiled = generated["xmq_native_compiled.h"]
    assert catalog == catalog_fragment()
    assert compiled == compiled_fragment()
    for driver in SUPPORTED:
        assert f'std::strcmp(name, "{driver}") == 0' in compiled
    for driver in GENERATED:
        assert f'generated_ixml::{driver.name}::rules' in catalog
        assert f'namespace {driver.name} ' in generated["xmq_generated.h"]
    assert tuple(SPECS) == ("munia","cma12w","lansendw","lansenpu","supercom587","sontex868","maddalena")
    assert generated["xmq_generated_clear_profiles.h"] == generate_clear_profiles(
        ROOT / "experimental/xmq/drivers")
    assert SOURCES == ("iperl", "munia")
    standard_source = generate_standard_selectors(
        ROOT / "experimental/xmq/drivers")
    assert generated["xmq_generated_standard_selectors.h"] == standard_source
    assert '"total_m3",Kind::Volume,0x0010,0x0017' in standard_source
    assert '"max_flow_m3h",Kind::VolumeFlow,0x0038,0x003F' in standard_source
    assert '"current_temperature_c",Kind::TemperatureC,0x0064,0x0067' in standard_source
    assert '"current_relative_humidity_rh",Kind::RelativeHumidity,0x7B1A,0x7B1A' in standard_source
    # A compatible upstream field rename changes generated output names.
    # An unsupported quantity/VIF combination halts compilation safely.
    with TemporaryDirectory() as temp:
        scratch = Path(temp)
        for name in SOURCES:
            (scratch / f"{name}.xmq").write_text(
                (ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text())
        iperl_xmq = scratch / "iperl.xmq"
        original = iperl_xmq.read_text()
        assert "name     = max_flow" in original
        iperl_xmq.write_text(original.replace(
            "name     = max_flow", "name     = peak_flow", 1))
        revised = generate_standard_selectors(scratch)
        assert '"peak_flow_m3h",Kind::VolumeFlow' in revised
        iperl_xmq.write_text(original.replace(
            "vif_range        = VolumeFlow",
            "vif_range        = ExternalTemperature", 1))
        try:
            generate_standard_selectors(scratch)
        except ValueError:
            pass
        else:
            raise AssertionError("Unreviewed XMQ quantity/VIF change was accepted")
    pulse_source = generate_pulse_fields(
        ROOT / "experimental/xmq/drivers")
    assert generated["xmq_generated_pulse_fields.h"] == pulse_source
    assert '"a_counter",{0x0E,0xFD,0x3A,0x00},3,6' in pulse_source
    assert '"b_counter",{0x8E,0x40,0xFD,0x3A},4,6' in pulse_source
    assert '{0x4C,"PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE"}' in pulse_source
    with TemporaryDirectory() as temp:
        scratch=Path(temp)
        pulse=(ROOT / "experimental/xmq/drivers/lansenpu.xmq").read_text()
        (scratch / "lansenpu.xmq").write_text(pulse.replace(
            "value = 0x40", "value = 0x20", 1))
        try:
            generate_pulse_fields(scratch)
        except ValueError:
            pass
        else:
            raise AssertionError("Changed manufacturer status passed pulse XMQ review")
    source_compiled=generate_xmq_full_profiles(
        ROOT / "experimental/xmq/drivers")
    assert generated["xmq_generated_full_profiles.h"]==source_compiled
    assert "namespace cma12w {" in source_compiled
    assert "namespace lansendw {" in source_compiled
    assert "namespace iperl {" in source_compiled
    assert "namespace munia {" in source_compiled
    assert '"total_m3"' in source_compiled
    assert '"max_flow_m3h"' in source_compiled
    assert '"current_temperature_c"' in source_compiled
    assert '"current_relative_humidity_rh"' in source_compiled
    assert '"ERROR POWER_LOW"' in source_compiled
    assert '0x06,0x0000' in source_compiled
    # Both wrappers call the same generated application-profile interpreter.
    for name in ("iperl","munia"):
        header=(ROOT / "components/wmbus_native_sx126x" / f"{name}_plain.h").read_text()
        assert "full_xmq::decode(" in header
        assert f"xmq_compiled_profiles::{name}::profile" in header
    # Prevent new source record selectors, TPL meanings or fixtures from
    # being silently skipped by the XMQ compiler.
    for name,mutations in (
        ("iperl",(
            ("vif_range        = VolumeFlow","vif_range        = ExternalTemperature"),
            ('"total_m3":123.529','"total_m3":123.528'),
            ("mvt = SEN,68,07","mvt = SEN,69,07"),
            ('"status":"ERROR POWER_LOW"','"status":"OK"'),
        )),
        ("munia",(
            ("difvifkey = 02FD971D","difvifkey = 02FD971E"),
            ('"current_temperature_c":20.1','"current_temperature_c":20.2'),
            ("mvt = WEP,02,1b","mvt = WEP,03,1b"),
            ('"status":"OK"','"status":"UNKNOWN"'),
        )),
    ):
        original=(ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text()
        with TemporaryDirectory() as td:
            scratch=Path(td)
            for x in ("iperl","munia","cma12w","lansendw"):
                (scratch/f"{x}.xmq").write_text(
                    (ROOT / "experimental/xmq/drivers" / f"{x}.xmq").read_text())
            for before,after in mutations:
                assert before in original
                (scratch/f"{name}.xmq").write_text(original.replace(before,after,1))
                try:
                    generate_xmq_full_profiles(scratch)
                except ValueError:
                    pass
                else:
                    raise AssertionError(f"{name} accepted unsafe XMQ drift: {before}")
    assert '"current_temperature_c"' in source_compiled
    assert '"average_temperature_1h_c"' in source_compiled
    assert '"a_counter"' in source_compiled and '"b_counter"' in source_compiled
    assert '"ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_96"' in source_compiled
    assert '"OPEN"' in source_compiled and '"CLOSED"' in source_compiled
    # A changed upstream XMQ lookup that no longer matches original JSON
    # must not silently regenerate a broken native decoder.
    for name,mutations in (
        ("cma12w",(
            ("vif_range        = ExternalTemperature",
             "vif_range        = FlowTemperature"),
            ("map_type  = BitToString","map_type  = IndexToString"),
        )),
        ("lansendw",(
            ("value = 0x55","value = 0x54"),
            ("subunit_nr       = 1","subunit_nr       = 2"),
        )),
    ):
        original=(ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text()
        with TemporaryDirectory() as td:
            scratch=Path(td)
            for other in ("cma12w","lansendw"):
                (scratch/f"{other}.xmq").write_text(
                    (ROOT / "experimental/xmq/drivers" / f"{other}.xmq").read_text())
            for before,after in mutations:
                assert before in original
                (scratch/f"{name}.xmq").write_text(original.replace(before,after,1))
                try:
                    generate_xmq_full_profiles(scratch)
                except ValueError:
                    pass
                else:
                    raise AssertionError(f"{name} accepted unsafe XMQ drift: {before}")
    pair = generated["xmq_generated_supercal_lse08.h"]
    assert pair == generate_supercal_lse08(ROOT / "experimental/xmq/drivers")
    # Deliberately alter individual upstream fields/frames; the profile
    # compiler must reject changes instead of silently dropping a field.
    mutations = {
        "supercal": (
            ("mvt = SON,1b,04", "mvt = SON,1b,05"),
            ('"target_kwh": 0', '"target_kwh": 1'),
            ('"meter_datetime": "2023-03-04 18:54"',
             '"meter_datetime": "2023-03-04 18:55"'),
            ("820A6CE1F1", "820A6CE1F2"),
            ('"total_kwh": 296', '"total_kwh": 297'),
            ("storage_nr       = 20", "storage_nr       = 21"),
        ),
        "lse_08": (
            ("mvt = LSE,01,08", "mvt = LSE,01,09"),
            ('"duration_since_readout_h":2.489167',
             '"duration_since_readout_h":2.5'),
            ('"model_version":"01"', '"model_version":"02"'),
            ('"status":"TPL_STS_128"', '"status":"OK"'),
            ("02fdac7e0123", "02fdac7e0124"),
            ("add_combinable   = FutureValue",
             "add_combinable   = PastValue"),
        ),
    }
    with TemporaryDirectory() as tmp:
        driver_dir = Path(tmp)
        for name in mutations:
            source_xmq = (ROOT / "experimental/xmq/drivers" /
                          f"{name}.xmq").read_text()
            (driver_dir / f"{name}.xmq").write_text(source_xmq)
        for name, trials in mutations.items():
            original = (ROOT / "experimental/xmq/drivers" /
                        f"{name}.xmq").read_text()
            for before, after in trials:
                assert before in original, f"missing original {name}: {before}"
                (driver_dir / f"{name}.xmq").write_text(
                    original.replace(before, after, 1))
                try:
                    generate_supercal_lse08(driver_dir)
                except ValueError:
                    pass
                else:
                    raise AssertionError(
                        f"{name}: altered source was accepted: {before}")
            (driver_dir / f"{name}.xmq").write_text(original)

    assert "namespace supercal {" in pair
    assert "namespace lse_08 {" in pair
    assert "verified_length=69;" in pair
    assert "verified_length=42;" in pair
    for driver in ("supercal","lse_08"):
        decoder = (ROOT / "components/wmbus_native_sx126x" /
                   f"{driver}_plain.h").read_text()
        assert '#include "xmq_generated_supercal_lse08.h"' in decoder
        assert f"xmq_generated_pair::{driver}::rules" in decoder
    txt = (ROOT / "components/wmbus_native_sx126x/xmq_plain_text_fields.h").read_text()
    for field in ("target_date","meter_datetime","set_date",
                  "device_datetime","model_version"):
        assert f'"{field}"' in txt
    vario = generated["xmq_generated_vario451.h"]
    assert vario == generate_vario451_ixml(
        ROOT / "experimental/xmq/drivers/vario451.xmq")
    assert "previous_payload_offset = 3;" in vario
    assert "current_payload_offset = 7;" in vario
    assert "kwh_divisor = 3.6;" in vario
    vario_decoder = (ROOT / "components/wmbus_native_sx126x/vario451_plain.h").read_text()
    assert '#include "xmq_generated_vario451.h"' in vario_decoder
    assert "xmq_generated_vario451::previous_payload_offset" in vario_decoder
    assert "xmq_generated_vario451::current_payload_offset" in vario_decoder
    assert "xmq_generated_vario451::kwh_divisor" in vario_decoder
    for field in ("prev_raw_counter","curr_raw_counter","mfct_payload_hex"):
        assert field in vario_decoder
    vario_router = (ROOT / "components/wmbus_native_sx126x/xmq_decode_catalog.h").read_text()
    assert '{"prev_raw_counter",static_cast<double>(r.prev_raw_counter)}' in vario_router
    assert '{"curr_raw_counter",static_cast<double>(r.curr_raw_counter)}' in vario_router
    vario_text = (ROOT / "components/wmbus_native_sx126x/xmq_plain_text_fields.h").read_text()
    assert 'append(out,"mfct_payload_hex",reading.mfct_payload_hex);' in vario_text
    # Compiler must fail closed on changed IXML, MVT, scaling, DIF/VIF
    # or source fixture; this does not authorize additional RF profiles.
    source = (ROOT / "experimental/xmq/drivers/vario451.xmq").read_text()
    mutations = (
        ("hex, hex, hex, hex, @DV_PREV",
         "hex, hex, hex, hex, hex, @DV_PREV"),
        ("curr_raw_counter / 3.6 counter * 1 kwh",
         "curr_raw_counter / 3.5 counter * 1 kwh"),
        ("mvt = TCH,27,C3", "mvt = TCH,27,04"),
        ("difvifkey = 027E", "difvifkey = 027D"),
        ("129F25383300A86226", "129F25383300A86326"),
    )
    with TemporaryDirectory() as tmp:
        fake = Path(tmp) / "vario451.xmq"
        for original, replacement in mutations:
            assert original in source, original
            fake.write_text(source.replace(original, replacement, 1))
            try:
                generate_vario451_ixml(fake)
            except ValueError:
                pass
            else:
                raise AssertionError("Vario451 generator admitted changed XMQ: " + original)
    supercom_generated = generated["xmq_generated_clear_profiles.h"]
    assert "namespace supercom587 {" in supercom_generated
    assert "inline constexpr size_t verified_frame_bytes = 163;" in supercom_generated
    assert "15, // total_m3" in supercom_generated
    assert "123, // software_version" in supercom_generated
    assert "158, // status" in supercom_generated
    text_router = (ROOT / "components/wmbus_native_sx126x/xmq_plain_text_fields.h").read_text()
    assert 'append(out,"software_version",reading.software_version);' in text_router
    native_yaml = (ROOT / "components/wmbus_native_sx126x/__init__.py").read_text()
    assert '"software_version"' in native_yaml  # binding exposed as ESPHome text_sensor
    # Source-driven full-field regression: all original Supercom587 JSON
    # values are validated by the generator, not just the BCD total.
    official_supercom = (ROOT / "experimental/xmq/drivers/supercom587.xmq").read_text()
    mutations = (
        ('"total_m3":5.548', '"total_m3":5.549'),
        ('"software_version":"010002"', '"software_version":"010003"'),
        ('"status":"OK"', '"status":"LOW_BATTERY"'),
        ('"media":"warm water"', '"media":"water"'),
        ('"timestamp":"1111-11-11T11:11:11Z"',
         '"timestamp":"2026-11-10T00:00:00Z"'),
        ('"id":"12345678"', '"id":"12345679"'),
        ('"name":"MyWarmWater"', '"name":"DifferentName"'),
        ('"driver":"supercom587"', '"driver":"other"'),
        ('"software_version":"010002"', '"software_version":"010002","unknown_field":"x"'),
        ('use = software_version', 'use = different_software_version'),
        ('mvt = SON,3c,06', 'mvt = SON,3d,06'),
        ('0BFD0F020001', '0BFD0F0A0001'),
        ('02FD170000', '02FD170100'),
    )
    with TemporaryDirectory() as tmp:
        drivers = Path(tmp)
        for name in SPECS:
            (drivers / f"{name}.xmq").write_text(
                (ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text())
        modified = drivers / "supercom587.xmq"
        for original, changed in mutations:
            assert original in official_supercom, original
            modified.write_text(official_supercom.replace(original, changed, 1))
            try:
                generate_clear_profiles(drivers)
            except ValueError:
                pass
            else:
                raise AssertionError(
                    "Supercom587 generator accepted changed source: " + original)
    for driver, frame_len, fields in (
        ("sontex868",176,8),
        ("maddalena",79,6),
    ):
        assert f"namespace {driver} {{" in supercom_generated
        assert f"inline constexpr size_t verified_frame_bytes = {frame_len};" in supercom_generated
        assert f"xmq_generated_clear::{driver}::offsets" in (
            ROOT / "components/wmbus_native_sx126x" / f"{driver}_plain.h"
        ).read_text()
    txt = (ROOT / "components/wmbus_native_sx126x/xmq_plain_text_fields.h").read_text()
    for source_key in ("storage_48_date", "tariff_1_date",
                       "tariff_2_date", "compact_profile_hex",
                       "fabrication_no", "target_date", "meter_datetime",
                       "storage_2_date", "storage_3_datetime"):
        assert f'"{source_key}"' in txt
    for driver in SPECS:
        header = (ROOT / "components/wmbus_native_sx126x" / f"{driver}_plain.h").read_text()
        if driver not in ("cma12w","lansendw","munia"):
            assert '#include "xmq_generated_clear_profiles.h"' in header
            assert f"xmq_generated_clear::{driver}::models" in header
        if driver=="lansenpu":
            # Its record keys, names and statuses now compile directly
            # from XMQ via the dedicated source-backed pulse compiler.
            assert "xmq_generated_pulse::lansenpu::find_field" in header
            assert "inline constexpr KnownStatus statuses[]" in generated[
                "xmq_generated_pulse_fields.h"]
        elif driver in ("cma12w","lansendw","munia"):
            # Both use a single C++ interpreter of complete source-compiled
            # record formats, mappings, conversions and verified statuses.
            assert "xmq_compiled_profiles::"+driver+"::profile" in header
            assert "full_xmq::decode(" in header
        else:
            assert f"xmq_generated_clear::{driver}::rules" in header
    # Lansen DW: protect every upstream C! field and the Lansen
    # OPEN/CLOSED + unknown TPL/error-bit interpretation.
    dw_router = (ROOT / "components/wmbus_native_sx126x/xmq_decode_catalog.h").read_text()
    # a/b are now generated names, not C++ literals in the router.
    assert 'xmq_compiled_profiles::lansendw::rules[2].name' in dw_router
    assert 'xmq_compiled_profiles::lansendw::rules[3].name' in dw_router
    assert '"a_counter"' in source_compiled
    assert '"b_counter"' in source_compiled
    for field in (
        "digital_input_raw", "door_open",
        "error_flags_raw", "error_flags_unknown_raw", "tpl_status_raw",
        "tpl_status_unknown_raw", "status_alarm", "status_busy",
        "status_error", "status_permanent_error", "status_power_low",
        "status_temporary_error",
    ):
        assert f'{{"{field}",' in dw_router
    original_dw=(ROOT / "experimental/xmq/drivers/lansendw.xmq").read_text()
    dw_mutations=(
        ("mvt = LAS,07,1d", "mvt = LAS,08,1d"),
        ("value = 0x11", "value = 0x12"),
        ("value = 0x55", "value = 0x54"),
        ('"a_counter":22', '"a_counter":25'),
        ('"b_counter":0', '"b_counter":1'),
        ('"status":"CLOSED"', '"status":"OPEN"'),
        ('"error_flags":"ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_96"',
         '"error_flags":"OK"'),
        ("2f2f02fd1b5500", "2f2f02fd1b5600"),
        ("0efd3a220000000000", "0efd3a230000000000"),
        ("attributes = STATUS,INCLUDE_TPL_STATUS",
         "attributes = STATUS"),
    )
    with TemporaryDirectory() as tmp:
        source_dir=Path(tmp)
        for name in SPECS:
            (source_dir / f"{name}.xmq").write_text(
                (ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text())
        filename=source_dir / "lansendw.xmq"
        for old,new in dw_mutations:
            assert old in original_dw, old
            filename.write_text(original_dw.replace(old,new,1))
            try:
                generate_clear_profiles(source_dir)
            except ValueError:
                pass
            else:
                raise AssertionError(
                    "Lansendw changed original fixture/meaning accepted: "+old)
    assert "lansenpu" in SUPPORTED
    # The manufacturer-specific 0x40 bit is SABOTAGE_ENCLOSURE. Do not
    # silently lose any original JSON field, status meaning, or pulse value.
    pulse_router = (ROOT / "components/wmbus_native_sx126x/xmq_decode_catalog.h").read_text()
    for field in ("tpl_status_raw", "status_power_low",
                  "status_permanent_error", "status_sabotage_enclosure"):
        assert f'{{"{field}",' in pulse_router
    upstream_pulse = (ROOT / "experimental/xmq/drivers/lansenpu.xmq").read_text()
    sabotage_mutations = (
        ("value = 0x40", "value = 0x20"),
        ("mask_bits       = 0xe0", "mask_bits       = 0xc0"),
        ("attributes = STATUS,INCLUDE_TPL_STATUS", "attributes = STATUS"),
        ('"b_counter":870', '"b_counter":871'),
        ('"status":"PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE"',
         '"status":"POWER_LOW"'),
        ("mvt = LAS,0b,02", "mvt = LAS,0b,03"),
        ("8E40FD3A700800000000", "8E40FD3A700900000000"),
    )
    with TemporaryDirectory() as tmp:
        sample_dir=Path(tmp)
        for name in SPECS:
            (sample_dir / f"{name}.xmq").write_text(
                (ROOT / "experimental/xmq/drivers" / f"{name}.xmq").read_text())
        source_lansen=sample_dir / "lansenpu.xmq"
        for before,after in sabotage_mutations:
            assert before in upstream_pulse, before
            source_lansen.write_text(upstream_pulse.replace(before,after,1))
            try:
                generate_clear_profiles(sample_dir)
            except ValueError:
                pass
            else:
                raise AssertionError(
                    "Lansen Pulse accepted modified source: "+before)

    assert 'std::strcmp(name, "lansenpu")' in compiled
    for experimental in CONFIGURED_NATIVE:
        assert experimental in CONFIGURABLE
        assert experimental not in SUPPORTED
        assert f'std::strcmp(name, "{experimental}")' not in compiled
    assert SECURE_ONLY == ("mkradio4a", "topaseskr", "lansenth", "lansensm")
    assert "mkradio4a" not in CONFIGURABLE
    assert "topaseskr" not in CONFIGURABLE
    assert "lansenth" not in CONFIGURABLE and "lansensm" not in CONFIGURABLE
    assert "mkradio4a" not in compiled
    assert "topaseskr" not in compiled  # not generic plaintext/AUTO
    assert "lansenth" not in compiled and "lansensm" not in compiled
    assert set(CONFIGURABLE).isdisjoint(SECURE_ONLY)
    native_secure = (ROOT / "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h").read_text()
    assert 'std::strcmp(driver,"mkradio4a")==0' in native_secure
    assert "decode_mkradio4a_mode5(" in native_secure
    assert "decode_topaseskr_mode5(" in native_secure
    native_yaml = (ROOT / "components/wmbus_native_sx126x/__init__.py").read_text()
    assert 'cv.one_of("minomess", "kamwater", "apator162", "mkradio4a"' in native_yaml
    # A successfully compiled YAML must not silently lose secure_meters
    # bindings at runtime. Cross-check the EXACT YAML and C++ allowlists.
    cxx_bindings = (ROOT / "components/wmbus_native_sx126x/secure_meter_binding.h").read_text()
    receiver = (ROOT / "components/wmbus_native_sx126x/native_receiver_probe.h").read_text()
    yaml_match = re.search(
        r"cv.Required\(CONF_DRIVER\): cv.one_of\(([^\n]+)", native_yaml)
    cxx_match = re.search(
        r"inline bool supported_driver\([^)]*\) \{(.*?)\n\}",
        cxx_bindings, re.S)
    assert yaml_match and cxx_match
    yaml_drivers = set(re.findall(r'"([a-z0-9]+)"', yaml_match.group(1)))
    runtime_drivers = set(re.findall(r'driver == "([a-z0-9]+)"', cxx_match.group(1)))
    assert yaml_drivers == runtime_drivers == set(CONFIGURED_NATIVE + SECURE_ONLY), (
        yaml_drivers, runtime_drivers)
    assert "secure_bindings::add(this->secure_meters_,id,driver,key_hex,dll_id)" in receiver
    assert 'driver != "minomess" && driver != "mkradio4a"' in cxx_bindings
    assert 'entry[CONF_DRIVER] not in ("minomess", "mkradio4a")' in native_yaml
    native = (ROOT / "components" / "wmbus_native_sx126x" / "__init__.py").read_text()
    assert 'components" / "wmbus_radio' not in native
    assert "validate_driver_name" in native
    assert "from native_driver_specs import CONFIGURABLE" in native
    # A new driver containing unsupported numeric operators must fail
    # compilation rather than silently omit a measurement field.
    official = (ROOT / "experimental/xmq/drivers/mkradio4.xmq").read_text()
    assert "prev_raw_counter * 1m3" in official
    with TemporaryDirectory() as tmp:
        invalid = Path(tmp) / "mkradio4.xmq"
        invalid.write_text(official.replace(
            "prev_raw_counter * 1m3", "prev_raw_counter / 1m3"))
        try:
            generate([invalid])
        except ValueError as exc:
            assert "unsupported calculation" in str(exc)
        else:
            raise AssertionError("Unsupported XMQ arithmetic was silently accepted")
    print("PASS native driver manifest matches generated decode and AUTO catalog")

if __name__ == "__main__":
    main()
