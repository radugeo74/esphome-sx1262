# Native ESP-IDF build regression fixture

Create an isolated Python 3.13 environment and install `requirements.txt`.
Run from the repository root:

```sh
python -m pip uninstall -y platformio
python -m esphome config tests/native-espidf.yaml
python -m esphome compile tests/native-espidf.yaml --only-generate
python -m esphome compile tests/native-espidf.yaml
```

The fixture pins ESP-IDF 5.5.6, uses the native ESP-IDF backend (CMake/Ninja),
and loads the working tree so unpublished changes are tested. It covers SX1262,
Heltec V3 / ESP32-S3 / 8 MB, the specified SPI/radio pins, T1, seven fhkvdataiii
and five mkradio4 meters, 50 numeric sensors, native API, OTA, the generic socket
action and all three frame socket formats. Credentials and meter IDs are dummy
values. It is not a replacement for an existing Home Assistant configuration.
Do not flash this test firmware.

For a deployed configuration, use:

```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@native-espidf
    refresh: always
```

No MQTT configuration or PlatformIO backend is needed. ESPHome's Python package
may install PlatformIO as an upstream package dependency; this build uses its
native ESP-IDF backend, not PlatformIO. The validation environment removes that
package explicitly before the regression build.

On Windows, if IDF tool discovery reports a missing `riscv32-esp-elf` after the
ESP32-S3-only install, set `ESPHOME_IDF_DEFAULT_TARGETS=esp32s3;esp32c3` for the
first build to install both required compiler archives. The firmware target
remains ESP32-S3.

Compilation cannot validate RF reception, actual meter telegrams, RSSI accuracy,
Home Assistant connectivity or an OTA update. Verify those on hardware separately.
