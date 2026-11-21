# Heltec SX1262 XMQ shadow — local canary verification (NO automatic flash)

This procedure is for the private **12-meter** Heltec Stick Lite 8MB firmware,
not the synthetic `heltec-xmq-shadow-enabled-lab.yaml` used in GitHub CI.
Never flash that synthetic CI fixture to the live device. The original
`wmbus_meter` decoder remains the only writer of Home Assistant states.

## Release gates (local machine only)

1. Check that the Stage A workflow is **fully green** (host + four builds), not
   just the host job. Pin a reviewed branch commit for every external ESPHome
   component; do not consume a moving experimental branch during a canary.
2. Keep a private copy of the **known-working firmware application .bin**,
   its exact source YAML, its ESPHome/ESP-IDF version, and a proven USB recovery
   procedure. A GitHub compile is not a backup of the original firmware.
3. Prepare a second, **private** candidate YAML with identical network, API,
   OTA, pins, frequency, T1/C1 handling, all 7 heat and 5 water meter entries,
   IDs and locally stored AES keys (if any). The first new candidate should
   have `xmq_shadow_probe.enabled: false`. The existing canary gate rejects
   `enabled: true` deliberately. For an **enabled** upgrade after that,
   perform a separate human-reviewed change and compile; do not bypass the
   first-stage gate or treat its approval as authorization for the second.
4. Build both images **locally** using the reviewed ESPHome version. From the
   repository root, with the appropriate local Python environment installed:

   ```bash
   python experimental/xmq-runtime-stage-a/tools/canary_release_gate.py \
      --reference /private/heltec-known-good.yaml \
      --candidate /private/heltec-xmq-disabled.yaml \
      --fallback-bin /private/heltec-known-good.bin \
      --canary-bin /private/heltec-xmq-disabled.bin \
      --manifest /private/heltec-canary-manifest.json
   ```

   The command performs validation and writes a **private** SHA-256 manifest;
   it never flashes. If it fails, do **not** flash. Never add the private YAML,
   images, manifest or device logs to GitHub. Confirm that source revisions and
   rollback provenance match before manually installing.
5. After the disabled canary works normally, the enabled canary is a **separate
   opt-in step** requiring an exact private YAML diff, a locally compiled
   firmware image, memory and rollback checks, and explicit user approval.
   Maintain legacy ownership of every sensor. Do not replace the decoder.

## Hardware-specific guard for the existing T1_C1 installation

**DO NOT FLASH** `heltec-xmq-shadow-enabled-lab.yaml`,
`heltec-xmq-canary.yaml`, or `heltec-xmq-canary-12-meters.yaml`
to the real 12-meter Heltec. The 12-meter GitHub fixture intentionally
uses `native_stream_mode: T1` and `mode: [T1]` for host compilation,
whereas the working Heltec installation uses **`T1_C1`**. A green GitHub
build is not evidence that the fixture preserves its actual radio settings.

Use the **private, running, known-good YAML** as the source of truth for
every candidate. Preserve the active SX126x pins, 868.95 MHz parameters,
`native_stream_mode: T1_C1`, all 12 meter modes/IDs/keys, entity names,
Wi-Fi, API and OTA settings, and the existing legacy-only HA writer.
The original 12-meter fixture is only a synthetic compile regression.

1. Preserve the actual known-good image and private YAML, and record the
   currently installed source revision. Confirm USB rollback works.
2. Prepare and locally compile the **disabled** XMQ candidate by changing
   only the reviewed component sources and adding
   `xmq_shadow_probe.enabled: false`. Run the existing local fail-closed
   parity and rollback gates; reject any change to T1/C1 or meter topology.
3. Only after the disabled candidate is independently validated on the
   physical Heltec, create a **separate enabled candidate** from that exact
   reviewed private configuration. The only intended semantic change at
   this stage is `xmq_shadow_probe.enabled: true`; inspect the resolved
   YAML diff locally and retain the disabled image for immediate rollback.
   The existing `canary_release_gate.py` intentionally rejects enabled
   candidates: **do not bypass or weaken that gate**. The enabled image
   needs its own manual review and explicit installation approval.
