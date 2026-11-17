"""Build-only opt-in XMQ shadow probe. No radio callback or HA sensors."""
from pathlib import Path
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import add_idf_sdkconfig_option

CONF_ENABLED = "enabled"
CODEOWNERS = ["@radugeo74"]
xmq_ns = cg.esphome_ns.namespace("xmq_shadow_probe")
ShadowProbe = xmq_ns.class_("ShadowProbe", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ShadowProbe),
    cv.Optional(CONF_ENABLED, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Full ESP-IDF/ESPHome build-only proof: real XMQ header chain and code
    # linked into one firmware. Never enable in a production YAML.
    cg.add_build_flag("-I" + str(Path(__file__).resolve().parents[2]))
    add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    await cg.register_component(var, config)
