# Stage B gate status — full-driver acceptance not yet complete

Updated 2026-11-06 for branch `experimental/xmq-multidriver-auto`.
This is an evidence ledger, not a release approval. The user confirmed CI green
through `b11ebde` (including the portable host restart-checkpoint contract).
The new real ESP-IDF NVS backend and host fake-NVS tests are **CI pending**.

## Latest verified checkpoint — 2026-11-06

Owner-confirmed GitHub Actions green through `7e89dec` on
`experimental/xmq-multidriver-auto`: all **177 positive** upstream
JSON functional field occurrences have independent partial compiled-host
comparisons; four fields in invalid authenticated `kamwater #9` remain
correctly rejected. Executed Linux mbedTLS original #9/#10 cryptographic
regressions, Kamwater original #10 four-field host synthetic-DLL replay,
Apator162 #19 original CBC sample with synthetic format-B DLL/CRC,
and Minomess #1/#2 *wired M-Bus* original JSON and configured-ID gate
are green. No claim of an authentic full wireless RF fixture or
execution of ESP-IDF `app_main` on hardware.

**Laboratory source-field comparison substage CLOSED; Stage B
full-driver production admission OPEN**. The unchanged parity ledger
correctly reports 0/39 complete production-equivalent replays and
0/3 approved live batch-2 drivers. Previous sections below preserve
historical intermediate checkpoint descriptions and their old
"CI pending" wording; this latest checkpoint supersedes those
dated statements.

## Executable mbedTLS-to-fields integration — CI pending

Commit `8449b62` extends the already-executed Linux host mbedTLS test
with the **same shared native C++ Kamwater DIF/VIF record and numeric
decoders** used in earlier independently scoped field tests. In one
authenticated source #10 run it now compares `total_m3=0`,
`target_m3=0`, `status=TEMPORARY_ERROR` and
`accoustic_noise=FFFFFFFFFFFF`. Source #9 and corrupted tag/key/ID
remain negative. This closes a concrete Linux **mbedTLS → four fields**
integration gap, pending a new CI result; it does not include verified
wireless DLL RX, production ESP-IDF execution, actual HA sensors, or
live Kamwater admission.

## Executed host DLL-to-mbedTLS configured Kamwater replay — CI pending

Commit `59125a9` extends the existing executable mbedTLS host regression
by composing the **actual shared** format-B complete DLL/CRC verifier,
independent four-field configured DLL identity filter (manufacturer, BCD
ID, version and device type), ELL-I CI/CC/access-envelope structural
checks, the unchanged public Kamwater #10 encrypted TPL, and the same
mbedTLS CCM decoder compiled for the ESP-IDF probe. On success it checks
the exact authenticated application, verified TPL status and counter;
the executable also verifies original #10's four functional JSON fields.
It rejects a damaged CRC, an identity change despite recalculated CRC,
a wrong ELL CI and original #9's bad CCM tag, clearing previous output.
This **new combined test has not yet been CI-confirmed**.

The format-B DLL CRC is artificially reconstructed around the original
public fixture. This is **not an on-air recorded RF frame**, not ESP32-S3
execution, and not live sensor publication. All three batch-2 drivers
remain pending; full Stage B remains OPEN.

## Reusable configured DLL-to-mbedTLS ingress — CI pending

The previous host-only DLL verification / configured identity / CCM
composition is now factored into
`experimental/xmq/espidf_cpp_probe/main/oms_ccm_verified_ingress.h`,
with a reusable narrow `decode_format_b` API and explicit statuses.
The executed Linux mbedTLS source #9/#10 test calls **this exact API**,
rather than its former test-local lambda, and verifies rejection of a
different independently configured ID with no stale output. Full DLL
CRC, manufacturer/ID/version/medium, reviewed ELL-I envelope, and CCM
tag are all required before returning authenticated bytes.

**No sensor dispatch or publication is added**, and the original
`BATCH2_PENDING` manifest is unchanged. This host-side ingress has no
replay/duplicate policy, key provisioning, actual ESP32-S3 runtime
execution, nor recorded wireless RF evidence. CI for this new factoring
is pending. It must not be described as an enabled live Kamwater driver.

## Transactional configured Kamwater field mapping — CI pending

