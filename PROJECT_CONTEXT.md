# Project context — `experimental/xmq-multidriver-auto`

> **Branch-specific context, updated 2026-11-06.** This section applies **only to `experimental/xmq-multidriver-auto`**. No default branch, legacy experimental branch, local YAML, hardware firmware or server configuration is changed by this documentation update. The older 2026-11-01 `xmq-independent-pipeline` notes are preserved below as historical context and **must not be interpreted as the active architecture on this branch**.

## Read-first instructions for development assistants (including Codex)

1. At the start of each new development session, **read this branch's `PROJECT_CONTEXT.md`**, `experimental/xmq/BATCH2_STATUS.md`, `experimental/xmq/STAGE_A_CLOSEOUT.md`, `experimental/xmq/tools/native_driver_specs.py`, and the relevant tests/code before editing. If instructions in historical sections conflict with current branch code, **verify code/CI and prefer the branch-specific section**, explaining discrepancies.
2. Work **only** on `experimental/xmq-multidriver-auto` unless the owner expressly requests a different target. No merge to `main`/`master`, no modification of other branches, no forced history rewrites and no production Heltec OTA/flash without explicit authorization.
3. Preserve **our native C++ receiver and algorithm** as the implementation. Use the *original* `wmbusmeters` XMQ definitions and public fixture JSON as an **independent compatibility reference**, not as authorization to revive the old driver/radio stack.
4. Prefer small, auditable commits; include positive **and negative** regression tests; await reported CI green before expanding behavior. Fix CI regressions before moving to the next calup. Never claim CI passed, an `app_main` self-test executed on hardware, or live field validation unless it actually did.
5. Never commit private meter IDs, captured production telegrams, encryption keys, Wi-Fi credentials, private YAML or other identifying telemetry. Synthetic IDs and original public upstream fixtures only.

## Active architecture and working baseline

- Hardware target: Heltec ESP32-S3 / SX1262 / 8 MB; ESPHome 2026.10.1, ESP-IDF 5.5.6 in the experimental build context. **The official ESPHome SX126x receiver is the sole radio owner**, with native T1/C1 handling and a bounded `components/wmbus_native_sx126x` application pipeline.
- Native ingress must verify complete radio/DLL frame format and CRC, independently match the configured meter identity and allowed link mode, enforce valid plaintext/decryption/compact gates, and **only then** decode and publish finite mapped fields. Unconfigured meters can be observed diagnostically but must never auto-publish sensor readings.
- The original live Techem decoding behavior for **`fhkvdataiii` (manufacturer CI A0)** and **`mkradio4` (CI A2)** must remain unchanged. Original generated IXML rule tables and their mapping/sensor semantics remain the production baseline on this research branch. The native manifest also lists cleartext portable `iperl` and `hydrus` as supported; `mkradio4a` is post-decrypt-only, **not** general live RF admission.
- Source of truth for driver availability: `experimental/xmq/tools/native_driver_specs.py`. `BATCH2_PENDING = ("minomess", "kamwater", "apator162")` **must remain excluded from live YAML selection, receiver dispatch, sensor publication and any optimistic fallback** until separate end-to-end gates pass.
- Preserve the existing original XMQ generated decoder and T1/C1 algorithms, per-meter RSSI, temperature accuracy, status/dates, no stale or partial publishing on errors, and visibility of unconfigured telegrams where already supported. Never infer a valid decode solely from a matching manufacturer, signature, data CRC or decrypted-looking bytes.

## Stage A — completed consolidation, CI green (owner-confirmed)

The shared-source C++ application/structural algorithms are located at:

- `experimental/xmq/portable_cpp/apator162_ixml_fields.h` — bounded already-clear Apator162 subset, with host forwarding include.
- `components/wmbus_native_sx126x/kamwater_status_semantics.h` and `kamwater_numeric_semantics.h` — Kamwater states, durations and numeric fields.
- `components/wmbus_native_sx126x/kamwater_tpl_status_semantics.h` — narrow already-clear short TPL7A ERROR semantics.
- `components/wmbus_native_sx126x/minomess_wired_frame.h` — **wired M-Bus** envelope, never a wireless T1/C1 transport decoder.
- `components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h` — keyless ELL AES-CTR IV derivation and big-endian 128-bit counter. Host-only OpenSSL AES-CBC/CTR/CCM decryptors and experimental secure ingress **remain isolated**; the native module is **not** an authenticated decryptor.

See `experimental/xmq/STAGE_A_CLOSEOUT.md` for acceptance details and CI limits. ESP-IDF CI builds/links the portable probe; compiling `app_main` does not mean its assertions ran on physical hardware. No production firmware was flashed for Stage A.

