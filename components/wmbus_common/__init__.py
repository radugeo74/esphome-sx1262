from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, SOURCE_FILE_EXTENSIONS


CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]

CONF_DRIVERS = "drivers"

# ESPHome include implicit fișierele .cpp, dar sursele WMBus sunt .cc.
# Înregistrarea trebuie făcută imediat la importarea componentei,
# înainte ca toolchain-ul ESP-IDF să descopere fișierele sursă.
SOURCE_FILE_EXTENSIONS.add(".cc")


wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    file.stem.removeprefix("driver_")
    for file in Path(__file__).parent.glob("driver_*.cc")
}

_registered_drivers = set()


def validate_driver(driver):
    _registered_drivers.add(driver)
    return driver


validate_driver = cv.All(
    cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_"),
    validate_driver,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_DRIVERS, default=set()): cv.All(
            lambda value: AVAILABLE_DRIVERS if value == "all" else value,
            {validate_driver},
        ),
    }
)


def FILTER_SOURCE_FILES():
    """Exclude drivers that are not used by the configuration."""
    unused_drivers = AVAILABLE_DRIVERS - _registered_drivers
    return {f"driver_{driver}.cc" for driver in unused_drivers}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], sorted(_registered_drivers))
    await cg.register_component(var, config)