New experimental `espidf_cpp_probe/main/kamwater_configured_fields.h`
composes the reusable configured format-B DLL + AES-CCM mbedTLS ingress,
the existing bounded Kamwater DIF/VIF measurement parser, the opaque
acoustic diagnostic parser, and the reviewed original #10 status mapping.
The result is a single all-or-nothing *in-memory* structure containing
`total_m3`, `target_m3`, `status`, `accoustic_noise` and an
authenticated transport counter. No intermediate values are published;
failed frame/CCM/identity/application validation clears the entire
result. The executed Linux mbedTLS fixture test compares all four
positive #10 fields and checks no stale mapped output after original #9,
a corrupted DLL, unsupported CI or mismatched configured ID. **Pending
GitHub CI confirmation**.

This is an **adapter boundary only**. The file has no Home Assistant
sensors, no radio dispatcher hook, and no storage/replay policy. The
counter is diagnostic only. Do not enable Kamwater, Minomess or
Apator162 in the live manifest on these results.

## Native field-binding staging contract — CI pending

An isolated probe-only `kamwater_sensor_mapping.h` now stages only the
existing native binding names `total_m3`, `target_m3` (numeric,
cubic metres) and `status` (text) **after** the configured authenticated
Kamwater #10 decoder succeeded. The staged result is transactional:
missing/negative/nonfinite volumes or absent status yield an empty plan;
no partial measurements are available. `accoustic_noise` and the CCM
counter are intentionally **not** sensor fields, and the plan carries
no RSSI. The probe adds executable assertions for source #10 field
names/values and invalid mapping rejection. **CI pending.**

This is a synchronous in-memory binding plan, **not publication**: it
does not call `publish_state`, change YAML schemas, enable the Kamwater
driver, or alter existing `fhkvdataiii`/`mkradio4` behavior. True
sensor lifecycle, replay/duplicate suppression, real T1/C1 RF capture
and ESP32-S3 runtime remain separate Stage B acceptance gates.

## Per-meter authenticated CCM counter gate (host-only; CI pending)

The experimental `kamwater_replay_gate.h` now stages the **complete**
configured Kamwater sensor plan only after successful shared DLL/identity
admission, mbedTLS CCM authentication, and transactional four-field
decode/map. It remembers the greatest admitted authenticated counter
per configured manufacturer+ID+version+medium and rejects the same or
lower counter without exposing a plan. Invalid frames and wrong identities
neither advance the counter nor preserve stale outputs. Counter state
cannot silently be reused for a different configured meter.

An executed Linux host regression now exercises original authenticated
Kamwater #10 once, rejects its identical replay, rejects the original
bad CCM-tag #9 and DLL corruption without changing the state, and checks
strict older/newer/wrap-boundary behavior with **synthetic state values**.
The staging plan now **owns its text value** instead of retaining a
temporary Reading's `c_str()` pointer.

**CI for this increment is pending.** This is RAM-only single-threaded
host/probe logic: no NVS persistence across reboot, no device reset or
counter-wrap recovery, no production key provisioning, no live RX hook
or Home Assistant `publish_state` call. A counter accepted in this
prototype means **staged in memory**, not delivered to HA. True
cross-reboot anti-replay and exactly-once publication remain OPEN
production acceptance requirements. No `BATCH2_PENDING` driver is enabled.

## Delivery-confirmed replay-counter contract — CI pending

**Latest owner-confirmed green:** through `d0bb0d7` (Kamwater
authenticated host replay, transactional mapping, single-meter monotonic
counter checks). The new code in `kamwater_replay_gate.h` corrects an
important lifecycle limitation of the previous `stage_once` prototype:
previously the counter advanced upon staging a plan in memory, **before**
any delivery confirmation.

The interface now uses `prepare` (authenticate DLL/TPL, configured
identity, decode and build owned sensor fields **without mutating** replay
state), `discard`, and `confirm_delivery` (advance the counter only
after a synchronous downstream sink reports success). The host-only
`deliver_once` composes these calls. It does not call real ESPHome
`publish_state`.

The executable mbedTLS regression now tests original Kamwater #10 with a
**simulated failing sink** (counter unchanged, identical telegram can
retry), followed by a successful sink (counter committed); then
duplicates, source #9 bad authentication, changed DLL CRC, and mismatched
configured identity are rejected without invoking the sink. Synthetic
state-boundary tests cover stale/wrapped counters and rechecking two
pending candidates at confirmation time. **New CI is pending.**

**Limits:** this does not make separate Home Assistant entity updates
atomic or guarantee exactly-once delivery. A sink can partially update
entities then report failure, and crash/reboot still loses the RAM
counter. Any production integration must define persistence,
key-rotation/reset semantics, bounded per-meter state, synchronous
delivery/failure behavior and real RF validation before enabling
`BATCH2_PENDING`.

## Restart-safe checkpoint *contract* for CCM counters — CI pending