## Stage B — source-field laboratory coverage complete; full-driver admission OPEN

- **Last owner-confirmed green (2026-11-06): 177/177 positive functional JSON fields** across the original 39 upstream fixture cases (`minomess` 6, `kamwater` 14, `apator162` 19). The original JSON contains 181 field occurrences; the other **four** belong to `kamwater #9` with an invalid CCM tag and must remain deliberately rejected, not counted as successful decoded measurements. These are individually demonstrated host-side comparisons, **not 39 full replay passes**.
- Original `kamwater #10`: host OpenSSL secure ingress checks the configured DLL identity and a synthetic format-B CRC envelope around the original authenticated encrypted payload, and compares `total_m3`, `target_m3`, `status=TEMPORARY_ERROR`, `accoustic_noise=FFFFFFFFFFFF`. Public bad-tag #9 and stale-output regressions reject. The same mbedTLS CCM implementation compiled for ESP-IDF has **executed host Linux CI** verifying original #9/#10 and malformed input. This does not execute `app_main` on the Heltec.
- Original `apator162 #19`: AES-CBC mode-5 host decrypt plus original `total_m3=7.512` with a **synthetic** wireless format-B DLL/CRC envelope, identity mismatch and CRC rejection (green). CBC and `2F2F` are not authentication.
- Original `minomess #1/#2`: complete **wired M-Bus**, not wireless T1/C1. All 13 source JSON comparisons and M-Bus length/checksum plus independently configured eight-digit meter BCD identity gate and its negative cases are owner-confirmed green as of latest `7e89dec`. Remaining `minomess #3..#6` are original already-clear/post-decrypted RF application tests, not full secured RF replay.
- The actual native `wmbus_pure::publication::inspect` was tested against a synthetic clear Kamwater ELL/compact frame and **blocks CI=0x8D** (`UnsupportedTransport`) despite successful host-only decoding. No experimental driver is registered as live.
- **Still OPEN — no false promotion:** 0/39 approved production-equivalent complete frame-to-Home-Assistant sensor replays; 0/3 batch-2 drivers admitted. Pending: authentic complete T1/C1 frames and decryption/provenance for all needed modes, strict real identity/link-mode and replay policies, ESP-IDF/runtime execution where necessary, transactional sensor mapping and separately authorized Heltec canary/rollback. See `experimental/xmq/STAGE_B_GATE_STATUS.md` for the precise acceptance gate.
- Keep `BATCH2_PENDING = ("minomess", "kamwater", "apator162")` excluded from `SUPPORTED`, YAML driver selection and native publishing. Do **not** flash, modify installed YAML, or alter established `fhkvdataiii`/`mkradio4` behavior without explicit authorization.

## Stage B native integration — 2026-11-07 (all CI green; device validation OPEN)

**Last owner-confirmed GitHub CI green: `8136a346`, run
`42350691692` (three jobs PASS: native packet input, standalone
ESP-IDF ESP32-S3 probe, and native ESPHome complete build).**
Full build includes the opt-in `secure_meters` configuration
and conditional mbedTLS CCM/CMAC Kconfig enablement.
Earlier native source-fixture and rejection tests remain green.

**The three experimental drivers are CODE-INTEGRATED:**
- `minomess`: OMS long TPL AES-CBC Mode 5, existing native XMQ
  application decoder; optional **explicit** `dll_meter_id` for
  wireless converter DLL identity differing from actual meter TPL
  identity. TPL meter must match configured `meter_id`; optional
  DLL sender must match configured `dll_meter_id`. No AUTO key
  guesses or ambiguous multi-target sender bindings.
