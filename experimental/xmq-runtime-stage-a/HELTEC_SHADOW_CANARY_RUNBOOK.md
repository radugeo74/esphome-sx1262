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

## Host-only correlation token — not yet deployable

The staged legacy setter callback now attaches both the original legacy
meter-instance pointer and a monotonically increasing *per-thread* successful
frame sequence to an aggregate write report. Host tests reject stale tokens,
mix-ups between two meters, and callbacks for failed header/payload parses.
These tokens are ephemeral correlation aids, **not** meter identifiers, and
a per-thread sequence is not globally unique across tasks or restarts.

This staging is generated into a **copy** of `meters.cc` for host CI. The
live `components/wmbus_common` file and deployed Heltec firmware still
do not emit this evidence. A future integration must explicitly correlate
the report with the active `handleTelegram()` invocation and consume the
token synchronously; caching a pointer or relying on the numeric
snapshot alone is not acceptable. Never report `provenance=proven`
merely because a token or a field mask exists.

## XMQ-only decode readiness (host-only, no sensor writes)

The new `xmq_primary_readiness.h` evaluates **only XMQ's own decoded
`XmqFrameResult`**, without reading existing legacy meter values.
For original upstream proprietary fixture frames, it requires all four
heat numeric fields plus two dates, or both water numeric fields. Unknown
drivers, incomplete field sets, duplicate required names and nonfinite
numeric output fail closed.

This proves a *field-production contract on specific offline fixtures*,
not an OTA-ready replacement: live XMQ still receives already-verified
application bytes from the legacy parser, which currently performs the
decryption. Production radio/T1_C1, meter keys, HA sensor ownership, and
fallback remain unchanged. A new independent transport/decryption adapter
and live end-to-end parity are required before the legacy decoder can be
disabled. Never publish XMQ outputs into HA from this host-only gate.

## XMQ-only verified application decode (host-only candidate API)

`decode_primary_candidate()` in `xmq_primary_readiness.h` now
decodes an already verified/decrypted application through XMQ and emits
a candidate result **only when all caller-required fields are valid**.
The destination is cleared on rejection, preventing a later telegram
from reusing stale sensor values. Offline original heat/water fixture
tests cover complete outputs, malformed payload, absent required
fields, and zero retained fields after failure. The calendar check
requires a real YYYY-MM-DD date including leap-year validity.

The API does **not** yet accept raw radio ciphertext, implement AES
decryption, subscribe to `wmbus_radio`, or publish any Home Assistant
state. It must not be installed as a legacy replacement based only on
host test success. The existing Heltec OTA image remains unchanged.

## Single-pass live XMQ field-production diagnostic — pre-install candidate

The next staged firmware source adds read-only aggregate counters
`xmq_fields_ready`, `xmq_fields_present`, and
`xmq_fields_required` to the existing shadow line. It computes
these from the **same XMQ parser result** already used for the
legacy-shadow comparison; it does not execute a second XMQ parser.
For proprietary `fhkvdataiii` it requires four numeric values and
two valid calendar dates. For `mkradio4` it requires two numeric
values. A failed parse resets the readiness report rather than
reusing a prior telegram's output. The log explicitly includes
`primary_transport=legacy_verified` to prevent mistaking this
for independent decryption or radio dispatch.

The code is not installed until a separate, reviewed Heltec firmware
flash. It must not become the Home Assistant sensor writer yet.
Before any installation confirm the existing full CI, a proven USB
fallback, and a private compile of the exact 12-meter T1_C1 YAML.
Do not upload real configs, AES keys, IDs or telegram bytes.

## Direct T1/C1 radio boundary — host-only prerequisite for legacy removal

`xmq_primary_transport_contract.h` defines a separate input contract for
a future direct radio-to-XMQ adapter. It requires an already DLL-CRC-verified
T1/C1 frame **and** verified cleartext with the correct inner meter MVT and
application encoding. A CRC-valid ciphertext frame, unknown link mode, missing
CRC evidence, empty payload or failed decode is rejected and produces no
retained candidate sensor values. Tests use only public upstream fixture
payloads; test-supplied cleartext metadata is not proof of a live decryptor.

