# Project context — `experimental/xmq-multidriver-auto`

> **Branch-specific context, updated 2026-11-07.** This section applies **only to `experimental/xmq-multidriver-auto`**. No default branch, legacy experimental branch, local YAML, hardware firmware or server configuration is changed by this documentation update. The older 2026-11-01 `xmq-independent-pipeline` notes are preserved below as historical context and **must not be interpreted as the active architecture on this branch**.

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
- Source of truth for driver availability: `experimental/xmq/tools/native_driver_specs.py`. `BATCH2_PENDING = ("minomess", "kamwater", "apator162")` remains excluded from `SUPPORTED` and AUTO. Those three can now be **manually selected in experimental `meter_drivers` for strictly unencrypted RF** or in `secure_meters` for AES with explicitly provided key. Neither path is production-authorized before real radio/HA validation.
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

## Stage B manual NOKEY / clear RF path — 2026-11-07 (CI GREEN; hardware gate OPEN)

**Latest verified run `42378059859` at `f6fa1dca` is SUCCESS:
all three jobs PASS** (`native-packet-input`,
`stage-a-espidf-s3-portable-link`, `native-esphome-build`).
The keyless ESPHome YAML config and complete ESP32-S3 compile,
as well as the separate keyed build, passed. Eight preceding
intermediate workflow runs (`42377687000` through `42377967849`)
remain historically RED because the Stage A static guard still
expected the old Kamwater compact-only
`decode_clear_ell_ii_tpl79(` call instead of the now-shared
`decode_clear_ell_ii(` wrapper. Commit `f6fa1dca` updated
the guard while preserving ELL/CRC/transport checks. Do NOT re-run
or treat old red commit statuses as failures of current code.
The post-green changes to this context are docs-only.

- The `meter_drivers` YAML now accepts explicit `minomess`,
  `kamwater`, `apator162` **without any `key`**, while the
  generated `SUPPORTED` / `auto` list remains unchanged.
  Optional `dll_meter_id` for Minomess lets the configured DLL
  converter sender differ from the actual TPL meter ID. Duplicate,
  ambiguous and mixed-key/plain bindings are forbidden.
- Actual verified T1 and C1 callbacks now route explicit clear
  batch-2 readings to `xmq_batch2_plain_dispatch.h` before the
  existing generic plaintext gate. The clear path matches DLL sender,
  checks Minomess long TPL identity and **TPL security mode 0**, uses
  existing strict Apator plaintext admission, and Kamwater checks
  ELL-II mode 0 + independent payload CRC for clear full `78`,
  compact `79` or short `7A` applications. CRC, model, frame
  shape or security failures do not publish readings, RSSI or fall
  back to AUTO/encrypted dispatch. C1 retains its existing
  `c1_plaintext_publish: true` opt-in.
- Host tests cover Minomess converter and cipher-mode-5 rejection,
  Kamwater clear full/compact and bad ELL CRC/AES-CTR refusal,
  and Apator short plaintext and mode-5 refusal. A **separate
  build-only keyless ESP32-S3 YAML** confirms all 3 can compile
  with `meter_drivers`, `xmq_sensors`, no AES keys and no
  `secure_meters`: `experimental/semtech-radio/ci/native-sx126x-batch2-nokey-lab.yaml`.

**Important:** original upstream `NOKEY` samples may be
post-decrypted applications rather than actual unencrypted on-air
telegrams. In particular, original Minomess examples carry TPL
Mode 5 despite `NOKEY`; never accept that header as plaintext
without a verified keyed decryptor. Some Kamwater compact profiles
reference a previously validated full telegram and cannot be
trusted without that reference. This manual no-key path authorizes
only independently VERIFIED clear RF; it does not magically
decode encrypted radio without a key. The on-device proof and
real HA publication gate remain open; no flash was performed.

## Stage B passive automatic identification — 2026-11-07 (CI pending)

**Owner decision:** keep `minomess`, `kamwater`, `apator162`
**manually configured only for actual decoding**, but identify all
three automatically in CRC-verified T1/C1 traffic, even for unknown
`configured_meter_ids`. No new YAML setting or real-meter ID/key
is necessary for passive recognition. Existing production Techem
`auto`, explicit `meter_drivers`, `secure_meters`, sensors and
publication permissions are untouched.