- `kamwater`: ELL-II AES-CTR for independently checked compact /
  clear records, plus authenticated OMS profile-D AES-CMAC/CCM
  (original public #10 positive and #9 bad CCM tag rejected).
  The validated status text is published only after CCM acceptance.
- `apator162`: OMS short TPL AES-CBC Mode 5 to native XMQ
  `total_m3`; original public encrypted #19 host regression.
All three use the actual experimental `NativeReceiverProbe` verified
T1/C1 ingress with strict, opt-in YAML `secure_meters`, individual
16-byte AES keys from `!secret`, and sensor/RSSI mapping. Existing
Techem `fhkvdataiii` and `mkradio4` paths remain unchanged.

**Validation boundary:**
- **PASS:** executable source-fixture host tests, positive/negative
  secure dispatcher regressions, all three CI jobs, complete ESP32-S3
  build/link, existing non-secure regressions.
- **NOT TESTED:** runtime on physical Heltec hardware, original
  *complete on-air* T1/C1 capture with each target driver, real
  production key provisioning, or observed Home Assistant state
  publication. The synthetic Minomess converter/CTR fixtures
  and public upstream ciphertext are not substituted for these.
- **Release gate remains OPEN:** 0/3 drivers admitted to
  `SUPPORTED` / AUTO and 0/39 complete production-equivalent
  physical RF-to-HA approvals. A separate explicitly authorized
  test build/flash with rollback and suitable actual meter captures
  is needed before production admission. No NVS work is relevant.
  **Installed Heltec firmware is unchanged.**

**Next action is hardware/runtime validation, not additional driver
framework work.** If the site has no Minomess/Kamwater/Apator162
meters or keys, report that physical end-to-end validation is
unavailable rather than inventing approval. Do not flash, enable
YAML for real meters, or alter stable Techem paths without owner's
explicit authorization. Consult
`experimental/xmq/STAGE_B_GATE_STATUS.md` for historical gates.

## Next developer workflow

1. Verify the latest Stage B CI, including original fixture/field-gap tests and native ESP32-S3 build. Fix failing tests before asserting increased coverage.
2. Distinguish *positive source-field comparisons*, negative security tests and complete-driver end-to-end parity. Continue the remaining security-aware test coverage without forcing `kamwater #9` to pass.
3. Only after complete per-driver integrity, identity, decryption and publication regression tests consider an **explicitly opted-in** canary build, followed by measured Heltec sensor/log parity and an available rollback.
4. Update this context and `experimental/xmq/BATCH2_STATUS.md` when a validated milestone changes; treat older sections below as archive, not current configuration.

---

## Historical context — 2026-11-01, different branch

The following text was originally written for `experimental/xmq-independent-pipeline` and is retained for traceability. It must **not** override the branch-specific state above.

# esphome-sx1262 — current project context

Updated 2026-11-01. Active research branch: `experimental/xmq-independent-pipeline`.
Known-working hardware reference: `experimental/wmbus-pure-xmq`.
Do not change the installed Heltec firmware, rewrite its private YAML, or merge to master during pipeline development.

## Hardware and observed runtime
Heltec ESP32-S3 / SX1262 / 8 MB, ESPHome 2026.10.1, ESP-IDF 5.5.6.
Owner reports approximately 126,000 seconds continuous runtime on the XMQ-only configuration.
Real T1/C1 frames are received. Seven Techem fhkvdataiii heat allocators and five
Techem mkradio4 water meters are decoded and published to Home Assistant.
The 13th physical water meter is a separate suspected radio/commissioning failure;
do not change firmware merely to address its missing readings.

## Current architecture and exact limitation
The firmware has a generated XMQ/IXML subset and `xmq_only` supports only
`fhkvdataiii` (CI A0) and `mkradio4` (CI A2).
`components/wmbus_meter/wmbus_meter.cpp::Meter::handle_frame` still calls
`meter->handleTelegram(...)` before the authoritative XMQ publication path.
Therefore `xmq_only` is NOT yet a legacy-free pipeline, even if XMQ supplies
the published fields. Legacy sources are still required to build.

## Migration acceptance criteria
1. Preserve byte-for-byte the recorded known-good outputs for all 12 meters:
   water total/target, heat current/previous HCA, temperatures, RSSI.
2. Match configured meter ID and link mode independently; reject unknown IDs
   before any sensor publication. Preserve a separate raw frame analyzer path.
3. Correctly validate frame length/format/CRC; process T1 and C1 as specified.
4. For secured frames, handle supported security modes and keys safely;
   never publish ciphertext or stale values after unsuccessful decryption.
5. Decode fhkvdataiii and mkradio4 from validated payload without calling
   legacy `handleTelegram`, and without depending on legacy `Telegram` fields.
6. Run host regression fixtures, ESPHome compile/link CI and real hardware
   soak tests before changing the installed build or enabling other drivers.
7. Preserve the existing private YAML, credentials, meter IDs, and sensor names.
   Never commit secrets or production meter identifiers to GitHub.

## Implementation order
- First, add recorded-frame regression fixtures and tests covering valid
  T1/C1, malformed CRC/length, unknown IDs, and partial/decrypt failures.
- Then split independent frame validation, meter matching, and security
  preparation into a bounded reusable pipeline; keep legacy untouched.
- Then route `xmq_only` through that pipeline behind a separate opt-in
  configuration and verify parity; maintain a rollback to known-good.
- Finally remove legacy build/link requirements for XMQ-only, extend the
  generated driver catalog, refresh documentation and evaluate release.

## Evidence boundaries
A ~35-hour uptime is encouraging evidence for current workload stability,
not proof of every radio/security error path or a fully independent decoder.
Previous `NATIVE_ESPIDF_REPORT.md` covers the earlier native-espidf migration,
not final XMQ independence.