The working `wmbus_radio` already validates T1/C1 DLL CRC before dispatch,
so the SX1262 radio does **not** need replacement. The missing real work is
an independently audited TPL security-mode / AES decryption and application
extraction adapter. Until it supplies the verified-cleartext contract, XMQ
cannot safely replace legacy for encrypted meters. No private AES keys,
meter IDs or payloads should be placed in CI, and this host-only contract
does not change the Heltec image or HA writer.

## TPL security checks before independent AES integration

The host-only direct-radio contract now recognizes TPL security modes 0
(clear), 5 (AES-CBC IV), and 7 (AES-CBC with KDF/MAC); mode 8 and any
unrecognized or manufacturer-specific mode fail closed. In addition
to verified DLL CRC and a cleartext payload, the caller must supply
security evidence consistent with the mode:

- Mode 0: no decryption claimed.
- Mode 5: a completed decrypt operation and verified CBC check bytes.
- Mode 7: a completed decrypt operation, verified CBC check bytes,
  **and** authenticated MAC.

These booleans must be supplied by a separately audited decryptor; they
are **not** cryptographic proof by themselves. Tests deliberately use
public, already-clear fixture payloads plus synthetic evidence flags,
so a passing test does not establish live AES correctness.
Production `wmbus_common` still owns decryption and sensors.

## Experimental TPL5 AES-CBC primitive — HOST ONLY

The host-only `xmq_tpl5_cbc.h` now executes actual AES-128-CBC block
decryption using the repository's existing public-domain tiny-AES source
`components/wmbus_common/aes.cc`, without invoking legacy
`Telegram::parse` or touching Home Assistant sensors. Tests compile and
link that original AES implementation, verify the independent NIST SP
800-38A CBC known-answer vector, and exercise a synthetic two-block
TPL5 payload with CBC `2F2F` check bytes, wrong key, wrong IV and
strict block lengths. This is **not yet the full TPL5 decryptor**:
the supplied 16-byte IV and key are test inputs, not extracted from a
real frame. The caller must still parse the exact TPL header, derive the
correct IV and encrypted-block count, retain any unencrypted tail,
and explicitly handle mode 7's KDF/MAC separately.

CBC check bytes are a sanity check, **not cryptographic
authentication**. The reused tiny-AES code has process-global cipher
state and is not independently audited as thread-safe; never run it
concurrently or claim that this host helper is production-ready.
A future ESP32 integration must review reentrancy, memory/stack usage,
malformed-frame handling and plaintext lifetime before allowing it to
consume radio traffic. No live firmware, meter keys or HA data changed.

## TPL 5 IV and encrypted-prefix handling (host only)

The new `make_tpl5_iv()` and `decrypt_tpl5_application()` in
`xmq_tpl5_cbc.h` mirror the IV construction found in the repository's
`components/wmbus_common/wmbus_utils.cc`:

- 2 bytes of manufacturer M field (little-endian)
- 6 bytes of the matching A field from **TPL when available**, otherwise DLL
- 8 repetitions of the telegram's TPL access number

The caller must establish and validate these fields for the **same received
telegram**; this helper does not parse or authenticate a full TPL header.
TPL configuration bits 4–7 supply a block count, and count zero means
decrypt the entire remaining application (which therefore must be block-
aligned). An explicit count decrypts only that exact prefix; any trailing
unencrypted bytes remain unchanged. Missing blocks, nonaligned encrypted
data and lengths outside the strict bound are rejected, not truncated.

Tests include synthetic encrypted two-block TPL 5 data with a clear tail,
an invalid access byte and inner address, mismatched encrypted-block
counts, and all-block mode. This is not yet an independently verified
end-to-end encrypted meter telegram. Mode 7 KDF/MAC and live radio/TPL
header extraction are still separate work. No changes to current HA
sensor ownership, runtime secrets or Heltec firmware.

