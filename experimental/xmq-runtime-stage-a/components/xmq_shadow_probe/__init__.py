"""Build-only opt-in XMQ shadow probe. No radio callback or HA sensors."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import add_idf_sdkconfig_option
from ..wmbus_meter import Meter

CONF_ENABLED = "enabled"
CONF_TARGET_METER = "target_meter"
CODEOWNERS = ["@radugeo74"]
xmq_ns = cg.esphome_ns.namespace("xmq_shadow_probe")
ShadowProbe = xmq_ns.class_("ShadowProbe", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ShadowProbe),
    cv.Optional(CONF_ENABLED, default=False): cv.boolean,
    cv.Optional(CONF_TARGET_METER): cv.use_id(Meter),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Full ESP-IDF/ESPHome build-only proof: real XMQ header chain and code
    # linked into one firmware. Never enable in a production YAML.
    # Generic headers are staged beside this component for build-only native ESP-IDF.
    add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    if CONF_TARGET_METER in config:
        selected = await cg.get_variable(config[CONF_TARGET_METER])
        cg.add(var.set_target_meter(selected))
    await cg.register_component(var, config)
