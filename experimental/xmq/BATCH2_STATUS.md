# Batch 2 — initial source import and acceptance gates

> **Current-state notice — 2026-11-07.** This is a chronological
> engineering ledger. Earlier sections describing Minomess, Kamwater
> and Apator162 as source-only/host-only or prohibited in YAML
> describe **past milestones** and must not be read as current branch
> behavior. Refer to [PROJECT_CONTEXT.md](../../PROJECT_CONTEXT.md)
> and the current-state summary immediately below.

## Current branch status — source compatibility vs firmware admission

**Last green code CI:** `2f518a35c5ce34d56d8c40497755380293a4e4f3`,
[GitHub Actions run 42393848471](https://github.com/radugeo74/esphome-sx1262/actions/runs/42393848471).
All three jobs passed, including the native ESPHome Heltec combined
T1/C1 build. This is **source tests and compilation**, not physical
RF-to-Home-Assistant verification and not permission to flash.

| Driver | Configured clear native RX | Configured `driver: auto` | AES |
| --- | --- | --- | --- |
| `minomess` | Explicit `meter_drivers`, verified long TPL | Yes, requires matching actual TPL ID; optional separate DLL sender | Only explicit `secure_meters` with key |
| `kamwater` | Explicit verified ELL/compact clear path | Yes, strict model/identity/security/CRC | Only explicit `secure_meters` with key |
| `apator162` | Explicit verified clear short TPL | Yes, strict model/identity/security | Only explicit `secure_meters` with key |

`BATCH2_PENDING` stays **out of the generic `SUPPORTED`
manifest**, but that does **not** block the separately reviewed
configured-only `auto` path through
`xmq_batch2_plain_dispatch.h`. A model candidate alone is
insufficient: verified DLL CRC, configured sender/meter identity,
plaintext (or explicitly authenticated keyed pathway), supported
records and mapped sensors remain mandatory. Never auto-create or
publish sensors for unconfigured IDs.

**Separate latest driver calup:** `ultraheat` (LUG/04/04)
and `sontex868` (SON/16/08) are compiled and supported for
configured-only clear `driver: auto` or explicit driver name.
They use bounded native DIF/VIF subsets; the actual source
fixtures and fail-closed tests are green in the same run.
`apator08` was evaluated but not enabled: public examples use
nondecimal/non-BCD IDs incompatible with the current strict
eight-digit decimal address contract. Techem baseline decoding
remains unchanged; no real Ultraheat/Sontex devices or OTA were
involved.

**Remaining gate:** real authenticated/configured T1/C1
RF-to-HA behavior for these additional families, field parity
outside the explicitly implemented subsets, and canary/rollback.
CRC alone is never evidence of cryptographic authenticity.

---

Official upstream: `wmbusmeters/wmbusmeters`, branch `master`, directory
`drivers/src`. The three complete original XMQ files, including their public
regression fixtures, are present in `experimental/xmq/drivers`.

| XMQ file | Upstream source blob SHA | Key decoding obstacle |
| --- | --- | --- |
| minomess.xmq | 418c3a209ccbacb42909436e7af53034bd733c0c | 8C0413 falsely encoded as BCD: reinterpret binary volume while preserving the null/sentinel case; flags and dated targets |
| kamwater.xmq | 0cd7d5c6fbcb9d3381b7d18966ebab1a53e12485 | Compressed/compact DIF/VIF profiles, status variations and security |
| apator162.xmq | 116907a3e22314ccc52a0f4cd1ab8ddb10eaab27 | Manufacturer-specific IXML grammar: repetition, alternatives, checksums and FF padding |

## Historical initial-import status (superseded by current branch summary)

**In progress:** `minomess_clear_prefix.h` now implements and independently
tests the bounded original-XMQ clear-application prefix on four upstream
golden cases (two genuine null target forms, a normal target, dates and
malformed inputs). It does **not** decode proprietary history or authorize
live RF. `kamwater` and `apator162` remain source/fixture-only.

**Kamwater cleartext pilot:** `kamwater_clear_records.h` now uses the shared bounded DIF/VIF parser to decode the official already-clear application records for total, target, and raw status. Three upstream reference cases are exercised by `test_kamwater_clear_records.cpp` in CI. The pilot is transactional and deliberately excludes compact telegram expansion, ELL security/decryption, and live sensor publication.

**Compact-profile pilot (host-only):** Added a shared, fixed-capacity `compact_dif_vif.h` expander which requires an explicitly supplied, validated DIF/VIF template. Three official Kamwater templates are now exercised in host-only CI. The first two have matching paired compact/full examples; the third flowIQ profile (`04FF234413523B06FF1B426C61675167023B04138101E7FF0F`) is compared against a reconstructed full-record expected value because the separate upstream full snapshot has a different instantaneous flow (0 vs 0.035 m3/h). Total and target are still compared with the upstream reference (3.871 m3 and 0 m3). A fourth upstream-declared profile (`02FF2004134413426C523B615B616751678101E7FF0F06FF1B`) now has an isolated synthetic-only roundtrip test, because no official compact/full paired fixture for this exact profile exists in the imported XMQ. This test proves structural expansion and basic numeric decoding, not real-world RF compatibility. A fifth upstream template (`02FF2004134413A1015B8101E7FF0F`) is also covered by a synthetic-only compact expansion test. Upstream explicitly notes this particular format may belong to KamHeat or KamWater, so it must never be used as a driver identity selector. The sixth upstream format (`02FF2004134413615B5167`) is also implemented as a separate host-only helper and has synthetic expansion, total/target and length-rejection tests. No official paired compact/full frame is available for this format, so RF compatibility is not established. The seventh upstream template (`02FF2004134413`) now has a host-only synthetic roundtrip for status, total and target, with negative tests for short and excess payloads. It has no paired upstream compact/full golden example and must not be auto-selected from bytes alone. The eighth upstream template (02FF200413523B) has a synthetic-only host test for status, total and storage-one historical flow; no target volume exists in this format and no paired upstream telegram exists. The ninth KWM2231 template now has synthetic host-only regression coverage for expanded records, volume readings and malformed lengths. All nine declared Kamwater compact profiles are represented by isolated host-side functions: three use public paired examples and six have synthetic-only structural checks. Live RF is still blocked. The wrapper does not automatically choose a profile, validate format signatures, decrypt ELL, or publish sensors. Unknown and truncated compact content fails closed.

**Imported as source and regression fixtures, not yet compiled into live RF
sensor decoding.** They are enumerated in `BATCH2_PENDING`, not `SUPPORTED`.
The auto registry may identify their manufacturer/version/type metadata but
does not authorize decode or publication. The ESPHome YAML validator must
reject them as `meter_drivers` until their complete decode contracts pass.

`test_batch2_upstream_fixtures.py` checks each original fixture inventory
and blocks accidental enablement; CI also retains all established T1/C1 and
old driver regressions.

## Compact admission gate (host-only)

The new `kamwater_compact_admission.h` provides an independently tested
**evidence contract**, not a working RF verifier. It rejects requests unless
the caller has already established all three separate facts: device identity,
the compact format signature, and a verified-clear ELL application. The
profile index must also be one of the nine registered templates.
An encrypted, unknown, missing-signature, or guessed-profile telegram stays
rejected. The current TPL security probe does not validate Kamstrup 8D20 ELL
or its cryptographic mode. No live caller currently supplies these proofs,
so this gate does not enable any firmware sensor publication.

## Compact signature lookup (host-only)

`kamwater_compact_signature.h` now computes EN13757 CRC16 over each of the
nine original XMQ DIF/VIF templates (polynomial 0x3D65, initial 0x0000,
final complement). Its fixed registry returns exactly one matching index or
fails closed for unknown/ambiguous signatures. The first upstream compact
example carries signature EDA8 (little endian), matching 0xA8ED. CI checks
the nine signature values and the absence of authorization when only a
signature is known. The registry does not accept a decoded value, guessed
length, or device appearance as evidence, and it is not connected to RF.
A CRC16 format signature identifies a layout; it is NOT cryptographic
authentication, a data-integrity proof, or a substitute for ELL verification.

## ELL II read-only proof (host-only)

`kamwater_ell_ii_probe.h` now checks the original upstream ELL II
(`0x8D`) layout: CC, access, four-byte session number, two-byte ELL
payload CRC, and the following TPL bytes. The session security field
(bits 29..31) is required to be **zero**. AES-CTR security mode 1 and
reserved modes are rejected without trying to decrypt. For a clear-mode
telegram the EN13757 CRC must match the entire following TPL payload,
and the TPL CI must be 0x78 or 0x79. Tests cover a synthetic valid clear
frame, changed payload or CRC, truncation, wrong CI/TPL and the upstream
AES-CTR header. This probe does **not** validate DLL CRC, authenticate a
message cryptographically, implement AES-CTR, validate compact TPL 0x79
data CRC, or connect to the native runtime. In particular, upstream
NOKEY/postdecrypted fixtures do not make their raw ELL frames clear.

## TPL 0x79 compact-data integrity (host-only)

`kamwater_tpl79_probe.h` now reads the compact TPL header
(`79` + two-byte DIF/VIF format signature + two-byte data CRC) and
looks up exactly one of the nine declared profiles by EN13757 CRC16.
It reconstructs the complete DIF/VIF record stream and checks the
transmitted data CRC **over the expanded stream**, not over the
compressed value bytes. Both original public MULTICAL21 compact/full
pairs establish this: signature `0xA8ED` / data CRC `0xB66A`,
and signature `0xC412` / data CRC `0xFF0D`. CI rejects unknown
signatures, malformed lengths, corrupt values and CRC, and also
confirms a valid compact CRC cannot bypass ELL AES-CTR rejection.
The output consists only of verified expanded bytes and a profile
index; the driver is still not dispatched in live RF.

The compact CRC provides **accidental-error detection, not
cryptographic authentication**. Full acceptance additionally requires
the DLL CRC, ELL security validation and verified meter identity.
The original `wmbusmeters` parser reads the two-byte compact data
CRC without independently checking it in `parse_TPL_79`; this
stage explicitly verifies its meaning against the two public examples.
Raw AES-CTR ELL frames remain unsupported without a separately
validated cryptographic stage.

## Clear ELL II to compact measurements, host-only

`kamwater_clear_pipeline.h` composes the existing ELL II clear-mode
inspection, TPL 0x79 format/data CRC verification, admission contract,
and bounded DIF/VIF record decoder into one transactional host-only
function. It requires **explicit external evidence** that DLL CRC and
meter identity have both been verified; it does not itself establish
either condition. The result is cleared on every failure. Its CI test
uses the published compact application example inside a **synthetic**
clear ELL II envelope and rejects absent DLL/identity proof, encrypted
ELL, invalid ELL CRC, unknown format, changed compact CRC or measurement,
truncation, and unsupported TPL. Successful host tests do not establish
that the on-air Kamstrup frames, which may use AES-CTR, are decodable.
There is no runtime dispatch, YAML support, decryption, or sensor
publication. Before enabling on Heltec, separately verify DLL header
and identity provenance, encrypted ELL support where required, and
real RF integration with source-derived fixtures.

## DLL-verified and configuration-matched ingress (host-only)

`experimental/semtech-radio/host_only/kamwater_verified_dll_ingress.h` now passes an untrusted raw DLL
frame to the existing `wmbus_pure::verify_complete_frame` with explicit
CRC frame format. The resulting decoded header is matched against an
**independently configured** four-field meter identity (manufacturer,
BCD meter ID, version and medium/type); no caller-provided
`dll_crc_verified=true` or `meter_identity_verified=true` can bypass
this entry point. Only an exact match and ELL CI 0x8D proceed to the
clear ELL II / compact TPL79 / records pipeline. Failures leave an
empty result. CI exercises a fully synthetic format-B frame with CRC,
the public compact application fixture, and negative tests for DLL CRC,
mismatched manufacturer/ID/version/type, unexpected CI, ELL CRC,
encrypted ELL and compact data CRC.

The meter identity comparison is **not cryptographic authentication**:
a CRC-recalculated forged frame can carry the configured address. No
live radio registration, AES-CTR, or Home Assistant publication has
been enabled. Existing driver behavior remains unchanged.

## ELL AES-CTR IV preparation (host-only, no keys)

The public `wmbusmeters` `decrypt_ELL_AES_CTR` implementation derives
a 16-byte counter initialization vector from manufacturer (2 bytes,
little endian), DLL address (6 bytes), ELL control byte with repeated
and hop flags cleared (mask 0x12), the four ELL session bytes, and
three zero bytes (FN[2], BC[1]). A new independent keyless helper
`experimental/semtech-radio/host_only/kamwater_ell_ctr_iv.h` tests
that exact construction using only synthetic header fields. The
probe is limited to ELL II `0x8D` with AES-CTR security mode 1 and
returns no decrypted bytes; it does **not** take keys, implement
AES-CTR, perform counter increments, verify post-decryption ELL CRC,
or permit any live publication. This file stays outside the
ESPHome component include tree.

## ELL AES-CTR host-only proof of concept

The experimental `experimental/semtech-radio/host_only/kamwater_ell_ctr_openssl.h`
implements the upstream blockwise ELL II (`0x8D`) AES-128 CTR
transformation using OpenSSL AES-ECB keystream blocks and a big-endian
128-bit incrementing counter. It derives the IV from the separately
tested original field layout, decrypts from the two encrypted payload-CRC
bytes onward, then compares the recovered EN13757 CRC to the recovered
TPL application. Only TPL `0x78` and `0x79` are eligible; failures
leave no output. CI tests a standard NIST AES-128 ECB known-answer
vector and a synthetic encrypted ELL with synthetic key, plus negative
tests for wrong keys, changed ciphertext, incorrect mode and truncation.

**This is NOT production decryption**: no real keys, no decrypted RF
captures, no replay protection, no cryptographic message authentication,
and no integration into ESPHome or the live `kamwater` decoder. CRC is
a non-cryptographic error detector. The example's AES ciphertext is
constructed synthetically for this test and is not an independent
external W-MBus encrypted golden vector. Require independent upstream
encrypted reference verification and later full end-to-end admission
checks before any firmware use.

## Regression guard: keep experimental decryption host-only

`experimental/semtech-radio/ci/test_kamwater_host_only_isolation.py`
is now executed in the native CI workflow. It checks that the synthetic
ELL AES-CTR OpenSSL helper, keyless IV helper and the DLL ingress remain
under `experimental/semtech-radio/host_only/`, not in the ESPHome
component, and that the native component imports neither host-only
modules nor OpenSSL headers. This guards against the previous build
failure caused by a host-only header being auto-included by ESPHome.
No AES-CTR or Kamwater dispatch is enabled in firmware. A passing CI
only confirms code separation and existing tests, not real RF decoding.

## Synthetic encrypted ELL to TPL79 and measurement regression (host-only)

The existing OpenSSL host-only AES-CTR test now additionally verifies that
the decrypted application passes the TPL 0x79 signature and expanded-data
CRC checks and yields total, target, and status through the existing
bounded Kamwater DIF/VIF reader. A second synthetic ciphertext deliberately
contains a corrupted compact-data CRC but a recomputed valid outer ELL CRC:
decryption succeeds, while the independent TPL check rejects it without
emitting a profile. This is a **test-only composition**; no decrypting
driver, active RF dispatch, real keys, or new ESPHome firmware behavior
were added. Test success is not proof of a real encrypted Kamstrup
telegram or cryptographic authentication.

## Synthetic full DLL boundary for encrypted ELL regression

The existing host-only AES-CTR test now builds a synthetic format-B
DLL frame around the previously tested encrypted ELL sample. The
unmodified shared `wmbus_pure::verify_complete_frame` must validate
DLL CRC and extract the manufacturer, meter ID, version, type and ELL CI
before the test derives AES-CTR IV bytes directly from the verified
DLL frame. The decrypted TPL79 then passes through the existing compact
CRC and DIF/VIF tests. DLL corruption is rejected; a DLL packet with
a recalculated CRC but a changed meter ID must fail an independently
configured identity comparison. This is a host-only regression test,
not a wired production decryptor, not cryptographic authentication,
and not a real encrypted radio capture.

## AES-CTR negative-input regressions (host-only)

The synthetic Kamwater ELL CTR CI test now rejects shortened ciphertexts,
unsupported ELL security modes, an incorrect ELL CI, a modified
session-number byte, and missing key/frame pointers. Assertions also
require that every failed decode clears previously populated output.
These checks extend the existing host-only OpenSSL test; no production
decryption, real meter credentials, firmware changes, or RF publication
are introduced. Passing tests are a necessary regression guard, not
evidence that real encrypted telegrams have been validated.

## Stage A consolidated transport implementation (host-only)

The original `wmbusmeters` parser has separate ELL AES-CTR and OMS
security-profile-D AES-CCM paths. The project now has the corresponding
**host-only** code, with an integrated `kamwater_secure_ingress.h` gate:
verify full DLL CRC/length, require the independently configured four-field
identity, then dispatch *only* the supported secure transport shape.

- ELL II `CI=0x8D`, AES-CTR mode 1: derive original 16-byte IV from
  DLL M/A and ELL CC/SN, decrypt payload CRC and TPL; verify ELL CRC;
  decode TPL `0x78` standard records or TPL `0x79` using the exact
  known compact signature and expanded-data CRC.
- ELL I `CI=0x8C`, followed by short TPL `0x7A`, OMS mode 10:
  `oms_tpl_ccm_openssl.h` extracts CFG and CFE, requires KDF selection 1,
  no partial encryption (CFG N=0xFF), supports declared 4/8/12/16-byte
  tag sizes and optional key-version; derives Kenc by AES-CMAC from
  DC=00, little-endian message counter, DLL ID, seven 07 bytes. Builds
  nonce M(2)+ID(4)+version+type+00+counter(big-endian), validates the
  AES-CCM tag over the TPL header AAD and decrypted DIF/VIF application,
  and rejects all invalid tags before decoding or returning measurements.
- Any unrecognized security mode, malformed header, bad CRC, different
  configured identity, invalid compact signature or bad CCM tag is
  **rejected with empty output**. No fallback to guesswork or plaintext.

The host-only CI now includes synthetic encrypted TPL7A inside an ELL-I
and DLL format-B envelope, a separate independent OpenSSL CLI CMAC golden
value, corrupted-tag/wrong-identity/altered-AAD scenarios, and the
existing synthetic ELL-II/CTR compact and clear probes. Both encrypted
paths are composed in the same host-only ingress. The original
`wmbusmeters` implementation may label an invalid-tag telegram with
`FAILED_DECODE` for diagnostic output; this project deliberately
refuses to publish measurements on tag failure.

**Not yet passed as production compatibility:** the project's new
secure ingress is NOT wired into native firmware, YAML, radio reception,
MQTT or Home Assistant; no user AES key or private capture is stored.
Independent replay of the original `kamwater.xmq` encrypted golden
fixtures, supported partial-encryption variants, non-ELL-I/II transport
forms, AES-CBC and model-specific complete status/field parity remain
outside this stage. The preceding synthetic tests cannot prove
correctness for real encrypted telegrams. Next stage must use official
upstream fixture parity rather than treating synthetic CI PASS as a
release criterion.

## Stage B fixture acceptance ledger — not yet parity

`experimental/xmq/tests/test_batch2_parity_ledger.py` enumerates
every official upstream golden test and its expected JSON measurement
fields: minomess 6, kamwater 14, apator162 19 (39 total). It checks
source JSON and frame structure, reports keyed and NOKEY cases without
printing credentials or telegram bytes, and is included in CI.
Its `NOT_RUN` result is intentional: previous independent C++ unit
tests prove selected **subsets**, but there is not yet a replay driver
that compares **every** expected field to a decoded result for a
complete upstream frame. Consequently, the ledger is **not** a
39/39 PASS claim and does not authorize firmware dispatch.

Stage B acceptance requires a decoder-parity runner to produce the
actual field outputs, compare them to each original JSON with suitable
numeric tolerances, and classify each fixture as PASS, FAIL or
UNSUPPORTED with an explicit cause. No pending case may silently count
as PASS. Also track full status flags, dates, manufacturer-specific
formats, encryption and incomplete/invalid test cases.

## Stage B actual C++ to original XMQ JSON subset comparison

`experimental/xmq/tests/test_batch2_clear_fixture_parity.py` now
executes the existing C++ clear-application decoders through
`test_batch2_clear_fixture_cli.cpp` and compares their output fields
against original upstream XMQ fixture JSON. The first 7 specifically
reviewed cases are:

- **minomess #3–#6**: `total_m3`, `target_m3` (including
  absent/null cases), `meter_date`, `target_date` — four
  comparisons per case.