## Strict direct DLL to TPL5 header subset — host only

`xmq_tpl5_direct_header.h` now parses an already DLL-CRC-verified,
CRC-stripped frame with an unambiguous direct TPL CI: 0x7A (short TPL,
DLL M/A) or 0x72 (long TPL with INNER M/A). The parser extracts the
TPL ACC, STS, little-endian configuration word, mode-5 block count,
the application offset, and derives the correct IV field inputs. It
rejects invalid lengths, unknown/manufacturer-specific CI, ELL, NWL,
AFL and mode 7 (not yet independently authenticated). The host-only
adapter passes only the strictly isolated encrypted application region
into the previously implemented CBC primitive; explicit clear tails
are preserved. Tests use synthetic frames with deliberately distinct
DLL and inner TPL addresses, and never include production keys/IDs.

**Security correction from Stage A #107:** the earlier test that
assumed a wrong TPL access number must always invalidate CBC `2F2F`
was wrong. CBC IV positions 8–15 change plaintext positions 8–15,
so `2F2F` at the start may remain unchanged. The test now checks the
result differs instead. Such a decryption result is NOT authenticated;
future live mode-5 support must validate expected frame semantics
and provenance. Never use the mere `2F2F` sentinel as proof of
correct access number or a secure MAC.

These tests only exercise direct DLL-to-TPL CI 72/7A, not the full
wM-Bus header chain (ELL, AFL, other CI) or security mode 7. The
SX1262 receiver, Home Assistant sensor state and OTA image have not
been modified. No flash until real-device proven end-to-end parity
and a tested rollback.

## Synthetic end-to-end direct TPL5 to XMQ host fixture

`test_xmq_tpl5_to_xmq.cpp` now uses the public original
`mkradio4` application fixture, prepends the TPL5 CBC `2F2F`
sentinel, pads synthetic AES blocks, encrypts them with a PUBLIC NIST
test key and IV derived from synthetic manufacturer/address/access
fields, then exercises **CI=7A short and CI=72 long** paths:

- DLL frame length and caller-asserted CRC verification boundary
- Direct TPL M/A/ACC and security mode 5 header extraction
- AES-128-CBC decryption of the strict block span
- Explicit removal of test-only `2F2F` and known fixture padding
- `decode_primary_candidate()` for real upstream XMQ `mkradio4`
- Both decoded readings `total_m3=0.4` and `target_m3=0.1`

The complete test does NOT verify live DLL CRC (it sets a synthetic
caller-verified flag), decrypt any actual meter's ciphertext or derive
the application length from an unknown frame. The exact plaintext
length is known in the test from a public upstream fixture. In live
traffic CBC `2F2F` does not authenticate address, access byte or
payload, so this test must never authorize direct HA publishing or
`VerifiedCleartext` trust for received frames. Production still
requires verified full-frame semantics, decryption identity, correct
application boundaries, anti-stale behavior and a tested fallback.
Neither real meter keys nor IDs are included in GitHub or logs.

## Stage A #109 host-only AES header isolation

Stage A #109 proved the synthetic TPL5 -> CBC -> XMQ host test
but failed two ESPHome Heltec builds because the generic shadow-header
staging copied `xmq_tpl5_cbc.h` (and its transitive
`#include "aes.h"`) into the ESPHome component umbrella header.
The CBC experiment is **not used by the on-device shadow probe**.

`tools/stage_shadow_headers.py` now excludes both
`xmq_tpl5_cbc.h` and `xmq_tpl5_direct_header.h` from ESPHome
staging, and purges stale generated copies while leaving the host
sources untouched. `tools/test_shadow_staging_allowlist.py`
regresses that behavior on a temporary workspace. Host crypto
tests still compile against the original `wmbus_common/aes.cc`.

This is dependency isolation only: it does not grant any encrypted
meter the right to publish from XMQ, change T1/C1 RF, update HA,
or install firmware. A successful next CI must confirm all five
jobs before attempting further on-device work.

## Exact inner meter identity gate for direct TPL5 XMQ candidates

