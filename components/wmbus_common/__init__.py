import esphome.config_validation as cv
from esphome.const import SOURCE_FILE_EXTENSIONS, CONF_ID
from esphome.loader import get_component, ComponentManifest
from esphome import codegen as cg
from pathlib import Path

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_DRIVERS = "drivers"
CONF_RADIO_ONLY = "radio_only"

wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    f.stem.removeprefix("driver_") for f in Path(__file__).parent.glob("driver_*.cc")
}

_registered_drivers = set()


def _driver_anchor(driver: str) -> str:
    """Return the linker anchor exported by a driver translation unit."""
    return "wmbus_driver_anchor_" + "".join(
        character if character.isalnum() else "_" for character in driver
    )


validate_driver = cv.All(
    cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_"),
    lambda driver: _registered_drivers.add(driver) or driver,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_RADIO_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_DRIVERS, default=set()): cv.All(
            lambda x: AVAILABLE_DRIVERS if x == "all" else x,
            # YAML sequences are lists; retain support for legacy sets.
            lambda x: list(x) if isinstance(x, (set, tuple)) else x,
            cv.ensure_list(validate_driver),
            lambda drivers: set(drivers),
        ),
    }
)


class WMBusComponentManifest(ComponentManifest):
    exclude_drivers: set[str]
    exclude_legacy_meters: bool

    @property
    def resources(self):
        exclude_files = {f"driver_{name}.cc" for name in self.exclude_drivers}
        if self.exclude_legacy_meters:
            exclude_files.add("meters.cc")
        SOURCE_FILE_EXTENSIONS.add(".cc")
        resources = [fr for fr in super(
        ).resources if fr.resource not in exclude_files]
        SOURCE_FILE_EXTENSIONS.discard(".cc")
        return resources


async def to_code(config):
    component = get_component("wmbus_common")
    component.__class__ = WMBusComponentManifest
    component.exclude_drivers = AVAILABLE_DRIVERS - _registered_drivers
    component.exclude_legacy_meters = config[CONF_RADIO_ONLY]
    if config[CONF_RADIO_ONLY] and _registered_drivers:
        raise cv.Invalid("radio_only excludes legacy meters; drivers must be empty")

    var = cg.new_Pvariable(config[CONF_ID], sorted(_registered_drivers))
    for driver in sorted(_registered_drivers):
        # Native ESP-IDF puts component sources in static archives.  Referencing
        # the per-driver anchor forces extraction of only the selected driver's
        # object, so its existing registerDriver() initializer is retained.
        cg.add_build_flag(f"-Wl,--undefined={_driver_anchor(driver)}")
    await cg.register_component(var, config)
