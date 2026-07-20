from pathlib import Path

import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import codegen as cg


CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_DRIVERS = "drivers"


wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    file.stem.removeprefix("driver_")
    for file in Path(__file__).parent.glob("driver_*.cpp")
}


_registered_drivers = set()


validate_driver = cv.All(
    cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_"),
    lambda driver: _registered_drivers.add(driver) or driver,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_DRIVERS, default=set()): cv.All(
            lambda value: (
                AVAILABLE_DRIVERS
                if value == "all"
                else set(value)
                if isinstance(value, list)
                else value
            ),
            {validate_driver},
        ),
    }
)


def FILTER_SOURCE_FILES():
    """Return driver source files that must not be compiled."""
    return {
        f"driver_{name}.cpp"
        for name in AVAILABLE_DRIVERS - _registered_drivers
    }


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        sorted(_registered_drivers),
    )
    await cg.register_component(var, config)