- **kamwater #1, #5, #6**: `total_m3`, `target_m3` — two
  comparisons per case.

That totals **7 original example applications / 22 actual expected-field
comparisons**. The executable consumes ONLY the post-decryption
application after the original XMQ `_` boundary; it does not make
assumptions about the raw RF encryption mode, status string/flags,
unverified extra fields, compact decoding, or complete driver output.
The suite fails CI on a missing expected field, changed native output
shape, C++ decoder rejection or a wrong expected value. This is genuine
cross-comparison, not a source inventory or synthetic round-trip.

**Acceptance remains partial**: 32 other upstream cases, and additional
fields even in these 7 cases, have not passed equivalent output parity;
no driver is released to firmware. The 39-case upstream ledger remains
separately labeled NOT_RUN for *full-frame/full-output* parity. Until CI
passes this new step, the 7-case subset is pending confirmation.

## Stage B Apator162 official XMQ field-grammar parity

The original `apator162.xmq` embeds an IXML grammar with a seven-byte
status prefix, a repeated union of named tag+fixed-length fields,
an optional run of `FF` pad bytes and a 2/4-byte checksum suffix.
The host-only `apator162_ixml_fields.h` follows those IXML field
**tag and length rules** and extracts the `total` tag's uint32
little-endian liters as `total_m3` after consuming a structurally
valid manufacturer-specific application. It does not depend on a
single absolute byte offset, ignore unknown middle fields, guess a
field length, validate the trailing checksum algorithm, or wire into
firmware.