`xmq_tpl5_identity_gate.h` now checks that the **same parsed
TPL5 frame** has the configured manufacturer's two raw bytes and all
six A-field bytes (four ID bytes, version, type). For CI 0x72 the
identity comes from the inner TPL address; for CI 0x7A it comes
from DLL. No fallback to the outer DLL address is allowed if the
inner identity does not match. The XMQ `Mvt` is derived from
these checked bytes, not supplied as an unrelated caller literal.
Synthetic short/long TPL5 → XMQ tests reject a different ID,
manufacturer, version or media type before decoding.

**This is routing identity only, not authentication.** Mode-5
CBC check bytes `2F2F` do not authenticate the sender or
ACC/IV; a forged or malformed header could still pass the weak
sanity check. The live radio/AES-to-XMQ path remains disabled.
Any future sensor-publishing gate must separately establish
payload provenance, security mode and field semantics, then
validate the expected 12-meter contract in the real environment.

## CI/encoding gate — synthetic CBC fixture is not live proof

The offline synthetic TPL5-to-XMQ test encrypts original proprietary
`mkradio4` fixture bytes under standard OMS TPL CI 0x72/0x7A.
That proves only reversible AES-CBC and independent XMQ numeric
projection on chosen bytes. **It is not a valid live telegram fixture**:
CI 0x72/0x7A is the StandardDv encoding path, whereas the original
proprietary fixture is OriginalIxml. Promotion to live requires exact
CI/encoding consistency and verified plaintext provenance.

`xmq_tpl5_live_admission.h` rejects proprietary IXML on standard
CI 0x72/0x7A, rejects ciphertext and preserves the explicit
prohibition on treating CBC `2F2F` as authentication. The associated
host assertions make accidental promotion of synthetic fixtures fail
CI. This gate still does NOT make mode-5 telemetry live-ready:
a future independent source must establish verifiable meter,
link and application provenance first.

## Stage A #112 transitive host-only header isolation

The CI for the strict CI/encoding gate exposed an ESPHome packaging
failure, not an XMQ host decoder failure: the shadow staging script
copied `xmq_tpl5_identity_gate.h`, which includes the host-only
`xmq_tpl5_direct_header.h`. The latter is deliberately excluded
from the generated ESPHome image. The new
`xmq_tpl5_live_admission.h` has the same transitive host-only
dependency.

Both additional headers are now excluded and stale generated copies
are removed. The staging regression test covers all four:
`xmq_tpl5_cbc.h`, `xmq_tpl5_direct_header.h`,
`xmq_tpl5_identity_gate.h`, and
`xmq_tpl5_live_admission.h`. No extra workflow is added.
Do not flash Heltec before the bundled compile and host tests
finish green.

## CI-origin classification before any direct XMQ activation

`xmq_ci_route_contract.h` now distinguishes the two evidence-backed
classes already recognized by the original `xmq_telegram_capture.h`:
manufacturer-specific **CI 0xA0/0xA2 -> OriginalIxml**, available
only as a successfully decrypted and handled **legacy verified**
application, versus standard **CI 0x72/0x7A -> StandardDv**, either
legacy verified or a *candidate* direct TPL5 path. The latter is
**not authorized for publication** simply by its CI classification;
it still requires DLL CRC, correct meter binding, actual decrypt
provenance and the independent XMQ readiness gate.

The classifier rejects unknown CI, wrong encoding, missing CRC evidence,
unverified proprietary payload and ambiguous dual-provenance requests.
`test_xmq_ci_route_contract.cpp` exercises each distinction with host
synthetic inputs. This is an isolation rule, not a claim that
actual private meter frames use a particular CI; complete live
telegram samples must establish that before selecting the direct route.

## CI evidence verified against actual production Telegram declaration

`test_xmq_real_telegram_contract.cpp` now includes the host-only
`xmq_ci_route_contract.h` and executes classification using the
**actual unmodified upstream `Telegram` type** and its
`tpl_ci` member (the telegram instance is a synthetic fixture).
The tests check A2 proprietary XMQ via successful legacy parsing,
7A standard DV via verified legacy parsing, a direct-standard
candidate that remains untrusted, unknown CI rejection, and
mandatory DLL CRC. The same frame must never be treated as both
a proprietary and a standard application.

