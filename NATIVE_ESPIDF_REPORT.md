# Native ESP-IDF validation — 2026-10-21

## Scope and result

Validated on Windows with ESPHome 2026.10.0, ESP-IDF 5.5.6, Python 3.13.5,
Xtensa GCC 14.2.0 (crosstool-NG esp-14.2.0_20260121), CMake 3.30.2 and Ninja 1.12.1.
Both configurations completed native CMake/Ninja compilation, linking, partition
size checks and binary generation. PlatformIO was uninstalled from the isolated
Python environment before the socket build and final Heltec rebuild; import
availability was checked and false. No PlatformIO backend or MQTT is required.

Base: `2afb12a3a6fb5624d8660fcc90bbb304ce575852`, fetched from origin/native-espidf.
Only native-espidf is modified. No merge or flash operation was performed.

## Changes and evidence

- `components/wmbus_common/meters.h`: rename the enum member to
  `PRINT_PROPERTY_STATUS = 4`; preserve `hasSTATUS()` and all other flags.
- `components/wmbus_common/meters.cc` and 50 `driver_*.cc` files: update only
  qualified enum references. Text conversion continues to use `"STATUS"` in both
  directions. A byte-normalized comparison against the base verified these 52
  files have exactly the requested symbol substitution and no other changes.
- `components/socket_transmitter/__init__.py` and
  `components/wmbus_radio/__init__.py`: explicitly register both actions with
  `synchronous=True`. ESPHome's Action::play_complex calls play then play_next_
  immediately; SocketTransmitterSendAction::play calls send directly, without
  scheduling a continuation. Python async code generation does not make the
  runtime action asynchronous.
- `components/wmbus_radio/__init__.py`: generate `frame->as_...()` because the
  trigger argument is Frame*. The previous generated C++ used invalid dot access.
- `components/socket_transmitter/socket_transmitter.h`: change the override to
  `play(const Ts &...x)` to match ESPHome 2026.10. The socket build reproduced
  `marked override, but does not override` and abstract-class errors before this
  correction. Its body and execution ordering are unchanged.
- `README.md`: external component source points to native-espidf with refresh always.
- `tests/native-espidf.yaml`, `tests/requirements.txt`, `tests/README.md`: reproducible
  synthetic fixture, exact ESPHome/IDF versions and build instructions.
- `.gitignore`: exclude generated test build files and Python caches.
- This report and `PROJECT_CONTEXT.md`: validation scope and operational limits.

The source already contained no MQTTPublishAction; no MQTT code needed removal.
No external add_library/PlatformIO dependency is used by these components. SX1262
is implemented in the repository and compiles against ESPHome SPI/FreeRTOS.
Native CMake included the selected .cc drivers correctly. No ESP-IDF deprecated
API compiler warning or further header/link failure was found in these builds.

## Configuration and build results

The supplied heltec-stick-lite-8mb.yaml was not edited. A private validation copy
keeps its node/friendly name, all 50 sensor definitions, all 12 meter IDs, 7
fhkvdataiii / 5 mkradio4 drivers, T1 modes, SPI 9/10/11, SX1262 CS8/RESET12/IRQ14,
GPIO35 output, API encryption, OTA password mode, time and network structure.
A parsed comparison verified the preserved definitions. Credentials were replaced
with test values, and the local working tree was loaded to test unpublished code.
A separate sanitized copy has the requested GitHub native-espidf source and
refresh always. Neither private configuration is committed to the public repo.

The synthetic fixture additionally instantiates socket_transmitter.send and all
three send_frame_with_socket formats. It contains 12 dummy meter IDs and 50
sensors covering RSSI, current/previous HCA, both temperatures and total/target m3.

| Check | Heltec supplied configuration, sanitized | Synthetic socket fixture |
|---|---|---|
| YAML validation | PASS | PASS |
| C++ generation | PASS | PASS |
| Native compilation and linking | PASS | PASS |
| OTA/factory binary generation | PASS | PASS |
| OTA .bin bytes (actual padded file) | 1,056,704 | 947,760 |
| Application partition bytes | 3,932,160 (0x3C0000) | 3,932,160 |
| OTA partition utilization | 26.87% | 24.10% |
| Free OTA partition bytes | 2,875,456 | 2,984,400 |
| Reported static RAM bytes | 111,439 / 341,760 | 111,059 / 341,760 |

Both OTA partitions have the same size. Heltec factory.bin is 1,122,240 bytes.
ESP-IDF check_sizes.py passed. The .bin size above includes padding; the linker
reports 1,056,583 bytes for the Heltec application image before padding.

Additional targeted compiler check: the extracted old PrintProperty enum plus an
IDF-shaped STATUS typedef reproduces the reported redeclaration error (exit 1).
The new enum compiles (exit 0) with static_assert on numeric value 4.

On the first Windows setup ESPHome installed only Xtensa, but IDF tool discovery
also required riscv32-esp-elf. Installing with
`ESPHOME_IDF_DEFAULT_TARGETS=esp32s3;esp32c3` resolved the environment issue.
The generated firmware target remained esp32s3. No upstream framework source
patch was added to this repository.

## Remaining warnings

The Heltec full build emitted 40 compiler warnings in existing code:

- util.cc: two time_t/long printf mismatches and one extra format argument.
- formula.cc: unsigned comparison against zero.
- meters.cc: one time_t/unsigned-long printf mismatch and deprecated implicit
  Telegram copy assignment.
- transceiver_sx1262.cpp: uint32_t/unsigned-int hexadecimal printf mismatch.
- packet.cpp: dangling-else warning and 32 switch warnings for modes other than
  the implemented cases (including UNKNOWN). T1/C1 have explicit cases.

These were not broadened into decoder/receiver refactors. The socket fixture
uses the same code; the override warnings from its initial failed attempt are
resolved by the final fix. No missing-synchronous warning remains.

ESPHome also recommends OTA encryption instead of password mode. The existing
OTA method was retained. CMake reports that GCC 14.2.0 on Windows cannot use the
precompiled header and compiles normally without it. The upstream esp_blockdev
folder has no CMakeLists.txt and is skipped; it is not used by this project.

## Regression boundaries and rollback

The requested decoder files, meter filtering/dispatch, RSSI extraction, radio
receiver, packet conversion and SX1262 implementation are unchanged from base.
Static review and compilation cover their integration, not live behavior.
No telegram replay test was executed. Actual T1 reception, decoder outputs from
real meters, meter_id rejection, RSSI accuracy, Home Assistant entity continuity,
API connection and OTA transfer still need hardware validation by the owner.
No USB/OTA flash, network telegram send or live device access was performed.

Artifacts use test credentials and are build-validation artifacts, not deployment
images for the existing device. Rebuild with the original private credentials
and native-espidf external source for device testing.

The code changes are small; remaining risks are untested live RF/network behavior
and pre-existing warnings listed above. The const-reference action signature
specifically targets ESPHome 2026.10; older ESPHome versions are not claimed.

Rollback: on native-espidf, revert the migration commit with git revert. A verified
Git bundle of origin/native-espidf at the base commit is stored with local
validation artifacts. Do not reset or merge master.
