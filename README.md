# Native wM-Bus / XMQ on ESPHome SX126x

**Experimental Heltec ESP32-S3 + SX1262 receiver** for wireless M-Bus
T1 and C1, with a native C++ parsing/decoding pipeline and configurable
Home Assistant sensors.

**Active branch:** `experimental/xmq-multidriver-auto`  
**Last fully green code CI:** [GitHub Actions #42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133)
(code commit `8cb03357806b50316e88e3eb9e25a1cffb7e4813`, all three jobs passed).

> This branch is a distinct native implementation. It **does not use
> `wmbus_radio` / `wmbus_meter` or the old
> `Meter::handleTelegram()` pipeline as its live receiver/decoder**.
> Earlier experiments in this repository used different components; do
> not copy their YAML into this branch. The current architecture and
> safety rules are recorded in [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).

## Architecture

- **Radio:** ESPHome's SX126x component, locally patched for T1/C1
  progressive FIFO reception; target board is Heltec WiFi LoRa 32 V3
  (ESP32-S3 / SX1262).
- **wM-Bus:** `components/wmbus_native_sx126x` handles radio-to-DLL
  framing, CRC verification, T1/C1 routing, meter identity, security
  checks, driver selection and mapped sensor publication.
- **Decoding:** native C++ implementations and selected generated XMQ
  rules, checked against public original `wmbusmeters` XMQ fixtures.
  The old `wmbusmeters` radio/decoder stack is **not an active
  dependency** of this receiver.
- **Home Assistant:** only explicitly configured meter IDs and mapped
  `xmq_sensors`, `xmq_text_sensors` or `rssi_sensors` can publish
  values. Unknown meters may be recognized in diagnostics, but do not
  cause entities or readings to be created.

### Supported driver selection

| Driver | `driver: auto` for a configured meter | Explicit `meter_drivers` |
| --- | --- | --- |
| `fhkvdataiii` (Techem heat) | Yes | Yes |
| `mkradio4` (Techem water) | Yes | Yes |
| `iperl` | Yes (supported clear payloads) | Yes |
| `hydrus` | Yes (supported clear payloads) | Yes |
| `lansenpu` (pulse channels A/B) | Yes | Yes |
| `lansendw` (door/window + pulse events) | Yes | Yes |
| `cma12w` (room temperature, battery) | Yes | Yes |
| `ultraheat` (heat energy, flow, temperature) | Yes, verified clear short TPL | Yes |
| `sontex868` (heat allocation and temperature) | Yes, verified clear short TPL | Yes |
| `minomess` | Yes, clear transport only | Yes |
| `kamwater` | Yes, clear transport only | Yes |
| `apator162` | Yes, clear transport only | Yes |

The new `ultraheat` and `sontex868` decoders currently cover the
publicly verified **unencrypted short-TPL numeric subset** only.
Ultraheat provides `heat_kwh`, `volume_m3`, `power_kw`,
`flow_m3h`, `flow_c`, `return_c`; Sontex868 provides
`current_consumption_hca`, `consumption_at_set_date_hca`,
`current_temp_c`, `current_room_temp_c`, `max_temp_c`,
`max_temp_previous_period_c`. Do not infer that other upstream
fields, wired M-Bus frames or post-decrypted ELL frames are supported.
Original source fields and strict negative-frame tests passed
[run #42393848471](https://github.com/radugeo74/esphome-sx1262/actions/runs/42393848471).

### Encrypted-only driver: `mkradio4a`

`mkradio4a` is now additionally software-integrated through
**explicit `secure_meters` only**. The public original XMQ
examples use Techem `TCH/95/37` radio converters and nested
`HYD/FE/06` (warm) or `WEH/FE/07` (cold) meter headers. The
receiver must match **both configured IDs**, validate DLL CRC,
decrypt exactly three AES-CBC Mode-5 blocks with the explicit AES-128
key, then validate the original application before publishing
`target_m3` and `target_date` (ISO `YYYY-MM-DD`).

```yaml
wmbus_native_sx126x:
  # ...sx126x_id and radio options from your complete YAML...
  secure_meters:
    - meter_id: "12345678"      # synthetic nested TPL ID
      dll_meter_id: "87654321"  # synthetic radio converter DLL ID
      driver: mkradio4a
      key: !secret example_mkradio4a_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: target_m3
      sensor:
        name: "Example MK Radio 4a target"
        unit_of_measurement: "m³"
  xmq_text_sensors:
    - meter_id: "12345678"
      field: target_date
      sensor:
        name: "Example MK Radio 4a target date"
```

Use **actual corresponding** meter/convertor IDs and the correct
private AES key in your own YAML; these values are illustrative.
The upstream `NOKEY` samples contain post-decrypted application
records, not verified on-air ciphertext. Regression tests use
synthetic AES encryption of both original sample applications; there
is **no real MK Radio 4a RF/HA validation**. All three new-driver CI jobs passed [run #42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133). AES-CBC and the
`2F 2F` marker are **not cryptographic authentication**.
`mkradio4a` is deliberately **not allowed in plaintext
`meter_drivers` or `driver: auto`**.

### Software support and acceptance status

**All 12 drivers in the table are software-configurable**, but only
the Techem baseline has been observed with real nearby meters.
The active driver manifest defines:

- `SUPPORTED`: generic native compiled/DLL AUTO drivers.
- `CONFIGURED_NATIVE = ("minomess", "kamwater", "apator162")`:
  software-tested native drivers with strict **configured-only** AUTO.
- `CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE`:
  the 12 names allowed in YAML `meter_drivers`.
- `SECURE_ONLY = ("mkradio4a",)`: the 13th driver, available only
  via explicitly keyed `secure_meters`, with separate DLL/TPL identity.

The old `BATCH2_PENDING` category was retired after software
integration. The three models are **not pending software implementation**;
they are omitted from generic `SUPPORTED` only because their
DLL/TPL/ELL identity/security admission requires a separate native
dispatcher. This restriction does not block `driver: auto` or an
explicit driver on a configured meter.

A matched manufacturer/model never proves the payload is clear,
valid or authenticated. **Physical RF-to-Home Assistant validation
is still open** for the new driver families.

## YAML: automatic or explicit driver selection

For a **configured meter**, both forms are supported. Use an
eight-digit, decimal-string meter ID (including leading zeroes). The
examples use **synthetic IDs only**.

**Option A — automatically identify the driver:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: auto
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example meter total"
        unit_of_measurement: "m³"
```

The firmware selects a driver from the verified telegram metadata,
then the chosen decoder independently checks transport, meter identity,
security and field validity. No matching/valid frame means **no
sensor update**.

**Option B — specify the driver yourself:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: kamwater
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example Kamwater total"
        unit_of_measurement: "m³"
```

In both examples, `sx126x_id` must reference a correctly configured
`sx126x:` radio in the **same full YAML**. These snippets demonstrate
driver selection; they are not standalone firmware configurations.
Map only fields supported by the selected meter and driver.

**Minomess behind a radio converter:** when the radio's DLL sender ID
differs from the actual meter ID in the long TPL address, configure
both explicitly. This works with `driver: auto` or `driver: minomess`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  meter_drivers:
    - meter_id: "12345678"      # meter identity in TPL
      dll_meter_id: "87654321"  # sender identity in DLL
      driver: auto              # or: minomess
```

The two identities must match their respective headers; a wrong or
ambiguous match blocks decoding. A distinct `dll_meter_id` is not a
general gateway auto-discovery feature.

### Encryption and discovery boundaries

**`driver: auto` does not guess AES keys or decrypt encrypted
telegrams.** For encrypted meters, use an explicit driver and the
appropriate AES-128 key in `secure_meters`, usually via `!secret`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  secure_meters:
    - meter_id: "12345678"
      driver: minomess
      key: !secret example_minomess_aes_key
```

`secure_meters` supports the separately reviewed keyed paths for
`minomess`, `kamwater`, `apator162` and `mkradio4a`; it **requires an explicit
driver**, not `auto`. A telegram that claims encryption but lacks a
configured valid key must not be published as plaintext. Failed
integrity, identity, payload or cryptographic checks do not publish
partial, stale or synthetic values. C1 plaintext sensor publication
additionally requires the opt-in `c1_plaintext_publish: true`.

CRC-verified T1/C1 frames from **unconfigured** meters can be
recognized passively, including Minomess, Kamwater, Apator162 and
Lansen, using original XMQ manufacturer/version/type signatures.
Diagnostic `BATCH2 AUTO candidate=...` messages do **not** decode,
create entities, or publish meter values.

### New cleartext drivers: Lansen DW and Elvaco CMA12w

The native pipeline also supports `lansendw` (Lansen
door/window, `a_counter` / `b_counter`, text `status` OPEN/CLOSED,
`error_flags`) and `cma12w` (Elvaco temperature,
`current_temperature_c`, `average_temperature_1h_c`,
text `battery`, `software_version`, `status`).
Both accept `driver: auto` or the explicit driver name on
a *configured* eight-digit meter ID. They use strict mode-zero
TPL frames and four original upstream `NOKEY` examples. The
new decoders reject wrong meter/model, encrypted modes and invalid
records rather than publishing partial sensor updates.
`lansenth` is **not** enabled: its upstream NOKEY examples
carry encrypted TPL Mode 5 and must not be misclassified as clear RF.

## Building and testing

The branch includes a **build-only** Heltec T1/C1 laboratory YAML at
[experimental/semtech-radio/heltec-sx126x-combined-lab.yaml](experimental/semtech-radio/heltec-sx126x-combined-lab.yaml).
It uses synthetic or public upstream example IDs and exercises
`driver: auto` for the eight newer drivers (including Ultraheat and Sontex868). Check the board revision and real radio pin/TCXO mapping
before adapting it to your own hardware.

GitHub Actions run [#42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133)
passed `native-packet-input`,
`stage-a-espidf-s3-portable-link` and `native-esphome-build`.
This establishes host regression coverage and ESP32-S3/Heltec
**compilation**, not reception or correct HA publication from real
Minomess/Kamwater/Apator162/Lansen/Elvaco/Ultraheat/Sontex devices. The known-working Techem
behavior has been observed separately on a Heltec, but does not
validate every newly added driver.

**Do not flash the existing production Heltec solely on the strength
of CI.** Preserve a working firmware backup and test any new build
with real reception before declaring field readiness. Do not commit
private meter IDs, encryption keys, Wi-Fi secrets or production
telegrams.

For implementation notes, start with
[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md),
[experimental/xmq/README.md](experimental/xmq/README.md),
[experimental/xmq/STAGE_B_GATE_STATUS.md](experimental/xmq/STAGE_B_GATE_STATUS.md),
[experimental/xmq/BATCH2_STATUS.md](experimental/xmq/BATCH2_STATUS.md)
and the [CI workflow](.github/workflows/native-sx126x-adapter.yml).

## Project origin, credits and licensing

This **current native SX126x + XMQ architecture is not a continuation
of the old `SzczepanLeon/esphome-components` runtime**. That project
is part of the **historical development lineage** of earlier wM-Bus
experiments in this repository; it is not the active radio/decoding
component described above.

The active project uses the ESPHome SX126x component with local
modifications, plus native implementation work in this repository.
Original XMQ driver specifications and public fixture expectations
come from [wmbusmeters](https://github.com/wmbusmeters/wmbusmeters)
and retain their upstream copyright/licence notices. Historical
SzczepanLeon contributions and any other imported source still
retain the attribution and licensing applicable to those files:
architecture changes do **not** erase the origin of reused code.
Consult the individual file headers before reuse or redistribution.

For earlier development and the former relationship with
[SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components),
see the [archived historical README](docs/history/xmq-independent-pipeline-README.md).