`test_apator162_upstream_volume_parity.py` executes the compiled C++
parser on all **18** original `NOKEY` clear manufacturer applications
(#1-#18), after their original short TPL header and optional `2F`
padding, and compares actual `total_m3` with each reference JSON.
The **19th** fixture carries an encryption key and is explicitly
rejected as clear/UNSUPPORTED. These 18 new actual output comparisons,
plus the 22 earlier minomess/kamwater field comparisons, comprise
**40 field comparisons across 25 original fixtures** (if CI green).
This is **partial field parity only**: it does not validate the full
XMQ status, radio CRC, keyed decoding, authenticated TPL or the
complete manufacturer-specific semantic fields. Do not mark the 39
full-output ledger entries PASS on the basis of subset checks.

## Stage B original Kamwater TPL79 compact parity — reference CRC correction

The original compact `kamwater.xmq` TPL79 cases #2, #4, #7
and #8 are exercised by `test_kamwater_compact_upstream_parity.py`.
**The earlier conclusion that original #7 contained a broken compact CRC
was incorrect.** Its on-wire `0x8054` matches the EN13757 CRC of
the immediately preceding *full* application in official fixture #6.
The reconstructed *current compact* application (which includes a
changed instantaneous flow reading) instead hashes to `0x335D`.
Comparing those two different generations was a protocol error in our
implementation, not proof of a corrupt upstream telegram.

`kamwater_tpl79_probe.h` now offers the explicit
`verify_with_full_reference` path. It checks that a separately
trusted prior full application matches the registered DIF/VIF
template **byte by byte at every record header**, checks the
transmitted TPL79 reference CRC against that complete prior
application, and only then expands the current compact values.
The host-only C++ parity CLI verifies #7 against the upstream full
fixture #6. Without the trusted reference it retains the old strict
self-comparison and demonstrably rejects #7. **Never invent or guess
a reference for live RF.** The new path cannot by itself authenticate
current compact data: independent DLL and ELL CRC or cryptographic
verification is required at the radio ingress.

The corrected upstream subset is **4 original compact applications
and 8 JSON volume-field comparisons**; together with 25 previous
fixtures / 40 fields, this gives **29 distinct fixtures / 48 partial
field comparisons**, subject to CI confirmation. Full output parity,
persistent trusted per-meter full-frame reference state, real RF
admission and status/temperature/flow parity remain outstanding.

## Stage B Kamwater XMQ status and alarm-duration parity

Host-only `kamwater_status_semantics.h` implements the reviewed
`kamwater.xmq` status field interpretations for `02FF20` (legacy
16-bit flags, 4-bit first duration shift), `04FF23` (modern cold
environment, 11-bit first duration shift), and `04FF24` (modern
hot environment). Alarm durations use the eight original
`duration_bucket` labels. The text is derived from bounded
DIF/VIF records, with no radio dispatch.

`test_kamwater_status_upstream_parity.py` runs the actual C++ parser
on original NOKEY clear applications **#1, #5, #6, #11 and #12**
and asserts **34 status / alarm-duration field values** against the
source XMQ JSON, including both high and low environmental duration
variants. These are additional field comparisons, not five newly
full-decoded RF fixtures. The previously implemented batch2 subset
covers 29 distinct fixtures / 48 values; with this test the expected
field-comparison count is **82** across **31 distinct fixtures**,
*only if CI passes*. Other Kamwater flags, numeric flow/temperature
measurements, history and complete JSON parity remain pending.

## Stage B Kamwater original numeric field parity

The HOST-ONLY `kamwater_numeric_semantics.h` uses the shared bounded
EN13757 DIF/VIF record parser and original Kamwater XMQ field selectors
(function, VIF family, storage number) to decode flow measurements,
flow/external temperatures and the target date. Unknown/duplicate
fields and invalid dates are rejected instead of assigning made-up
values. Numeric scaling uses the shared `to_canonical` function.

`test_kamwater_numeric_upstream_parity.py` now executes this native C++
parser on **8 official NOKEY applications**, including full examples
#1, #3, #5, #6 and compact #2, #4, #7, #8. The compact examples use
their exact profile signature and TPL79 parsing; #7 supplies its
previous validated full application #6 as a CRC reference, not a
current-frame CRC. The test compares **30 flow/temperature/target-date
values** against the official original XMQ JSON, without hardcoded
expected readings in C++. It rejects any extra or missing decoded
field in this test subset.

Together with the previously confirmed 82 subset-field comparisons,
the expected total becomes **112 compared fields across 32 distinct
original fixtures**, *pending the new CI run*. This is not full
driver-output parity: other Kamwater status variants, history,
manufacturer-specific fields and encrypted-radio fixtures remain.
The host-only source is not imported into production firmware.

## Stage B original encrypted Kamwater OMS profile D regression

`test_kamwater_upstream_ccm_openssl.cpp` executes the actual host-only
OpenSSL AES-CMAC KDF and AES-CCM decoder against *original public*
`kamwater.xmq` encrypted samples **#9 and #10**, not just a synthetic
round-trip. It uses only the original upstream fixture key. With the
official DLL manufacturer, meter ID, version and media as inputs,
the malformed #9 fails CCM tag validation and MUST produce no clear
application, while #10 authenticates, decrypts to the expected complete
DIF/VIF bytes and yields `total_m3=0` and `target_m3=0` with TPL
status byte `0x10` and counter `0x000E578C`.

Additional negative checks enforce output clearing after altered AAD,
ciphertext, tag or meter identity and missing key. The local independent
Python `cryptography` AES-CMAC/AES-CCM check agreed with these outcomes;
the GitHub Actions step compiles and exercises the *actual C++* code.
These tests **do not** assert the original upstream's
`FAILED_DECODE` fallback fabricated values: the secure local policy
rejects unauthenticated content entirely. Likewise, public upstream
test-frame representations are not full CRC-bearing RF frames, so
no live reception or firmware admission is inferred.

If the CI step passes, two more *distinct original fixtures* have
cryptographic outcome coverage (#9 rejected, #10 authenticated), but
the existing 32-fixture / 112-field partial comparison count does
not automatically become 34 field-parity PASS fixtures: #9 is a
negative-security test and #10 is a partial numeric-field comparison.

## Stage B Kamwater opaque acoustic field parity (pending CI)

The Kamwater XMQ field `accoustic_noise` (the spelling in upstream JSON)
is a **six-byte opaque text diagnostic** matched by `06FF1B`. The
host-only numeric semantics parser retains its original uppercase
hexadecimal bytes, never converts that field into a numeric reading,
and rejects duplicate matches. The existing upstream-native field
comparison now tests this text in clear/compact official fixtures
**#5, #6, #7, #8**, adding **four** JSON field comparisons. The
encrypted official `#10` test also checks its original
`FFFFFFFFFFFF` value **only after** successful AES-CCM
authentication; corrupted authenticated fixture `#9` produces no
clear application and no measurements.

Pending the new CI run, this would increase the existing confirmed
112 subset-field comparisons to **117** (four clear/compact + one
authenticated). The four clear/compact cases were already counted; the
authenticated #10 now gains its first directly compared opaque field.
Thus the number of distinct fixtures with at least one compared output
field rises from **32 to 33** if CI passes. Fixture #9 remains a
separately verified **negative authentication test**, not a field PASS.

## Stage B Kamwater original TPL7A status propagation (#13/#14)

Host-only `kamwater_tpl_status_semantics.h` now composes the
**exact published** unencrypted short TPL `7A` status `0x02`
(generic `ERROR`) with any separately decoded DIF/VIF alarm flags.
For original **#13** it emits `ERROR` without inventing an
application status. For original **#14** it emits `DRY ERROR`
and the six explicitly supplied XMQ alarm-duration fields.
Unsupported TPL CI, other status bytes, nonzero TPL
configuration and malformed applications are denied rather than
silently interpreted as valid. This is a narrow, reviewed subset,
not general TPL status semantics or radio admission.

`test_kamwater_tpl_status_upstream_parity.py` compares the actual C++
output with **8 original JSON fields across two new fixture cases**,
and tests fail-closed malformed TPL variants. Following the last
confirmed stage B subset (33 distinct fixtures / 117 compared fields),
this would bring partial-field comparisons to **35 distinct
fixtures / 125 field comparisons**, *subject to a green CI*.
The #13 `target_m3` value and full transport remain **outside**
this newly compared TPL-status contract. Other outstanding examples
include minomess wired #1/#2 and encrypted apator162 #19; a
positive partial-field result is not full JSON/transport parity.

## Stage B original wired Minomess #1/#2 (pending CI)

The remaining official Minomess fixture examples **#1 and #2** are
complete **wired M-Bus long frames**, not wireless T1/C1 payloads.
The new host-only `minomess_wired_frame.h` verifies the
`68 L L 68 C A 72 ... checksum 16` envelope, its exact byte-sum
checksum, bounded 12-byte variable-data header and supported
Zenner manufacturer/version/media before passing the clear
application to the existing bounded Minomess parser.
The gateway-supplied `01FD71` signed RSSI and `0274` actuality
duration, when present, are separately labelled metadata and never
mistaken for meter-owned readings. Unknown trailing records are
rejected, not silently skipped.

`test_minomess_wired_upstream_parity.py` passes both **original
complete source frames**, checking **13 original JSON values**
(two volumes, two dates, status, RSSI for each; actuality duration
only in #2). It also asserts rejection for mutated M-Bus length,
checksum, end marker and changed manufacturer with recomputed
checksum. The code is host-only and does not add a wired M-Bus
receiver to ESPHome or change the Heltec's SX126x radio.

With this new CI passing, **37 of the 39 original fixtures would
have at least one directly compared output field**, with
**138 partial-field comparisons** (previous 35/125 + 2/13).
This is still not full-output parity or authorization to activate
batch2 drivers. The remaining uncovered source examples are the
encrypted `apator162 #19` and the deliberately rejected
`kamwater #9` whose CCM tag fails; the latter is already covered
by a negative security regression.

## Stage B original Apator162 #19 AES-CBC mode 5 regression (pending CI)

Original public `apator162.xmq` fixture **#19** is a wireless
short TPL `7A` telegram using **OMS AES-128-CBC mode 5**
(`TPL CFG=0x8530`, three encrypted 16-byte blocks), with the
public XMQ all-zero 16-byte test key. It is **not** AES-CCM,
does not use a cryptographic authentication tag and must never
be represented as cryptographically authenticated.

The host-only `oms_tpl_cbc_mode5_openssl.h` derives the CBC IV
from the original CRC-stripped DLL manufacturer, identifier,
version, medium and repeated TPL access number; it checks the exact
encrypted block count and `2F2F` decrypted plaintext marker.
The separate `test_apator162_upstream_cbc_mode5.cpp` compares the
**complete decrypted manufacturer application** byte for byte with
independent original-fixture plaintext, then runs the existing
bounded Apator IXML reader and checks **`total_m3=7.512`** from
the original XMQ JSON. It also checks refusal on missing key,
unsupported CI/configuration and corrupted first CBC block/IV.
This is a **one-field source comparison pending GitHub CI**.

**Security boundary:** `2F2F` is only a weak plausibility check.
A later ciphertext modification may preserve this marker, and
the current IXML parser does not validate the manufacturer-specific
trailing checksum. The host-only CBC path must not be connected to
any live sensor or RF publication. A fully verified wireless DLL,
configured meter identity and independently justified application
integrity/admission policy would still be required.

If the CI is green, source fixture coverage with *some* positive
output field becomes **38/39**, with **139 positive field
comparisons** (previously confirmed 37/138). The remaining
`kamwater #9` has a deliberately **invalid AES-CCM tag** and
must remain a negative security test, never a positive decoding
case. Thus a blanket claim of 39/39 positive decoding would be
misleading. Full JSON parity and real-firmware integration are
still outstanding.

## Stage B evidence-based complete-field and security gap audit

`test_batch2_field_gap_audit.py` now enumerates **every original
source JSON** for the 39 upstream fixtures. Excluding only identifying
metadata, timestamp, source-driver name and the deprecation note,
it discovers **181 distinct per-case functional JSON fields**.
A field is credited only when a named existing C++-backed golden test
explicitly compares that field. The count is **139 compared and
42 NOT_COMPARED**; the test pins the per-case totals and keeps full
driver parity and firmware enablement explicitly false.
This is an **inventory of existing source/test contracts**, not an
additional decoder replay or a passing 139-field integration suite.
The 38/39 positive-fixture metric is also *partial field coverage*
rather than 38 complete valid JSON results.

**Key specific gaps** (machine-readable per-case in the new CI report):

- Minomess #3-#6: four status JSON values not yet compared.
- Kamwater #2/#3/#4/#7/#8: status and duration parity missing on
  compact or alternate already-clear cases; **#3 total_m3** not
  credited because no existing C++ golden test asserts that field.
- Kamwater #10: `TEMPORARY_ERROR` from the TPL status is not
  yet checked as complete field output, although CCM and volumes are.
- Kamwater #13: `target_m3` still not compared via the TPL status path.
- Kamwater #9: four JSON output fields deliberately NOT_COMPARED;
  its source CCM tag is invalid, so publishing its fallback zeros
  would violate the secure rejection policy.

**Security/production gaps that cannot be settled by JSON field
counting**: real wireless DLL CRC/identity before decrypting
`apator162` CBC mode 5; no cryptographic authentication of that
legacy mode; validation of the proprietary Apator application tail
checksum; reliable per-meter compact template/reference lifecycle
for Kamwater TPL79, including previous full-frame CRC and current
transport integrity; T1/C1 RF-admission, unconfigured packet policy,
bounded runtime/resource and end-to-end publication tests for
batch2 drivers. All three remain **pending** in the native driver
registry. Existing `mkradio4` and `fhkvdataiii` firmware paths
stay unchanged.

The standalone script additionally creates a JSON audit report
in CI (not a user-meter report), makes no driver registry edits and
blocks accidental inflation if an expected source field disappears.
The previous The previously reported **139 cumulative positive field checks**
are a different metric from the **139 distinct functional JSON
fields** credited here; the identical totals are coincidental.
Upstream Minomess #3 and Kamwater #4 omit `target_m3` from JSON:
native checks of absence are useful negative assertions, but cannot
count as positive comparisons against existing JSON keys.
The audit explicitly excludes both missing fields and retains
per-driver totals: Minomess 28/32, Kamwater 92/130 and Apator162 19/19.

## Kamwater #3 leading three bytes: host-only golden regression correction

The original source NOKEY fixture #3 puts `468878` before its
`02FF20`/volume/flow DIF/VIF records. Treating all bytes as an
ordinary application record stream causes the generic clear-record
decoder to reject it. The host-only fixture CLI now accepts **only**
this explicit three-byte prefix in its separate
`kamwater_prefixed` test mode and then runs the unchanged bounded
generic decoder. The source JSON's `total_m3=20.015` and absence
of `target_m3` are compared in the existing CI test; an altered
prefix must fail without any readings. This fixture-specific test
adapter does **not** imply that arbitrary radio telegrams with a
similar prefix may be stripped, nor does it authorize firmware
integration. The test has not yet been confirmed by new CI.

## Stage B explicit Minomess status and Kamwater #3 total-volume regression

The compiled batch2 clear-fixture C++ CLI now prints `status=OK`
for Minomess **only when the already-decoded, mandatory `02FD17`
status word is exactly zero**. Four original Minomess clear
application fixtures **#3–#6** compare that value against source
XMQ JSON. Nonzero status masks are not translated to invented text.

The same test suite now compares **Kamwater #3 `total_m3`**
directly from its original clear application against source JSON,
instead of crediting a volume that had not been asserted. Test
inventory expands from seven to eight clear-application examples
and from 22 to 28 assertions, including the absence assertions
that are not source JSON keys.

This adds **five uniquely verified source JSON fields**, pending CI,
raising the field-gap audit from **139/181** to **144/181**
(**37 functional fields still without a positive match**, including
four fields deliberately withheld for corrupted CCM sample #9).
`minomess` would be **32/32**, `kamwater` **93/130** and
`apator162` **19/19**. This is still only partial host-side
parity. Firmware admission remains blocked.

## Stage A — native/host algorithm consolidation, first slice (CI pending)

The live path is `NativeReceiverProbe` -> CRC/security/identity gate ->
`decode_configured_plain_frame` -> generated XMQ `decode_driver_payload`
-> configured sensors. The approved manifest remains unchanged:
`fhkvdataiii`, `mkradio4`, `iperl`, `hydrus`; the Stage B
`minomess`, `kamwater` and `apator162` remain
`BATCH2_PENDING`, not selectable for sensor publication.

**First consolidation:** Apator162's existing bounded manufacturer
application parser now has one implementation:
`experimental/xmq/portable_cpp/apator162_ixml_fields.h`.
The old `experimental/semtech-radio/host_only/apator162_ixml_fields.h`
contains only a forwarding include, so every existing original XMQ
host test uses the same parser. The ESP-IDF ESP32-S3 C++ probe now
includes that shared header and embeds the original public
already-clear #19 sample (7.512 m3), including a truncation check.
The `native-sx126x-adapter.yml` workflow now has a dedicated
`stage-a-espidf-s3-portable-link` job on our current branch using
the ESP-IDF v5.5.6 ESP32-S3 toolchain. It generates only the unchanged
live Techem XMQ tables, then compiles/links the existing portable
C++ probe with the shared Apator162 application parser. This CI
**compiles/links** the test, but does not run
`app_main` on physical hardware. This is not full telemetry parity
or an authorization to decode live encrypted frames.

The new CI guard checks this single-source architecture, the
shared-header copy performed by the ESPHome generator, and the
unchanged pending-driver manifest. No RF hooks, YAML, active
`mkradio4` or `fhkvdataiii` code were changed.

**Remaining Stage A mapping:** shared code already exists for
DIF/VIF, IXML runtime, Minomess clear application and Kamwater
clear/compact records. Host-only pure semantics still to assess:
Kamwater status, numeric fields, TPL status and Minomess wired
envelope. Host-only OpenSSL AES-CBC/CTR/CCM and transport
ingress must not be promoted without an ESP-IDF mbedTLS backend
and explicit integrity/admission gates. Keep original
`wmbusmeters` behavior as the reference, not a copied second
radio stack. New drivers remain disabled until end-to-end checks.

## Stage A — Kamwater shared status/numeric application algorithms (CI pending)

Following the green first Stage A ESP-IDF build, the original
`kamwater_status_semantics.h` and `kamwater_numeric_semantics.h`
implementations were moved **without changing decoding semantics** into
`components/wmbus_native_sx126x/`. Their old paths under
`experimental/semtech-radio/host_only/` are **forwarding includes**,
so existing upstream XMQ host golden tests now exercise the exact native
header implementations rather than duplicate copies.

The ESP-IDF ESP32-S3 C++ probe includes both native headers using an
explicit CMake include directory. It embeds upstream clear-application
sample #1 to assert `status=DRY` and `time_dry=22-31 days`; the
already-clear DIF/VIF portion of sample #3 asserts
`max_flow_last_day_m3h=0.317` and
`min_flow_temperature_last_day_c=2`. A truncated status sample
must fail and leave the reading empty. These checks are compiled into
the ESP-IDF `app_main` probe, **not executed on the Heltec by CI**.
The standalone host goldens compare more fields in the same shared
decoders. Neither test authorizes radio publication.

The extended Stage A structural CI guard checks that each algorithm
has just one native source, the old paths only redirect, ESP-IDF build
paths include the native component, and the meter driver manifest
**still excludes all three Stage B drivers** from production dispatch.
No `NativeReceiverProbe` decode paths, YAML selectors, T1/C1 radio
ownership or live `fhkvdataiii`/`mkradio4` logic changed.

**Remaining in Stage A:** host-only TPL status interpretation and
Minomess wired-envelope code; host OpenSSL AES-CBC/CTR/CCM plus
crypto-admission logic are a separate trust-boundary problem and must
not be promoted as ordinary application decoding. Original
`wmbusmeters` golden fixtures remain the compatibility reference.

## Stage A — Kamwater TPL 0x7A and wired Minomess consolidation (CI pending)

After the previous Stage A native/host shared status and numeric code passed
CI, two further **unchanged** C++ algorithms now have one source in
`components/wmbus_native_sx126x/`:

- `kamwater_tpl_status_semantics.h` accepts only the reviewed already-clear
  short TPL `CI=0x7A` with status byte `0x02` and zero configuration
  from original Kamwater #13/#14. The original host header merely forwards
  to this file. No arbitrary TPL status values or cryptography were enabled.
- `minomess_wired_frame.h` validates original M-Bus `68` wired length,
  SUM checksum, meter identity, clear application and gateway-side
  RSSI/actuality. This checksum is **not wireless DLL CRC**. The old
  host-only file forwards to the same implementation; the native radio
  ingress **does not** route wireless traffic to it.

The dedicated real ESP-IDF ESP32-S3 compile job now includes both headers
and embeds original fixture Kamwater #14 (expected `DRY ERROR`) and
original wired Minomess #1 (`total_m3=57.597`, gateway RSSI -86).
Truncated TPL and corrupted M-Bus checksum must reject. As before, the
CI compiles/links `app_main` but does not execute it on physical hardware;
original-XMQ host golden parity executes independently.

The Stage A CI boundary guard verifies that only the native files hold
these algorithms and that they have **not** been added to live
`NativeReceiverProbe` or `decode_configured_plain_frame`. The
driver manifest remains unchanged and batch2 driver publication blocked.

Remaining Stage A work: classify and extract reusable platform-independent
crypto primitives and transport admission without copying OpenSSL into
the firmware or altering the live Techem code. Maintain original
`wmbusmeters` fixture behavior as the compatibility reference.

## Stage A — ELL AES-CTR keyless algorithm consolidation

This slice consolidates **only** the upstream-compatible ELL II
keyless IV derivation and big-endian 128-bit counter increment into
`components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h`.
The old `experimental/semtech-radio/host_only/kamwater_ell_ctr_iv.h`
is a forwarding include. The OpenSSL host experimental ELL decoder
uses the same function instead of defining a second counter
increment algorithm. No security policy or AES decryptor was
changed.

A CI structural guard verifies a single IV/counter implementation,
forwarding include, and that the shared header contains **no
OpenSSL dependency**. The original host-side ELL IV and synthetic
encryption tests remain the semantic reference.

**Important limitation:** the proposed additional ESP-IDF probe
code was not applied; this slice has not added an explicit keyless
IV fixture into the `app_main` ESP-IDF self-test. Compile-time
availability of the header in the native component and all host
fixture tests must not be confused with a successful end-to-end
encrypted RF decode. OpenSSL decrypt, CCM authentication and
publishing are still host-only/blocked. The Stage B drivers
remain pending, and installed Heltec firmware is unchanged.
CI for this change is awaiting confirmation.

## Stage A closeout checkpoint

The final single-source/CI boundary inventory is now maintained in
[`STAGE_A_CLOSEOUT.md`](STAGE_A_CLOSEOUT.md). This closes **source
consolidation only** once the new CI passes. All earlier entries labeled
`CI pending` are historical notes, not claims that unverified
decryptors or new drivers run in firmware. No live algorithm, SX126x
radio, YAML or Heltec firmware changed as part of the closeout.

## Stage B — fast evidence-first parity pass (CI pending)

The previous Stage A consolidation is complete and user-confirmed green.
The first Stage B change adds original upstream `kamwater.xmq` fixture #3
to the **existing compiled native C++ clear-application status decoder**.
The original already-clear application has an explicitly reviewed
`46 88 78` prefix; only that exact prefix is removed in the host
test. No generic RF prefix acceptance is enabled.

The native CLI output now must match **six additional original JSON
fields**: `status`, `current_status`, `time_dry`,
`time_reversed`, `time_leaking`, and `time_bursting`.
These checks extend `test_kamwater_status_upstream_parity.py`
from 5 fixtures / 34 fields to **6 fixtures / 40 fields**. The
audit of distinct source JSON field occurrences moves from
**144/181** to **150/181** across 39 fixtures (38 positive partial,
1 negative-only authentication). Those counts remain **partial**
comparisons from separate decoders, not full driver parity.

The Stage B drivers are still `BATCH2_PENDING` and unavailable for
live YAML or ESPHome sensor publication. No AES decryption, radio code,
ESP-IDF dispatch or existing Techem driver was changed.

## Stage B — compact TPL79 legacy statuses (CI pending)

Using the already shared native `kamwater_tpl79_probe.h` and
`kamwater_status_semantics.h`, the host fixture CLI adds a
`kamwater_compact_status` operation. It first checks the declared compact
profile signature, reconstructs DIF/VIF records, and validates the
reconstructed-record data CRC; **only then** decodes status/duration.
It supports only the explicitly reviewed legacy status form.
Corrupted compact CRC fails with no emitted fields.

The fixture parity test compares the exact original
`kamwater.xmq` #2, #4 and #8 clear compact applications against
all six source JSON status/duration fields per fixture. This is
**18 additional source JSON field assertions**, moving partial
evidence from **150/181 to 168/181** across the same 39 original
fixtures, contingent on the new CI passing. The original compact
volume tests (#2/#4/#7/#8) remain intact, including #7's separately
validated full-reference CRC handling.

No RF decryption, ELL identity admission, C1/T1 publication, firmware
driver manifest or live Techem decoding has changed. This is not
full-driver parity and must not activate the three pending drivers.

## Stage B — compact #7 and clear short TPL #13 (CI pending)

Next evidence-first increment uses existing shared portable decoders, not
new radio or plaintext assumptions:

- **Kamwater #7:** expand the original already-clear TPL79 compact
  application using the known format signature and the original #6
  previously validated full application as the **CRC reference**.
  Compare seven modern status/duration JSON fields using the existing
  shared `kamwater_status_semantics.h`. Reject corrupted compact data
  CRC or a mutated reference without emitting fields.
- **Kamwater #13:** behind the original clear CI=7A / status=ERROR
  short TPL, use the shared bounded standard DIF/VIF record reader
  to compare `target_m3=0.001`. This original fixture has **no
  current total** and no artificial total should be inferred. Truncated
  target data fails closed.

The corresponding host fixture CLI modes remain test-only and are
not connected to the ESPHome native receiver. If CI passes, the
distinct **partial** XMQ JSON field comparisons become **176/181**.
The five fields without positive comparisons then consist of
four fields from original **Kamwater #9** (an intentionally rejected
invalid authenticated frame) and `status` from Kamwater **#10**
(valid encrypted CCM source fixture). Neither group may be marked
positive without the correct security-aware verification.

**All three batch2 drivers stay pending.** Full-frame, firmware,
cryptographic and publication parity have not been approved. The
original Techem decoders and SX126x radio are unchanged.

## Stage B — authenticated source status + integrated host ingress (CI pending)

The original `kamwater.xmq` #10 public CCM fixture has a valid
authenticated short TPL with `status=0x10` and expects
`TEMPORARY_ERROR`. The host-only exact-subset status interpretation
is asserted **only after a successful CCM authentication**. Unknown
status values fail closed. The source #9 fixture has a corrupted
CCM authentication tag; it must continue to reject the entire
application with no measurement output.

The `test_kamwater_upstream_ccm_openssl.cpp` host regression now also
wraps the **original public** #9/#10 encrypted ELL/TPL content in
explicitly **synthetic** complete format-B DLL frames so that the
existing `kamwater_secure_ingress.h` verifies frame length/CRC,
configured identity, CCM authentication and transactional output
clearing. Negative cases include an invalid CCM tag, DLL CRC
tampering and wrong configured meter identity. This is **not** a
recorded RF transmission, not an ESP-IDF mbedTLS implementation,
and not a production receiver or HA sensor parity test.

`test_batch2_field_gap_audit.py` now expects **177/181 positive
partial source-field comparisons**, with **four source JSON fields
from deliberately invalid #9 not claimed as decoded**. The
separate `test_batch2_parity_ledger.py` tracks the same field
distinction, but **all 39 full-driver end-to-end parity cases remain
NOT_RUN**. The dedicated acceptance checklist is
[`STAGE_B_GATE_STATUS.md`](STAGE_B_GATE_STATUS.md).
**This entire new change set is pending fresh CI validation.**
All three experimental batch-2 drivers remain disabled.

## Next implementation steps

1. Develop a bounded minomess record decoder and compare normal, sentinel,
   date and invalid-DIF cases to the original `json` reference values.
2. Implement compact profile expansion as a shared standard feature before
   enabling kamwater; reject unknown signatures rather than guessing.
3. Extend the general IXML runtime for apator162 grammar constructs and
   verify all upstream tests. Avoid a one-off byte-offset parser.
4. For all three, authorize live RF decoding only when the relevant CI,
   identity and encryption checks pass. Do not treat upstream post-decrypted
   `NOKEY` fixtures as proof that raw encrypted frames are plaintext.

No actual meters, personally captured RF telegrams, AES keys or local YAML
settings should be added to this repository.