The new isolated host/ESP-IDF-probe-only
`experimental/xmq/espidf_cpp_probe/main/kamwater_replay_checkpoint.h`
defines a canonical 25-byte checkpoint: magic/format version, verified
configured meter identity (manufacturer, ID, version, medium), **external
nonzero key epoch**, last accepted authenticated CCM counter and CRC32.
The CRC is **accidental-damage detection, not authentication, anti-rollback
or a substitute for protected storage**.

Restoration is fail-closed: missing, truncated, corrupted, wrong-version,
wrong-meter or wrong-key-epoch checkpoint leaves the session unready.
There is deliberately **no automatic watermark reset** after a missing
or corrupt checkpoint. A production implementation would require an
explicitly authorized provisioning/recovery policy and a trusted
storage backend (e.g. validated NVS handling), neither is introduced
here.

A caller-provided synchronous delivery callback must first report
success; a caller-provided **atomic/durable** checkpoint writer must
then report success before the in-memory watermark advances. If the
writer fails, the session is blocked until a trusted recovery. The
new Linux test exercises original authenticated #10, bad-tag #9,
simulated delivery failure, simulated restore after restart, duplicate
suppression, incorrect epoch/identity, corrupt/truncated checkpoint,
reentrant callback rejection and a failed checkpoint writer. These
are **mock storage tests**; new CI not yet confirmed.

**Inherent limitation:** sink side effects may occur before durable
write. A crash in that interval can lead to repeated idempotent
Home Assistant sensor updates after restart; this design cannot
guarantee exactly-once delivery. The writer's atomicity/durability and
security against malicious rollback are assumptions **not validated**
by CI. No ESP32-S3 NVS, real RX hook, live YAML, driver promotion or
`publish_state` change has been made.

## Real ESP-IDF NVS adapter, inert in firmware — CI pending

The experimental `espidf_cpp_probe/main/kamwater_nvs_checkpoint.cpp`
uses actual ESP-IDF `nvs_open`, `nvs_get_blob`, `nvs_set_blob` and
`nvs_commit`, compiled in the `esp32s3` ESP-IDF probe target with the
real `nvs_flash` component. `app_main` references only a harmless
link anchor (`backend_name`); **no NVS initialization, reading, writing,
auto-erasure, or provisioning is executed by this probe**.

The adapter:
- derives a bounded per-meter NVS key from manufacturer and eight-digit
  configured ID, with version/medium/externally provided nonzero epoch
  verified by the 25-byte checkpoint decoder;
- reads only existing records and fails closed on missing/corrupt/wrong
  identity/epoch checkpoints; **never auto-creates a replacement**;
- accepts a write only for a correctly encoded, strictly higher CCM
  watermark than the already persisted value; `nvs_commit` must succeed
  and a separate NVS readback must match before success is returned;
- never calls `nvs_flash_init` or `nvs_flash_erase`: a future explicitly
  authorized application must supply independently verified NVS setup,
  initial provisioning, isolation and recovery.

A new Linux **fake NVS** test compiles the *same* adapter implementation
against test-only ESP-IDF API shims. It exercises missing state,
separately provisioned initial state, meter and key epoch mismatch, CRC
corruption/truncation, stale/repeated watermark, successful update,
failed `set_blob`, failed `commit`, failed read/open, and corrupted
readback. Mock correctness **does not** prove power-loss behavior of
real NVS flash. CI for the new code and ESP-IDF compile/link is pending.

**Critical deployment blockers:** frequent per-telegram NVS writes can
wear flash; no write throttling or independent replay-safe low-wear
storage policy has been approved. CRC32 does not detect malicious
rollback; NVS encryption/anti-rollback, trusted epoch management,
atomic HA delivery, real ESP32-S3 runtime and receiver integration
are NOT proven. No batch-2 driver admission or installed YAML changes.

## Source-fixture parity: narrow substage ready for CI

Original public `wmbusmeters` XMQ fixtures from the unchanged imported
`minomess.xmq` (6), `kamwater.xmq` (14) and `apator162.xmq` (19):

| Evidence | Count | Interpretation |
| --- | ---: | --- |
| Original cases | 39 | 38 with positive *partial* field comparisons; 1 negative-security-only |
| Original functional JSON field occurrences | 181 | Excludes names, IDs, timestamps and deprecation notes |
| Positively comparable fields | 177 | Tested using existing native C++ application semantics and host crypto experiments, **only if this CI passes** |
| Remaining source fields | 4 | All are from original `kamwater #9`, an intentionally BAD CCM tag; **must not** become positive measurements |
| Full production-equivalent frame-to-sensor parity | **0/39 approved** | The source-field tests are separately scoped, not an end-to-end replay |
| Batch-2 live drivers enabled | **0/3** | `minomess`, `kamwater`, `apator162` remain `BATCH2_PENDING` |

