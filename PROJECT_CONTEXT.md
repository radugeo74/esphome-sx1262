# esphome-sx1262 context

Updated 2026-10-21. Work branch: native-espidf. Stable master is untouched.

Target: ESPHome 2026.10.0 / ESP-IDF 5.5.6, native CMake/Ninja, ESP32-S3 Heltec
WiFi LoRa 32 V3, 8 MB flash, SX1262 T1. Do not substitute PlatformIO.

PrintProperty::PRINT_PROPERTY_STATUS retains numeric value 4 and serialized
string STATUS. Both socket actions execute synchronously. ESPHome 2026.10
requires play(const Ts &...x); frame callbacks carry Frame*.

Native builds passed for the supplied 12-meter/50-sensor Heltec configuration
with sanitized credentials and for tests/native-espidf.yaml including all socket
formats. See NATIVE_ESPIDF_REPORT.md for versions, warnings and hardware limits.

Original user YAML/credentials are outside the repository. Never commit them.
No device flashing is authorized. Runtime reception, decoding and Home Assistant
checks need the owner's hardware. Keep meter IDs, sensor names, fields and pins
unchanged when applying this migration to an existing configuration.