This verifies type/API compatibility and routing logic, not actual
on-air CI distribution. The repository's HCA and water application
fixtures were captured after processing by `wmbus_common`;
these samples do **not** establish the full radio/TPL/AFL security
envelope for each physical meter. Before enabling independent
decryption, obtain privacy-safe live CI, link-security mode and
header category evidence on the Heltec *without keys, IDs or raw
payloads*; do not infer live formats from synthetic fixture CI.

## CI/TPL security metadata census — host-only first

The isolated `xmq_ci_security_census.h` records only aggregate counts
per CI bucket A0/A2/72/7A/other and TPL security bucket 0/5/7/other.
It reads `tpl_ci` and `tpl_sec_mode` from a successfully parsed
legacy `Telegram`, rejecting failed, discarded or undecrypted inputs.
No meter IDs, manufacturer identity, raw payload, values, access number,
AES key, timestamps or per-meter counters are retained.

`test_xmq_real_telegram_contract.cpp` validates the census against
the real upstream Telegram declaration and tests all three security
bucket examples with synthetic objects. No radio hook is registered:
observations from *actual radio frames* are still uncollected.
An opt-in, privacy-reviewed on-device diagnostic is required before
concluding which CI/security modes the 12 deployed meters transmit.

Stage A #115 failed compilation because a test tried to copy the real
`Telegram` object; its copy constructor is private. The fixture now
constructs independent `Telegram` objects and copies only fields
explicitly needed by the test. This does not modify the legacy class.

## Optional Heltec CI/security census through existing legacy shadow callback

The **experimental opt-in** `xmq_shadow_probe` now stages the
allocation-free `xmq_ci_security_census.h` and calls
`ci_census_.observe_telegram(telegram, decoded)` only after the
existing bounded post-decode callback has consumed a budget slot.
It resets counts at each hourly budget renewal. A single aggregate
`XMQ CI/TPL census` line is emitted when **128** callbacks consume the
session quota; it reports a 5x4 table of CI A0/A2/72/7A/other and
TPL modes none/5/7/other with a rejected count. It neither logs nor
retains meter IDs, access counters, manufacturers, raw frames,
keys, payloads or per-meter counts. It cannot update HA sensors.

**Coverage limitation:** this is not a radio-wide census. The existing
shadow hook and driver registry handle the subset of configured legacy
drivers with shadow readings (currently `mkradio4` and `fhkvdataiii`),
not necessarily `mkradio4a` or unmatched frames. It counts
*post-decode callback observations*, so an absent bucket does not
prove a CI/security mode is absent on air. The log line is emitted
only when the 128-observation cap is reached, not every hour by timer.
There is no production YAML change or device flash.

Before enabling this read-only probe, review the whole firmware
configuration, log-level behavior, radio memory budget and rollback.
The installed SX1262 remains on its existing decoder.

## GitHub-sourced shadow probe CI census header fix (#116/#117)

Two previous Stage A runs compiled the host CI/security census and the
staged ESPHome probe, but failed the **direct GitHub external_components**
smoke compile. This is a packaging gap: the standalone
`xmq_ci_security_census.h` was present under the experiment root and
staged by the local build script, but absent from
`components/xmq_shadow_probe/`, which ESPHome downloads when using the
GitHub-sourced external component.

The header is now bundled byte-for-byte with the GitHub-installable
component, and the existing host job compares the source and bundled
copy *before staging*. The direct GitHub smoke job remains part of
the SAME single Stage A workflow and must complete green before
claiming the probe is installable. This fix makes no changes to
production radio framing, meter drivers, or Home Assistant sensors.

The census is still **opt-in**, read-only, and scoped to the existing
post-decode shadow callback. It does not yet prove coverage for
`mkradio4a`; no claim about its on-air CI/security mode may be made
until that coverage is established. No firmware flash is authorized
by the CI result alone.

