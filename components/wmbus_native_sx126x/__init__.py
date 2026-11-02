from pathlib import Path
import shutil
import sys
"""Opt-in receive diagnostic for ESPHome's official SX126x component.

No meter decoding or sensor publication: first establish radio payload semantics.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.sx126x import SX126x
from esphome.components import sensor

DEPENDENCIES = ["sx126x"]
AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_SX126X_ID = "sx126x_id"
CONF_C1_FORMAT = "c1_format"
CONF_LINK_MODE = "link_mode"
CONF_CONFIGURED_METER_IDS = "configured_meter_ids"
CONF_FIFO_PROGRESS_PROBE = "fifo_progress_probe"
CONF_XMQ_SENSORS = "xmq_sensors"
CONF_METER_ID = "meter_id"
CONF_FIELD = "field"
CONF_METER_DRIVERS = "meter_drivers"
CONF_DRIVER = "driver"

def validate_meter_id(value):
    if len(value) != 8 or not value.isascii() or not value.isdecimal():
        raise cv.Invalid("Meter ID must contain exactly 8 ASCII digits")
    return value


native_ns = cg.esphome_ns.namespace("wmbus_native_sx126x")
NativeReceiverProbe = native_ns.class_("NativeReceiverProbe", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(NativeReceiverProbe),
    cv.Required(CONF_SX126X_ID): cv.use_id(SX126x),
    cv.Optional(CONF_C1_FORMAT, default="B"): cv.one_of("A", "B", upper=True),
    cv.Optional(CONF_LINK_MODE, default="C1"): cv.one_of("T1", "C1", upper=True),
    cv.Optional(CONF_FIFO_PROGRESS_PROBE, default=False): cv.boolean,
    cv.Optional(CONF_METER_DRIVERS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_DRIVER): cv.one_of("fhkvdataiii", "mkradio4"),
    })),
    cv.Optional(CONF_XMQ_SENSORS, default=[]): cv.ensure_list(cv.Schema({
        cv.Required(CONF_METER_ID): cv.All(cv.string_strict, validate_meter_id),
        cv.Required(CONF_FIELD): cv.string_strict,
        cv.Required("sensor"): sensor.sensor_schema(accuracy_decimals=2),
    })),
    cv.Optional(CONF_CONFIGURED_METER_IDS, default=[]): cv.ensure_list(cv.All(cv.string_strict, validate_meter_id)),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    radio = await cg.get_variable(config[CONF_SX126X_ID])
    cg.add(var.set_radio(radio))
    cg.add(var.set_c1_format_b(config[CONF_C1_FORMAT] == "B"))
    cg.add(var.set_t1_mode(config[CONF_LINK_MODE] == "T1"))
    cg.add(var.set_fifo_progress_probe(config[CONF_FIFO_PROGRESS_PROBE]))
    # Reuse the existing XMQ generator; no wmbus_radio / legacy radio.
    root = Path(__file__).resolve().parents[2]
    xmq = root / "experimental" / "xmq"
    sys.path.insert(0, str(xmq / "tools"))
    try:
        from generate_ixml_cpp import generate
        generated = generate([xmq / "drivers" / "mkradio4.xmq",
                              xmq / "drivers" / "fhkvdataiii.xmq"])
        component_dir = Path(__file__).resolve().parent
        (component_dir / "xmq_generated.h").write_text(generated, encoding="utf-8")
        # ESPHome copies components into build/src; absolute source include paths
        # do not reliably survive the isolated ESP-IDF compilation stage.
        # Bundle all portable headers alongside the native component instead.
        for header in (xmq / "portable_cpp").glob("*.h"):
            shutil.copyfile(header, component_dir / header.name)
        cg.add_build_flag(f"-I{component_dir}")
        cg.add_define("WMBUS_NATIVE_XMQ_ENABLED")
    finally:
        sys.path.pop(0)
    for meter_id in config[CONF_CONFIGURED_METER_IDS]:
        cg.add(var.add_configured_meter_id(meter_id))
    for item in config[CONF_METER_DRIVERS]:
        cg.add(var.add_meter_driver(item[CONF_METER_ID], item[CONF_DRIVER]))
    for item in config[CONF_XMQ_SENSORS]:
        obj = await sensor.new_sensor(item["sensor"])
        cg.add(var.add_xmq_sensor(item[CONF_METER_ID], item[CONF_FIELD], obj))
    await cg.register_component(var, config)
