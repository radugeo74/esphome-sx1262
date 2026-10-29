from pathlib import Path
import shutil
import sys
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_KEY,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
    CONF_MODE,
)
from esphome import automation

from ..wmbus_radio import RadioComponent
from ..wmbus_common import validate_driver

CONF_METER_ID = "meter_id"
CONF_RADIO_ID = "radio_id"
CONF_ON_TELEGRAM = "on_telegram"
CONF_XMQ_SHADOW = "xmq_shadow"

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]

DEPENDENCIES = ["wmbus_radio"]
AUTO_LOAD = ["sensor", "text_sensor"]

MULTI_CONF = True


wmbus_meter_ns = cg.esphome_ns.namespace("wmbus_meter")
link_mode_enum = cg.global_ns.enum("LinkMode", is_class=True)
Meter = wmbus_meter_ns.class_("Meter", cg.Component)
MeterRef = Meter.operator("ref")
TelegramTrigger = wmbus_meter_ns.class_(
    "TelegramTrigger",
    automation.Trigger.template(MeterRef),
)


def hex_key_validator(key):
    try:
        key = cv.bind_key(key)
        return key
    except cv.Invalid as e:
        raise cv.Invalid(e.msg.replace("Bind key", "Key"))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Meter),
        cv.GenerateID(CONF_RADIO_ID): cv.use_id(RadioComponent),
        cv.Optional(CONF_METER_ID, default=""): cv.hex_int,
        cv.Optional(CONF_TYPE, default="auto"): validate_driver,
        cv.Optional(CONF_XMQ_SHADOW, default=False): cv.boolean,
        cv.Optional(CONF_KEY): cv.Any(
            cv.All(cv.string_strict, lambda s: s.encode().hex(),
                   hex_key_validator),
            hex_key_validator,
        ),
        cv.Optional(CONF_ON_TELEGRAM): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TelegramTrigger)},
        ),
        cv.Optional(CONF_MODE, default="Any"): cv.ensure_list(
            cv.enum(
                {name: getattr(link_mode_enum, name)
                 for name in ("Any", "C1", "T1")}
            )
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    meter = cg.new_Pvariable(config[CONF_ID])
    cg.add(
        meter.set_meter_params(
            str('{:08x}'.format(config[CONF_METER_ID])),
            config[CONF_TYPE],
            config.get(CONF_KEY, ""),
            config[CONF_MODE],
        )
    )

    if config[CONF_XMQ_SHADOW]:
        cg.add_define("WMBUS_XMQ_SHADOW_ENABLED")
        # Keep XMQ generation explicitly opt-in; the normal component remains
        # independent of the experimental grammar and generated headers.
        root = Path(__file__).resolve().parents[2]
        xmq = root / "experimental" / "xmq"
        sys.path.insert(0, str(xmq / "tools"))
        try:
            from generate_ixml_cpp import generate
            output = xmq / "generated" / "generated_ixml.h"
            output.parent.mkdir(parents=True, exist_ok=True)
            generated = generate([
                xmq / "drivers" / "mkradio4.xmq",
                xmq / "drivers" / "fhkvdataiii.xmq",
            ])
            # ESPHome copies external component sources into its build directory.
            # Keeping the tables under experimental/ alone makes __has_include
            # silently skip XMQ in the compiled firmware.
            component_dir = Path(__file__).resolve().parent
            portable_dir = component_dir / "xmq_portable"
            portable_dir.mkdir(parents=True, exist_ok=True)
            for header in (xmq / "portable_cpp").glob("*.h"):
                shutil.copyfile(header, portable_dir / header.name)
            generated = generated.replace('#include "ixml_subset.h"', '#include "xmq_portable/ixml_subset.h"')
            generated = generated.replace('#include "calculate_subset.h"', '#include "xmq_portable/calculate_subset.h"')
            generated = generated.replace('#include "calendar_subset.h"', '#include "xmq_portable/calendar_subset.h"')
            (component_dir / "xmq_generated.h").write_text(generated, encoding="utf-8")
        finally:
            sys.path.pop(0)
    cg.add(meter.set_xmq_shadow(config[CONF_XMQ_SHADOW]))

    radio = await cg.get_variable(config[CONF_RADIO_ID])
    cg.add(meter.set_radio(radio))
    await cg.register_component(meter, config)

    for conf in config.get(CONF_ON_TELEGRAM, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], meter)
        await automation.build_automation(
            trig,
            [(MeterRef, "meter")],
            conf,
        )

