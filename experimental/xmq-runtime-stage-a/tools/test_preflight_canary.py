#!/usr/bin/env python3
"""Synthetic-only regression checks for the local confidential preflight."""
import contextlib
import copy
import io

from preflight_canary import check, differences

old={
    "esphome":{"name":"heltec-stick-lite-8mb"},
    "esp32":{"board":"heltec_wifi_lora_32_V3"},
    "wifi":{"ssid":"ci-only","password":"dummy-value"},
    "api":{"encryption":{"key":"ci-dummy-key"}},
    "ota":[{"platform":"esphome","password":"ci-dummy-ota"}],
    "sx126x":{"frequency":868950000,"tcxo_voltage":"3_0V"},
    "wmbus_radio":{"native_stream_mode":"T1_C1"},
    "wmbus_meter":[
        {"id":"heat_baie","meter_id":0x12345678,"key":"ci-placeholder",
         "type":"fhkvdataiii","mode":["T1"]},
        {"id":"water_calda_baie","meter_id":0x87654321,"key":"ci-placeholder",
         "type":"mkradio4","mode":["T1","C1"]}],
    "sensor":[{"platform":"wmbus_meter","parent_id":"heat_baie",
               "name":"Existing Heat Entity","field":"current_hca"}],
    "external_components":[{"source":"old-reviewed-lab-source",
       "components":["sx126x","wmbus_common","wmbus_radio","wmbus_meter"]}],
}
candidate=copy.deepcopy(old)
candidate["xmq_shadow_probe"]={"enabled":False}
candidate["external_components"]=[
    {"source":"legacy-local","components":["sx126x","wmbus_common","wmbus_radio"]},
    {"source":"experimental-clone","components":["wmbus_meter","xmq_shadow_probe"]},
]
with contextlib.redirect_stdout(io.StringIO()) as out:
    check(old,candidate)
assert "PREFLIGHT_OK" in out.getvalue()

def must_block(modify,label):
    other=copy.deepcopy(candidate)
    modify(other)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            check(old,other)
    except ValueError as exc:
        # Never print exception values, since a change could be confidential.
        assert isinstance(exc,ValueError),label
        return
    raise AssertionError("preflight accepted unsafe change: "+label)

must_block(lambda c:c["xmq_shadow_probe"].update(enabled=True),"probe on")
must_block(lambda c:c["wifi"].update(password="new-value"),"wifi changed")
must_block(lambda c:c["api"]["encryption"].update(key="changed"),"API changed")
must_block(lambda c:c["ota"][0].update(password="changed"),"OTA changed")
must_block(lambda c:c["esphome"].update(name="wrong-device"),"device name changed")
must_block(lambda c:c["sx126x"].update(frequency=123),"RF changed")
must_block(lambda c:c["wmbus_radio"].update(native_stream_mode="T1"),"C1 dropped")
must_block(lambda c:c["wmbus_meter"][0].update(key="different"),"meter AES changed")
must_block(lambda c:c["wmbus_meter"][1].update(meter_id=42),"meter id changed")
must_block(lambda c:c["wmbus_meter"][1].update(mode=["T1"]),"C1 meter mode dropped")
must_block(lambda c:c["sensor"][0].update(name="new HA name"),"HA entity changed")
must_block(lambda c:c["sensor"].clear(),"sensor removed")
must_block(lambda c:c["wmbus_meter"].clear(),"meter removed")
must_block(lambda c:c["external_components"][0].update(components=["wmbus_meter"]),
           "radio components omitted")
must_block(lambda c:c["external_components"].append(
           {"source":"unknown","components":["arbitrary_component"]}),
           "unknown external component")
must_block(lambda c:c["xmq_shadow_probe"].pop("enabled"),"missing explicit disable")
assert differences({"secret":"original"},{"secret":"updated"})==["secret"]
print("PREFLIGHT_TEST_OK: 17 cases, confidentiality and fail-closed structural parity")
