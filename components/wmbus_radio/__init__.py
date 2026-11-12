from contextlib import suppress
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sx126x, sx127x
from esphome.cpp_generator import LambdaExpression
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_FORMAT,
    CONF_DATA,
)

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]

DEPENDENCIES = ["esp32"]

AUTO_LOAD = ["wmbus_common"]

MULTI_CONF = True

CONF_SX126X_ID = "sx126x_id"
CONF_SX127X_ID = "sx127x_id"
CONF_NATIVE_STREAM_MODE = "native_stream_mode"
CONF_NATIVE_PHY_DIAGNOSTICS = "native_phy_diagnostics"
CONF_ON_FRAME = "on_frame"
CONF_MARK_AS_HANDLED = "mark_as_handled"

radio_ns = cg.esphome_ns.namespace("wmbus_radio")
RadioComponent = radio_ns.class_("Radio", cg.Component)
Frame = radio_ns.class_("Frame")
FrameOutputFormat = Frame.enum("OutputFormat")
FramePtr = Frame.operator("ptr")
FrameTrigger = radio_ns.class_(
    "FrameTrigger", automation.Trigger.template(FramePtr))

COMMON_SCHEMA = {
    cv.GenerateID(): cv.declare_id(RadioComponent),
    cv.Optional(CONF_ON_FRAME): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FrameTrigger),
            cv.Optional(CONF_MARK_AS_HANDLED, default=False): cv.boolean,
        }
    ),
}

NATIVE_SCHEMA = (
    cv.Schema(
        {
            **COMMON_SCHEMA,
            cv.Optional(CONF_SX126X_ID): cv.use_id(sx126x.SX126x),
            cv.Optional(CONF_SX127X_ID): cv.use_id(sx127x.SX127x),
            cv.Optional(CONF_NATIVE_PHY_DIAGNOSTICS, default=False): cv.boolean,
            cv.Optional(CONF_NATIVE_STREAM_MODE, default="T1"): cv.one_of(
                "T1", "T1_C1", "T1_C1_B", upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

def validate_radio_source(config):
    if (CONF_SX126X_ID in config) == (CONF_SX127X_ID in config):
        raise cv.Invalid("Configure exactly one of sx126x_id or sx127x_id")
    if CONF_SX127X_ID in config and config[CONF_NATIVE_STREAM_MODE] != "T1":
        raise cv.Invalid("SX127x FIFO stream currently supports T1 only; C1 requires separate PHY support")
    return config


CONFIG_SCHEMA = cv.All(NATIVE_SCHEMA, validate_radio_source)



async def to_code(config):
    # Only patched SX126x owns the SPI bus and IRQ.
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_SX126X_ID in config:
        radio = await cg.get_variable(config[CONF_SX126X_ID])
        cg.add(var.set_native_sx126x(radio))
    else:
        radio = await cg.get_variable(config[CONF_SX127X_ID])
        cg.add(var.set_native_sx127x(radio))
        cg.add(radio.set_wmbus_t1_mode(True))
    cg.add(var.set_native_t1_c1_b(
        config[CONF_NATIVE_STREAM_MODE] != "T1"
    ))
    cg.add(var.set_native_phy_diagnostics(config[CONF_NATIVE_PHY_DIAGNOSTICS]))
    await cg.register_component(var, config)

    for conf in config.get(CONF_ON_FRAME, []):
        trig = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, conf[CONF_MARK_AS_HANDLED])
        await automation.build_automation(
            trig,
            [(FramePtr, "frame")],
            conf,
        )


with suppress(ImportError):
    from ..socket_transmitter import (
        SOCKET_SEND_ACTION_SCHEMA,
        SocketTransmitterSendAction,
    )

    FRAME_SOCKET_SEND_SCHEMA = SOCKET_SEND_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_FORMAT): cv.one_of(
                "hex",
                "raw",
                "rtlwmbus",
                lower=True,
            ),
            cv.Optional(CONF_DATA): cv.invalid(
                "If you want to specify data to be sent, use generic 'socket_transmitter.send' action"
            ),
        }
    )

    @automation.register_action(
        "wmbus_radio.send_frame_with_socket",
        SocketTransmitterSendAction,
        FRAME_SOCKET_SEND_SCHEMA,
        synchronous=True,
    )
    async def send_frame_with_socket_to_code(config, action_id, template_arg, args):
        output_type = {
            "hex": cg.std_string,
            "raw": cg.std_vector.template(cg.uint8),
            "rtlwmbus": cg.std_string,
        }[config[CONF_FORMAT]]

        paren = await cg.get_variable(config[CONF_ID])
        var = cg.new_Pvariable(
            action_id, cg.TemplateArguments(output_type, *template_arg), paren
        )
        template_ = LambdaExpression(
            f"return frame->as_{config[CONF_FORMAT]}();", args, ""
        )

        cg.add(var.set_data(template_))

        return var
