"""T1 proprietary XMQ primary meters; no wmbus_meter or shadow probe."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.wmbus_radio import RadioComponent
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import CONF_ID

DEPENDENCIES = ["wmbus_radio"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True
CODEOWNERS = ["@radugeo74"]
ns = cg.esphome_ns.namespace("xmq_meter")
Meter = ns.class_("Meter", cg.Component)
FIELDS = ("current_hca", "previous_hca", "temp_room_c", "temp_radiator_c",
          "target_m3", "total_m3", "rssi_dbm")
CONF_RADIO_ID = "radio_id"
CONF_METER_ID = "meter_id"
CONF_FAMILY = "family"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Meter),
    cv.Required(CONF_RADIO_ID): cv.use_id(RadioComponent),
    cv.Required(CONF_METER_ID): cv.hex_int,
    cv.Required(CONF_FAMILY): cv.one_of("heat", "water", lower=True),
    **{cv.Optional(name): sensor.sensor_schema(accuracy_decimals=1)
       for name in FIELDS},
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    cg.add_build_flag("-DXMQ_STANDALONE_ONLY=1")
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_meter(config[CONF_METER_ID], config[CONF_FAMILY] == "heat"))
    radio = await cg.get_variable(config[CONF_RADIO_ID])
    cg.add(var.set_radio(radio))
    for name in FIELDS:
        if name in config:
            s = await sensor.new_sensor(config[name])
            cg.add(var.add_sensor(name, s))
    await cg.register_component(var, config)