## mkradio4a metadata-only CI/TPL observer coverage

The actual upstream `driver_mkradio4a.cc` registers both T1 and C1,
sets expected TPL security mode `AES_CBC_IV` (mode 5), and its
**public upstream** telegram fixtures use **CI 0x72** (long TPL) with
a `2F2F` framing sentinel and an inner address. These are documented
upstream examples, **not proof of the current user's meter's RF
CI or security envelope**. In contrast, public `mkradio4` fixtures
use manufacturer-specific CI 0xA2.

The experiment's cloned `wmbus_meter` invokes the existing
`observe_legacy_telegram` hook **after `handleTelegram` for any
configured meter**, including mkradio4a. The census updates before
the shadow comparison's supported-driver check; hence mkradio4a
can add CI/TPL counts without implementing or faking XMQ parity.
The build-only `heltec-xmq-shadow-build.yaml` includes one
synthetic `mkradio4a` meter with no key and no HA sensor.
`check_shadow_lab_meter.py` now asserts callback ordering and
this test-only fixture.

No live meter ID, key, frame, date or volume is saved in the census.
It only counts successfully handled post-decode callbacks. The
currently bundled XMQ shadow decoder continues to compare only
`mkradio4` and `fhkvdataiii`; adding a real `mkradio4a`
numeric/date parity check is a **separate** feature requiring
the correct XMQ driver source, verified date handling and a
full build/run test. This stage does not add that feature.

## Independent XMQ mkradio4a volume and date readiness (host only)

Stage A #119 passed all 5 GitHub jobs, including the direct
GitHub-sourced external component compilation. The next isolated
`test_xmq_mkradio4a_primary.cpp` uses the original upstream
`mkradio4a.xmq` with **two public clear DV record samples**:

- `TCH,95,37`: target volume 16.1 m³, target date 2021-12-31
- `HYD,FE,06`: target volume 75.2 m³, target date 2021-12-31

The test invokes `decode_primary_candidate` with both
`target_m3` and `target_date` mandatory. It rejects either
record missing, wrong driver/MVT, proprietary IXML interpretation
of the standard DV record and CI=0xA2 with a StandardDv encoding.
Rejected output must be **fully cleared**, not partially publishable.

The test is *not* an encrypted RF fixture, and it does not
authorize mkradio4a to write HA states. It asserts XMQ
readiness only after caller-provided verified/clear application
bytes and CI-consistent routing; no field values are borrowed
from legacy. The existing read-only shadow build still does
not compare mkradio4a readings. Do not flash the lab YAML.

## mkradio4a read-only shadow parity — complete DV volume AND date

After the independent public-source XMQ primary test, the isolated
`components/wmbus_meter` clone now records the legacy `target_m3`
and the original `target` PointInTime field for `mkradio4a`.
The clone marks both numeric and date field-matcher evidence from the
**same successfully parsed Telegram**. This is an opt-in lab callback,
not a change to `components/wmbus_meter` in production.

The opt-in shadow probe bundles the original upstream
`source_mkradio4a()` XMQ rules alongside the existing drivers.
For mkradio4a **only**, it requires the legacy volume to be present
and matched to this telegram's DV entry, and a nonempty target date
also matched to this telegram's DV entry, before comparison. Missing
or unmatched reference evidence is counted as `no_reference_` and
never as `Match`. XMQ must independently return both
`target_m3` and `target_date`; its primary readiness gate
rejects incomplete XMQ output.

The comparison remains bounded, optional, read-only, uses verified
legacy plaintext and does not prove independent AES/radio transport.
A passing host/build CI cannot substitute for observed matching
results on actual configured meters or memory/load measurements.
No RF takeover, sensor publication, key logging, real meter ID,
production YAML edit or device flash is performed.

## GitHub-bundled XMQ rule integrity

