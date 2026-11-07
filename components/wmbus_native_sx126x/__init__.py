from pathlib import Path
import shutil
import sys
"""Opt-in receive diagnostic for ESPHome's official SX126x component.

No meter decoding or sensor publication: first establish radio payload semantics.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome import automation
from esphome.components.sx126x import SX126x
from esphome.components import sensor, text_sensor

DEPENDENCIES = ["sx126x"]
AUTO_LOAD = ["sensor", "text_sensor"]
CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_SX126X_ID = "sx126x_id"
CONF_C1_FORMAT = "c1_format"
CONF_C1_PLAINTEXT_PUBLISH = "c1_plaintext_publish"
CONF_LINK_MODE = "link_mode"
CONF_CONFIGURED_METER_IDS = "configured_meter_ids"
CONF_FIFO_PROGRESS_PROBE = "fifo_progress_probe"
CONF_XMQ_SENSORS = "xmq_sensors"
CONF_XMQ_TEXT_SENSORS = "xmq_text_sensors"
CONF_RSSI_SENSORS = "rssi_sensors"
CONF_METER_ID = "meter_id"
CONF_FIELD = "field"
CONF_METER_DRIVERS = "meter_drivers"
CONF_SECURE_METERS = "secure_meters"
CONF_DLL_METER_ID = "dll_meter_id"
CONF_KEY = "key"
CONF_ON_VERIFIED = "on_verified"
CONF_T1_RAW_DIAGNOSTICS = "t1_raw_diagnostics"
CONF_DRIVER = "driver"

def validate_meter_id(value):
    if len(value) != 8 or not value.isascii() or not value.isdecimal():
        raise cv.Invalid("Meter ID must contain exactly 8 ASCII digits")
    return value

def validate_driver_name(value):
    # Batch-2 and Lansen may AUTO-decode only configured verified-clear RF.
    if value == "auto":
        return value
    root = Path(__file__).resolve().parents[2]
    tool_dir = root / "experimental" / "xmq" / "tools"
    sys.path.insert(0, str(tool_dir))
    try:
        from native_driver_specs import SUPPORTED, BATCH2_PENDING, MANUAL_PLAIN
        # Batch-2 remains outside generic AUTO manifest: native AUTO
        # selection checks DLL/TPL identity, security and strict model modes.
        if value not in SUPPORTED + BATCH2_PENDING + MANUAL_PLAIN:
            raise cv.Invalid(f"Native XMQ driver {value!r} is not available")
    finally:
        sys.path.pop(0)
    return value


def validate_secure_key(value):
    value = cv.string_strict(value)
    if len(value) != 32 or any(c not in "0123456789abcdefABCDEF" for c in value):
        raise cv.Invalid("secure_meters.key must be exactly 32 hexadecimal characters (AES-128)")
    return value.upper()


def validate_secure_config(config):
    secured = config[CONF_SECURE_METERS]
    plain = config[CONF_METER_DRIVERS]
    secure_ids = [entry[CONF_METER_ID] for entry in secured]
    secure_dll = [entry.get(CONF_DLL_METER_ID, entry[CONF_METER_ID])
                  for entry in secured]
    plain_ids = [entry[CONF_METER_ID] for entry in plain]
    plain_dll = [entry.get(CONF_DLL_METER_ID, entry[CONF_METER_ID])
                 for entry in plain]
    if len(secure_ids) != len(set(secure_ids)):
        raise cv.Invalid("secure_meters must not contain duplicate meter IDs")
    if len(secure_dll) != len(set(secure_dll)):
        raise cv.Invalid("secure_meters must have unique DLL sender IDs")
    if len(plain_ids) != len(set(plain_ids)) or len(plain_dll) != len(set(plain_dll)):
        raise cv.Invalid("meter_drivers must have unique meter and DLL sender IDs")
    for entry in secured + plain:
        if (entry.get(CONF_DLL_METER_ID, entry[CONF_METER_ID]) != entry[CONF_METER_ID]
                and entry[CONF_DRIVER] not in ("minomess", "auto")):
            raise cv.Invalid("Separate dll_meter_id is supported only by minomess or auto with validated Minomess TPL")
    # One sender cannot dispatch to two different meters, nor to both
    # encrypted and no-key decoding. Avoid any ambiguous cross-binding.
    if (set(plain_ids) | set(plain_dll)) & (set(secure_ids) | set(secure_dll)):
        raise cv.Invalid("An ID cannot be assigned to both meter_drivers and secure_meters")
    return config


native_ns = cg.esphome_ns.namespace("wmbus_native_sx126x")
NativeReceiverProbe = native_ns.class_("NativeReceiverProbe", cg.Component)
VerifiedFrameTrigger = native_ns.class_("VerifiedFrameTrigger", automation.Trigger.template(cg.uint32, cg.float_, cg.std_string, cg.uint32))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(NativeReceiverProbe),
    cv.Required(CONF_SX126X_ID): cv.use_id(SX126x),
    cv.Optional(CONF_C1_FORMAT, default="B"): cv.one_of("A", "B", upper=True),
    cv.Optional(CONF_C1_PLAINTEXT_PUBLISH, default=False): cv.boolean,
    cv.Optional(CONF_LINK_MODE, default="C1"): cv.one_of("T1", "C1", "T1_C1", upper=True),
    cv.Optional(CONF_FIFO_PROGRESS_PROBE, default=False): cv.boolean,
    cv.Optional(CONF_T1_RAW_DIAGNOSTICS, default=False): cv.boolean,
    # AUTO for configured clear batch-2 is gated by native DLL/TPL checks.
    cv.Optional(CONF_METER_DRIVERS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Optional(CONF_DLL_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_DRIVER): cv.All(cv.string_strict, validate_driver_name),
    })),
    # Explicit AES keys only: no AUTO key selection or guessing.
    # Keys belong in !secret, never in the repository or diagnostic logs.
    cv.Optional(CONF_SECURE_METERS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Optional(CONF_DLL_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_DRIVER): cv.one_of("minomess", "kamwater", "apator162", lower=True),
        cv.Required(CONF_KEY): validate_secure_key,
    })),
    cv.Optional(CONF_RSSI_SENSORS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required("sensor"): sensor.sensor_schema(unit_of_measurement="dBm", accuracy_decimals=1, device_class="signal_strength", state_class="measurement"),
    })),
    cv.Optional(CONF_XMQ_SENSORS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_FIELD): cv.string_strict,
        cv.Required("sensor"): sensor.sensor_schema(accuracy_decimals=2),
    })),
    cv.Optional(CONF_XMQ_TEXT_SENSORS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_FIELD): cv.one_of("status", "at_date", "target_datetime", "meter_datetime", "current_date", "previous_date"),
        cv.Required("sensor"): text_sensor.text_sensor_schema(),
    })),
    cv.Optional(CONF_CONFIGURED_METER_IDS, default=[]): cv.ensure_list(cv.All(cv.string_strict, validate_meter_id)),
    cv.Optional(CONF_ON_VERIFIED): automation.validate_automation(
        {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(VerifiedFrameTrigger)}
    ),
}).extend(cv.COMPONENT_SCHEMA)
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate_secure_config)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    radio = await cg.get_variable(config[CONF_SX126X_ID])
    cg.add(var.set_radio(radio))
    cg.add(var.set_c1_format_b(config[CONF_C1_FORMAT] == "B"))
    cg.add(var.set_c1_plaintext_publish(config[CONF_C1_PLAINTEXT_PUBLISH]))
    cg.add(var.set_t1_mode(config[CONF_LINK_MODE] == "T1"))
    cg.add(var.set_combined_mode(config[CONF_LINK_MODE] == "T1_C1"))
    cg.add(var.set_t1_raw_diagnostics(config[CONF_T1_RAW_DIAGNOSTICS]))
    cg.add(var.set_fifo_progress_probe(config[CONF_FIFO_PROGRESS_PROBE]))
    # Reuse the existing XMQ generator; no wmbus_radio / legacy radio.
    root = Path(__file__).resolve().parents[2]
    xmq = root / "experimental" / "xmq"
    sys.path.insert(0, str(xmq / "tools"))
    try:
        from native_driver_specs import validate, catalog_fragment, compiled_fragment
        from generate_ixml_cpp import generate
        generated = generate(validate(xmq))
        from generate_driver_registry import generate as generate_registry
        # Auto detection indexes EVERY locally available XMQ signature.
        # It is independent of the decoders explicitly assigned in YAML.
        registry_files = sorted((xmq / "drivers").glob("*.xmq"))
        if not registry_files:
            raise cv.Invalid("No XMQ driver metadata available")
        # Keep the generated include local to the native component. ESPHome
        # copies this directory into build/src independently of wmbus_radio.
        registry_header = generate_registry(registry_files)
        component_dir = Path(__file__).resolve().parent
        (component_dir / "xmq_auto_registry.h").write_text(
            registry_header, encoding="utf-8")
        (component_dir / "xmq_generated.h").write_text(generated, encoding="utf-8")
        (component_dir / "xmq_native_descriptors.h").write_text(
            catalog_fragment(), encoding="utf-8")
        (component_dir / "xmq_native_compiled.h").write_text(
            compiled_fragment(), encoding="utf-8")
        # ESPHome copies components into build/src; absolute source include paths
        # do not reliably survive the isolated ESP-IDF compilation stage.
        # Bundle all portable headers alongside the native component instead.
        for header in (xmq / "portable_cpp").glob("*.h"):
            shutil.copyfile(header, component_dir / header.name)
        cg.add_build_flag(f"-I{component_dir}")
        cg.add_define("WMBUS_NATIVE_XMQ_ENABLED")
        if config[CONF_SECURE_METERS]:
            # ESPHome's lean ESP-IDF build can omit CCM and CMAC from
            # libmbedcrypto even though the headers and source files exist.
            # Only explicit encrypted meters need these OMS algorithms.
            from esphome.components import esp32

            esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CCM_C", True)
            esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_CMAC_C", True)
            cg.add_define("WMBUS_NATIVE_BATCH2_SECURE_ENABLED")
    finally:
        sys.path.pop(0)
    # Secure bindings must pass through exactly the same DLL-meter
    # configured admission as the existing T1/C1 pipeline.
    configured_ids = set()
    for meter_id in list(config[CONF_CONFIGURED_METER_IDS]) + [
        item.get(CONF_DLL_METER_ID, item[CONF_METER_ID])
        for item in config[CONF_SECURE_METERS]
    ] + [
        item.get(CONF_DLL_METER_ID, item[CONF_METER_ID])
        for item in config[CONF_METER_DRIVERS]
    ]:
        if meter_id not in configured_ids:
            cg.add(var.add_configured_meter_id(meter_id))
            configured_ids.add(meter_id)
    for item in config[CONF_METER_DRIVERS]:
        cg.add(var.add_meter_driver(
            item[CONF_METER_ID], item[CONF_DRIVER],
            item.get(CONF_DLL_METER_ID, item[CONF_METER_ID])))
    for item in config[CONF_SECURE_METERS]:
        cg.add(var.add_secure_meter(
            item[CONF_METER_ID], item[CONF_DRIVER], item[CONF_KEY],
            item.get(CONF_DLL_METER_ID, item[CONF_METER_ID])))
    for item in config[CONF_RSSI_SENSORS]:
        obj = await sensor.new_sensor(item["sensor"])
        cg.add(var.add_rssi_sensor(item[CONF_METER_ID], obj))
    for item in config[CONF_XMQ_SENSORS]:
        obj = await sensor.new_sensor(item["sensor"])
        cg.add(var.add_xmq_sensor(item[CONF_METER_ID], item[CONF_FIELD], obj))
    for item in config[CONF_XMQ_TEXT_SENSORS]:
        obj = await text_sensor.new_text_sensor(item["sensor"])
        cg.add(var.add_xmq_text_sensor(item[CONF_METER_ID], item[CONF_FIELD], obj))
    await cg.register_component(var, config)
    for conf in config.get(CONF_ON_VERIFIED, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trig,
            [(cg.uint32, "meter_id"), (cg.float_, "rssi"),
             (cg.std_string, "telegram_hex"), (cg.uint32, "telegram_size")],
            conf,
        )