For `kamwater #10`, the valid original CCM tag authenticates the
application, and the verified short-TPL byte `0x10` has the
source-exact `TEMPORARY_ERROR` interpretation. Unsupported statuses
are rejected, and a wrong key, changed tag, incorrect identity, or
malformed frame must not return stale readings. This host-only mapping
is **not** general support for every TPL status bit combination.

For `kamwater #9`, expected source JSON contains
`TEMPORARY_ERROR FAILED_DECODE` and values, but the CCM tag is
corrupt. The secure decoder must reject the frame and expose **no
measurement fields**. Its four nominal JSON measurement/status
fields are deliberately excluded from positive decoded-field counts.

`test_batch2_field_gap_audit.py` and `test_batch2_parity_ledger.py`
track both classes without promoting the latter to a PASS. Running
the ledger alone proves only consistency of test coverage metadata.
The independently compiled C++ host parity and cryptographic tests
must also be green.

## ESP-IDF mbedTLS initial isolated build probe (CI pending)

The native **ESP-IDF test application only** now contains
`experimental/xmq/espidf_cpp_probe/main/oms_ccm_mbedtls_probe.h`.
It implements the reviewed OMS short TPL 0x7A / security mode 10 / KDF1
subset with ESP-IDF mbedTLS CMAC-derived key and authenticated AES-CCM
decryption, modeled on the existing isolated host OpenSSL test backend.
It clears output on errors and never admits partial plaintext. ESP-IDF
CMake explicitly lists `mbedtls`. The probe translation unit includes
this header and builds a synthetic invalid-tag exercise.

**Evidence limitation:** a GitHub CI build proves compilation/linkage,
**not runtime execution** of `app_main`, nor equivalence on the public
valid #10 encrypted fixture. The attempted large source-fixture insertion
into the ESP-IDF application was not applied. The original public #9
bad-tag / #10 valid-tag fixture tests are green **on host OpenSSL only**;
a dedicated ESP-IDF on-device equivalent remains necessary.

No active `components/wmbus_native_sx126x` source includes this
probe-only header. No radio dispatcher or firmware sensor is connected
to its output, and no key or plaintext is logged. Host-only OpenSSL
crypto remains host-only. **Not production cryptography approval.**

## Public original CCM parity compiled into ESP-IDF probe (CI pending)

The isolated ESP-IDF `app_main` now includes the unmodified public
Kamwater source #9/#10 encrypted TPL test vectors in
`espidf_cpp_probe/main/kamwater_ccm_original_fixtures.h`.
The mbedTLS code must return the exact 38-byte original #10
plaintext, `tpl_status=0x10` and `counter=0x000E578C`
with the original **public upstream fixture key**.
It must then reject source #9's invalid CCM tag and clear all
previous plaintext, status and counter output. The fixture parser
checks exact expected byte lengths.

**CI for this follow-up is pending.** The ESP-IDF job builds/links
the test application; it does **not execute `app_main` on a device**.
A successful compile cannot be reported as positive/negative
cryptographic runtime parity until test execution is independently
observed on ESP-IDF hardware (or an equivalent validated target
runner). Neither these public fixtures nor the mbedTLS test
implementation are wired to live radio/sensor publication.

## Latest ESP-IDF probe milestone (owner-confirmed build green)

The owner confirmed green CI after adding original public
`kamwater #9/#10` valid/invalid mbedTLS vectors to the isolated
ESP-IDF test application. Their assertions are compiled/linked but
**not run on the Heltec** by GitHub Actions.

A subsequent, **CI-pending** negative-test increment also checks:
incorrect configured meter identity, incorrect fixture key, and a
one-byte changed CCM authentication tag. Each case must return
`BadTag` and leave no clear application, status or previous
counter. The structural guard checks those cases stay included.

This still is not an ESP-IDF *executed* positive fixture test,
a live native receiver admission path, or permission to promote
`minomess`, `kamwater`, `apator162`.

## Executed mbedTLS host milestone — owner-confirmed green