Implementation:
- `components/wmbus_native_sx126x/batch2_auto_identify.h`:
  read-only MVT metadata recognition using the FULL generated
  original XMQ registry. Prefer meter identity from long TPL;
  otherwise ELL address if available, then DLL. Original
  Minomess `ZRI/00/07`, `ZRI/01/16`, `ZRI/01/06`;
  Kamwater `KAM`/`KAW`; Apator162 `APA/05/06` and
  `APA/05/07` are recognized; ambiguous/conflicting driver
  signatures never produce a definitive candidate. No payload
  decryption, metadata-based admission, or record parsing.
- `NativeReceiverProbe::log_batch2_auto_` is called only after
  the existing DLL/CRC verification on **T1, standalone C1 and
  combined-mode C1**. For one candidate it emits
  `BATCH2 AUTO candidate=<driver> source=TPL|ELL|DLL link=T1|C1-A|C1-B dll=... meter=... different_dll=... tpl_security=... (metadata only; no decode or publication)`.
  A fixed ring cache of 32 observed candidates prevents per-frame
  log spam, with no unbounded allocations or per-meter state.
- The new host regression
  `experimental/semtech-radio/test_batch2_auto_identification.cpp`
  compiles with a freshly generated registry made from actual
  `experimental/xmq/drivers/*.xmq` and tests all original
  signatures, a distinct Minomess converter DLL/TPL, ELL
  origin, collisions, ordinary Techem exclusion and malformed
  headers. A CI job also recompiles ESPHome/ESP32-S3.
- No changes to the generated `SUPPORTED` driver manifest,
  to decode `auto`, to `c1_plaintext_publish`, to active YAML,
  firmware installed on Heltec, or to HA sensor bindings.
  **An auto candidate is NOT a decoded/validated meter.** No
  actual Minomess/Kamwater/Apator162 RF packet was observed yet.

CI for this change is **not yet declared green**; initial test
command lacked the native component include directory. Fixed in
commit `b9103a7`, GitHub run `42379840181`, where
`native-packet-input` job passed. Wait for the remaining
ESP32-S3 build jobs before claiming all CI green.

## New manual clear driver — Lansen Pulse Counter (`lansenpu`), 2026-11-07

**Owner requested a simpler meter driver because no Minomess/Kamwater/
Apator162 devices are receivable locally.** Selected the Lansen
two-channel pulse counter for its three original upstream `NOKEY`
telegrams (A/B pulses and TPL status; no AES in the actual source
headers). It is a pulse counter, **not intrinsically a water meter**;
do not label its values m³. Imported original
`wmbusmeters/wmbusmeters:drivers/src/lansenpu.xmq`, upstream file
blob SHA `67d5e60247c7bce0cc288aa09d5449c912824c26` with
original GPL notice intact; original source public IDs only in tests.

`components/wmbus_native_sx126x/lansenpu_plain.h` is a strict,
independent native implementation: LAS/14/00, LAS/1B/00, LAS/0B/02,
short TPL 7A with verified security mode 0, exact
`0E FD 3A` counter A and `8E 40 FD 3A` counter B records,
six-byte/12-decimal BCD, no guessing, no repeated fields,
transactional no-stale-values, original TPL status
`OK`, `POWER_LOW`, and
`PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE` only.
`a_counter` and `b_counter` are **pulse counts**; optional
`status` is a separate text_sensor field (64-byte bounded text).

New `MANUAL_PLAIN = ("lansenpu",)` in
`experimental/xmq/tools/native_driver_specs.py` is intentionally
**excluded from `SUPPORTED` and the decoder AUTO manifest**.
ESPHome `meter_drivers` accepts `lansenpu` without AES key and
registers only its explicit numeric DLL ID. The existing CRC-verified
T1 and C1 callbacks already use `publish_plain_batch2_`, now also
recognizing the explicit Lansen binding and publishing numeric
values, TPL status and RSSI only after complete validation.
All existing Techem and the three previous batch-2 drivers stay
unchanged. `c1_plaintext_publish` remains opt-in.

`experimental/semtech-radio/test_lansenpu_native_no_key.cpp`
runs the three upstream examples expecting
A=4711, B=1234, status=POWER_LOW;
B=870, status=OK; and
B=870, status=PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE.
It independently adds and verifies synthetic DLL format-B CRC
around the public CRC-stripped source telegrams and rejects
invalid ID, AES mode, BCD, unknown TPL status/model, duplicate,
truncated, and malformed frames. A build-only no-key ESP32-S3
YAML fixture includes the two numeric and one text Lansen sensors.