4. Do not publish real configuration, IDs, decryption keys, captures,
   manifests or firmware backup to GitHub. Do not consider host-only
   setter-provenance tests proof of live setter attribution: the installed
   probe still logs `provenance=not_proven` until a separately audited
   live integration can demonstrate the same-frame contract.

## What to verify on the device after an approved enabled canary

- Confirm the active build version and uptime in the boot log. Installing an
  updated GitHub commit does **not** update a running Heltec automatically.
- Find `Experimental XMQ lab shadow: live bounded numeric shadow` and
  `Live XMQ shadow:`. Confirm actual `mkradio4` and `fhkvdataiii` observations.
- On the 128th accepted observation **within a window**, expect exactly one
  `XMQ shadow hourly observation budget exhausted` line. Further matching
  frames still go through legacy, but should not add XMQ comparisons.
- After one hour from the current budget window start, the **next eligible
  legacy telegram**, not a background timer, should emit
  `XMQ shadow hourly observation budget renewed`. The next comparison should
  restart the `observed` count at 1. If the capture begins mid-window, it may
  not include either boundary; absence in a short capture is inconclusive.
- Keep logging `matched`, `different`, `compared`, `mismatches`,
  `provenance=not_proven`, current/minimum heap, largest free block,
  fragmentation, sensor updates and uptime; never publish private IDs or frames.
  The local offline summary is:

  ```bash
  python experimental/xmq-runtime-stage-a/tools/summarize_shadow_log.py /private/heltec-capture.txt
  ```

  Note: `matched` and `different` in firmware logs are **cumulative within
  one window**. Do not sum them across log lines; the local script counts
  individual `disposition` events. Snapshot equality is not setter provenance.

## First physical Heltec shadow observation (2026-11-21)

A private ~4-minute capture on ESPHome 2026.11.0 with XMQ enabled
showed 11 XMQ snapshot matches out of 11 comparisons: 7 heat observations
at four numeric fields each and 4 water observations at two numeric
fields each (36 numeric comparisons, zero mismatches). Legacy Home
Assistant sensor updates continued, and both T1/A and C1/A frames were
observed. A subset of configured water meters had C1 frames rejected by
the meter's existing T1-only mode; do not change meter modes to conceal
that warning. No private meter identifiers or payloads are included here.

**Do not interpret these 11 matches as independent XMQ decoder parity.**
Every observation reported `provenance=not_proven`; all reported numeric
DV attribution counters were `matched_numeric_dv=0`, and
`calendar_refs=0`. Snapshot equality, even with changing values, is
not a confirmed same-frame setter journal. The live firmware does not
yet connect the host-tested `LegacyWriteJournal` to actual legacy setter
calls. It is unsafe to change the production sensor writer on this
evidence.

The capture is far shorter than an hour, so it does not verify
128-observation exhaustion, hourly renewal, long-run heap stability,
or an actual full 12-meter equality set. Leave the enabled canary
read-only; a later host-only journal gate must not be labelled
`proven` until actual firmware setter hooks are safely audited.

## Stop conditions and rollback

Stop the experiment and reinstall the verified previous image if the legacy
sensors stop updating, radio T1/C1 behavior materially changes, repeated
reboots/watchdogs appear, or heap deteriorates persistently from the baseline.
Keep the fallback firmware and the device reachable for USB recovery. A single
radio CRC or 3-of-6 rejection does not automatically imply a regression.

**Current evidence:** real logs show successful legacy sensor updates and
shadow equality for numeric `fhkvdataiii`/`mkradio4` fields. They do not
establish independent XMQ parsing provenance, date-field parity, or a completed
on-device hourly renewal. CI fixture compilation is not the same as verifying
the private 12-meter firmware.
