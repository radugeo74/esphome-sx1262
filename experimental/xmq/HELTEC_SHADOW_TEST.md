# First Heltec XMQ shadow field test (no new radio drivers)

Status: **procedure only**. Not a deployable YAML; the repository CI examples use dummy IDs and contain no Wi-Fi/API/OTA secrets.

## Before flashing

1. Export/retain the currently working ESPHome YAML (including every meter, sensor, pin, radio mode, API/OTA settings and substitutions). Keep a known-good binary or a reproducible known-good Git commit for rollback.
2. Copy the real YAML to a private experimental file; **do not commit secrets, API/OTA keys or real meter identifiers**.
3. Build against branch `experimental/xmq-independent-pipeline` with its local `components` source. Do not use a default-branch GitHub source: it may fetch another revision.
4. For each existing `wmbus_meter` entry, add:

   ```yaml
   xmq_shadow: true
   xmq_only: false
   ```

   Keep existing `meter_id`, `type`, `mode`, `key`, `radio_id`, sensors and radio hardware setup unchanged. In particular, **do not copy the fake meter IDs or GPIO fixture pins from the CI YAML**.
5. Run ESPHome `config` and `compile` on the **actual Heltec YAML** before any installation. The CI sample build does not establish that the real device configuration compiles. Verify the branch's generated XMQ tables are included (the compiler fails if enabled but unavailable).
6. Preserve a working local flashing/rollback route; prefer a controlled maintenance window. Nothing is installed by this repository procedure.

## Observations after an authorized test install

- Confirm boot, Wi-Fi/API, normal RSSI/packet reception, available memory, and no repeated reset/watchdog.
- Confirm existing heat and water sensor values still update using the legacy authority path.
- For CI A0/A2, inspect `Independent XMQ plain diagnostic`, `Independent XMQ vs legacy parity`, and `Independent XMQ plain parity field`. Investigate `different`, `missing_xmq`, `missing_legacy` with the driver's field name.
- Do not interpret all-zero mismatches as proof of parity without actual decoded frames. `observed_match` is not freshness verification. A0/A2 is the relevant independent plaintext path for the configured fhkvdataiii and mkradio4 devices.
- Mode 5 (CI 72/7A) is experimental, independent shadow-only. Even a successful decrypt and XMQ parse is not evidence that its application payload is valid for the A0/A2 manufacturer-specific grammars.
- Check several reception cycles per configured device, not only one meter and not only CI host tests.

## Rollback triggers

Loss of expected sensor updates, abnormal reboot frequency, persistently worsening memory, or reception/decoding regressions: restore the pre-test binary/configuration using the preserved recovery path.

**Do not set `xmq_only: true` on the field-test device. Do not remove `handleTelegram()`.** Those require independently reviewed, real-telegram parity and a separate deployment decision.