The owner confirmed GitHub Actions green for commit `fb6044d`. The
separately compiled and **executed** host regression
`experimental/xmq/espidf_cpp_probe/test_kamwater_ccm_mbedtls_host.cpp`
uses the same mbedTLS probe header compiled by ESP-IDF. It verifies the
original authenticated #10 plaintext byte-for-byte, rejects original
bad-tag #9, and checks wrong identity, wrong key and changed tag with
fail-closed output. This advances evidence beyond OpenSSL-only and
ESP-IDF compile/link-only tests.

A subsequent host negative-input increment (commit `8a7977f`,
**CI not yet owner-confirmed**) additionally checks malformed/unsupported
TPL header, mode, partial encryption, KDF, missing key, short payload
and null input, always after a successful decode to detect stale output.
These are still host executions, **not** real ESP32-S3 `app_main`
execution or complete frame-to-sensor replay.

The original 177/181 positive partial source-field comparisons are
owner-confirmed green; the remaining four fields belong solely to
invalid authenticated Kamwater #9 and must remain negative-only.
The **source-field coverage substage is complete**. Full Stage B remains
**OPEN**: all 39 production-equivalent replay approvals and all three
batch-2 live-driver admissions remain blocked.

## Native publication isolation — CI pending

The existing host test `test_kamwater_verified_dll_ingress.cpp` now
explicitly calls the **actual native** `wmbus_pure::publication::inspect`
after a successful synthetic Kamwater format-B DLL + configured identity +
clear ELL II + TPL79 + record decode. It asserts that the real live gate
returns `UnsupportedTransport` for CI `0x8D`; a different configured
meter ID returns `InvalidFrame`. This prevents a host-only success from
being mistaken for live sensor admission. The separate stale-output
negative test remains in place.

This is a **boundary regression, not live driver parity or sensor lifecycle
validation**. The stage remains OPEN until verified original-frame replay,
transport security, actual configured dispatcher and transactional sensor
mapping are independently demonstrated. No `BATCH2_PENDING` driver may
be enabled on this evidence alone.

## Minomess independently configured wired M-Bus admission — CI pending

Original public Minomess #1/#2 are complete **wired M-Bus** 0x68 envelopes,
not wireless T1/C1 DLLs. Their existing C++ decoder already checked length,
SUM checksum, structural type and all 13 expected upstream JSON fields.
The new `wired::decode_configured` gate additionally requires the caller's
**independently configured eight-digit BCD meter ID**, including leading
zeroes, before returning any reading; all failure paths clear the output.
The fixture CLI and original-JSON parity test exercise the matching IDs,
wrong IDs, malformed ID lengths and a modified wired ID with a recomputed
valid SUM checksum. **CI for this increment is pending**. Gateway RSSI and
actuality remain gateway metadata, never evidence of radio RSSI.

Earlier Apator162 public #19 AES-CBC fixture was owner-confirmed green
after its synthetic format-B DLL/CRC wrapper, exact configured identity
checks and original `total_m3=7.512` parity. This is not a recorded
on-air frame; AES-CBC mode 5 is **not authenticated**. Kamwater #10
encrypted host replay and #9 bad-tag rejection were also owner-confirmed
green after fixing optional metadata handling in the synthetic regression.

These advances do **not** change the 0/39 production-equivalent full
RF-to-sensor approvals or the 0/3 active batch-2 drivers.

## Strict remaining Stage B acceptance gates

1. **Per-driver production-equivalent replay:** verify complete T1/C1
   wireless frames (and wired M-Bus independently where applicable),
   including frame format, full packet CRC, meter manufacturer/version/
   medium/ID and configured link mode. Compare field values, dates,
   status, negative rejection and freshness to the original XMQ JSON.
2. **Cryptography in actual ESP-IDF:** implement and test vetted AES
   modes/KDF where needed, including verified CCM tags, ELL/TPL
   bounds, compact data profiles, identity/provenance and replay/duplicate
   behavior. OpenSSL tests alone do not establish an ESP-IDF implementation.
   CRC or the CBC plaintext marker is **not authentication**.
3. **Live sensor lifecycle:** configured native dispatcher must publish
   only supported finite measurements on success, with exact field and
   unit mapping, no partial/stale publish after failure, no entity
   creation for unconfigured IDs and no disruption to established
   `fhkvdataiii`/`mkradio4` receivers.
4. **Real Heltec canary and rollback:** separately authorized build and
   flash, then log/HA sensor parity, RSSI, stability, resource use and
   sustained T1/C1 reception. No firmware change is authorized by
   the current host fixture tests.

**Disposition:** the source-field comparison *substage* can be marked
complete after CI; **Stage B itself remains OPEN** until the full-driver
gates above are satisfied. Never promote the three pending drivers
because of 177/181 alone.