`tools/test_bundled_driver_sources.py` verifies that the entire
original XMQ text for `fhkvdataiii`, `mkradio4`, and
`mkradio4a` is included in the GitHub-installable
`components/xmq_shadow_probe/xmq_driver_sources.h`. The
**existing** host Stage A workflow executes this check before
staging/compilation, so direct-GitHub firmware cannot quietly use
different XMQ driver definitions from offline host tests.

This is a static source integrity gate only: it does not prove
independent on-air AES decryption, per-meter data parity,
RAM safety or RF performance. The new mkradio4a read-only
comparison and budget remain opt-in and cannot update sensors.

## Optional XMQ memory preflight for Heltec SX1262

The experimental, read-only shadow parser now requires **at least 128 KiB
of 8-bit-capable free heap** and **80 KiB in the largest contiguous
8-bit-capable free block** immediately before building reference
maps or invoking XMQ. Failure skips *only the optional shadow
comparison*, with a bounded `low_memory_skipped` counter; the
legacy meter receiver/decoder and Home Assistant publishing are
not affected by this early return.

`xmq_shadow_memory_gate.h` has byte-identical host and direct-GitHub
ESPHome component copies. `test_xmq_shadow_memory_gate.cpp`
checks thresholds, invalid impossible samples and a reported
Heltec memory baseline (160852 B free / 112640 B largest block).
The CI checks the exact bundled copy and enforces the gate's
placement before `diagnostic_.inspect`.

The source now records `min_largest_block` as well as
`min_free_heap` and `low_memory_skipped` in its non-sensitive
shadow statistics. These are **preflight guards, not memory usage
measurements or proof against later OOM**. A successful firmware
compile does not authorize a production flash. After compiling,
test only a separately reviewed, reversible canary, measuring
free/largest/minimum heap, RX counts and legacy sensor continuity.

## Earlier read-only memory checks: startup and pre-snapshot

In addition to the existing pre-decode memory check, the opt-in
`xmq_shadow_probe` now performs the same 128 KiB free / 80 KiB
contiguous **8-bit-capable** heap threshold **before constructing the
XMQ driver registry at setup**. If the check fails, the shadow
probe remains inactive while legacy processing continues unchanged.

When the cloned `wmbus_meter` asks `wants_legacy_readings()`, the
probe also performs the allocation-free heap preflight **before**
capturing baseline legacy numeric/date values, before/after DV
snapshots, and before optional XMQ inspection. If low, the
snapshot is skipped. The previously added check immediately
before XMQ parser execution is retained as a second guard.
These checks cannot guarantee that later allocations will succeed.

Both checks affect only the experimental shadow observer and do
not own the SX1262 receiver, call `mark_as_handled`, suppress
legacy meter frames, or write HA values. Source-level guard tests
enforce that the pre-snapshot memory check is in place. A real
canary requires a reviewed rollback and runtime data on heap,
largest free block and RX/sensor continuity; no flash is performed.

## T1_C1 synthetic shadow build gate (no device installation)

The **actual** SX1262 installation uses `native_stream_mode: T1_C1`,
not the T1-only generic compile fixture. The new
`heltec-xmq-shadow-t1-c1-build.yaml` is a **standalone**,
CI-checked derivative of the offline shadow test image, using `T1_C1` and
two fake/keyless meter IDs configured with `mode: [T1, C1]`.
It has **no Wi-Fi, API, OTA, MQTT, real meter secrets or production
sensors**; do not upload or flash this synthetic lab image.

The **same Stage A workflow** checks this standalone YAML for exact\nderivation from the T1-only baseline, preventing duplicate meter IDs\ncaused by ESPHome package-list concatenation, and
compiles it with ESPHome 2026.10.1 and 2026.11.0. This can catch
a dual-link-mode configuration or C++ compatibility regression,
which a T1-only CI build cannot.

It does **not** validate RF T1/C1 timings, accurate packet reception,
AES decryption, the 12 live meter configurations, startup memory,
or continuing legacy sensor updates. Use only the private known-good
YAML (preserving all 12 meters and original T1_C1 settings) to
construct any later, separately reviewed reversible candidate.

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