**Latest CI run at code revision `3ade338c`: `42381796061`.**
The `native-packet-input` job is PASS; at time of writing
the standalone ESP-IDF/ESP32-S3 and full ESPHome build are
still IN PROGRESS. Earlier intermediate RED workflows resulted
from an incorrectly updated manifest length guard;
`3ade338c` fixed that. Do NOT call all CI green until the
two remaining jobs finish successfully. No real physical Lansen
telegram/HA reading was observed; no Heltec flash was performed.

## Strict configured AUTO for all four imported drivers — 2026-11-07 (ALL CI GREEN)

Owner authorized enabling **configured-only** `driver: auto` for
`lansenpu`, `minomess`, `kamwater`, `apator162` on Heltec.
The latter three remain excluded from generic `SUPPORTED`;
there is NO change to the existing Techem `fhkvdataiii`/
`mkradio4` AUTO implementation.

Native `xmq_batch2_plain_dispatch.h` selects a candidate using
the COMPLETE original MVT registry, verified DLL sender ID,
unambiguous nested TPL/ELL identity, and exact configured meter
ID. Minomess requires an identified long TPL meter; an optional
explicit `dll_meter_id` may differ from `meter_id` even when
`driver: auto`. Kamwater and Apator162 require an identified DLL
meter matching the configured ID. Any ambiguity/mismatch fails
closed. A chosen model is not proof of plaintext: the unchanged
strict per-driver decoder checks actual TPL/ELL security, CRC,
records and outputs before publishing. The encrypted path still
uses `secure_meters` with explicit driver and configured AES key.
No guessing keys, AES-to-plaintext fallback, phantom RSSI or
unknown-meter sensor publication. Passive read-only recognition
of all four continues for unconfigured verified T1/C1.

CI includes host tests for all four `auto` candidates and
negative tests for wrong ID, converter mismatch, MVT collision,
Minomess Mode-5 and encrypted Kamwater/Apator; compile-only Heltec
T1/C1 test YAML maps four `driver: auto` entries with synthetic
IDs, without private meters/secrets. **Physical RF-to-HA
validation unavailable** for these models; do not flash the
installed Heltec based on CI alone.

**GitHub Actions run `42388899090` at `a4894ee185ae6c9fb0608489c92283d0b351fc28`
ALL GREEN:** `native-packet-input`, `stage-a-espidf-s3-portable-link`,
`native-esphome-build`. The latter compiled actual Heltec
T1/C1 combined laboratory YAML with synthetic `auto` mappings
for all four drivers, in addition to encrypted and no-key ESP32-S3
CI builds. No actual meter, key or physical RF-to-HA validation
was available and no installed firmware was changed.
The subsequent context edits are docs-only.

## Next two native keyless profiles — 2026-11-07 (CI pending)

Owner requested one or two additional simple drivers. Verified
upstream `wmbusmeters/wmbusmeters` original XMQ files and their
complete telegram headers; `lansenth` showed Mode-5 despite
`NOKEY`, thus was NOT admitted to the clear receiver.

Selected `lansendw` (two original clear TPL7A Mode-0 examples,
CLOSED/OPEN, event A/B counters and `error_flags`) and
`cma12w` (two original clear TPL7A Mode-0 examples,
temperature, one-hour average, battery and software version).
Independent native strict decoders live under
`components/wmbus_native_sx126x/*_plain.h`, with source XMQ
copyright notices preserved. They are included in portable
`SUPPORTED` / AUTO, independently requiring configured ID,
verified DLL CRC, exact version/manufacturer/type, transport
security Mode-0 and documented record layouts. Bad/unknown
records and security mode reject without HA publication.

Native AUTO uses the same configured no-key handler already
proven for Lansen PU; no RF stack, old legacy driver, AES key,
new external component or unknown-meter publication is added.
Status/battery/software fields are mapped only after a successful
numeric decode. Build-only Heltec YAML includes public original
fixture IDs and sensor bindings. This is GitHub-only integration,
NOT physical RF validation. CI pending, no hardware changes.

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
