"""Optional selected-meter XMQ shadow and opt-in independent test sensors."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
from ..wmbus_meter import Meter

CONF_ENABLED = "enabled"
CONF_TARGET_METER = "target_meter"
CONF_TEST_TARGET = "test_target_m3"
CONF_TEST_TOTAL = "test_total_m3"
CODEOWNERS = ["@radugeo74"]
DEPENDENCIES = ["sensor"]
xmq_ns = cg.esphome_ns.namespace("xmq_shadow_probe")
ShadowProbe = xmq_ns.class_("ShadowProbe", cg.Component)


def _validate_test_sensors(config):
    has_target = CONF_TEST_TARGET in config
    has_total = CONF_TEST_TOTAL in config
    if has_target != has_total:
        raise cv.Invalid("XMQ Stage 2 test sensors must be configured as a pair")
    if has_target and (not config[CONF_ENABLED] or CONF_TARGET_METER not in config):
        raise cv.Invalid("XMQ Stage 2 sensors require enabled shadow and selected meter")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ShadowProbe),
        cv.Optional(CONF_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_TARGET_METER): cv.use_id(Meter),
        cv.Optional(CONF_TEST_TARGET): sensor.sensor_schema(
            unit_of_measurement="m³",
            accuracy_decimals=3,
            state_class="total",
            icon="mdi:water",
        ),
        cv.Optional(CONF_TEST_TOTAL): sensor.sensor_schema(
            unit_of_measurement="m³",
            accuracy_decimals=3,
            state_class="total_increasing",
            icon="mdi:water",
        ),
    }).extend(cv.COMPONENT_SCHEMA),
    _validate_test_sensors,
)


async def to_code(config):
    # The two optional sensors are independent of legacy wmbus_meter sensors.
    # No test sensor is constructed unless BOTH explicitly exist in YAML.
    add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    if CONF_TARGET_METER in config:
        selected = await cg.get_variable(config[CONF_TARGET_METER])
        cg.add(var.set_target_meter(selected))
    if CONF_TEST_TARGET in config:
        target = await sensor.new_sensor(config[CONF_TEST_TARGET])
        total = await sensor.new_sensor(config[CONF_TEST_TOTAL])
        cg.add(var.set_stage2_sensors(target, total))
    await cg.register_component(var, config)
