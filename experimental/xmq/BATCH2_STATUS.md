# Batch 2 — initial source import and acceptance gates

Official upstream: `wmbusmeters/wmbusmeters`, branch `master`, directory
`drivers/src`. The three complete original XMQ files, including their public
regression fixtures, are present in `experimental/xmq/drivers`.

| XMQ file | Upstream source blob SHA | Key decoding obstacle |
| --- | --- | --- |
| minomess.xmq | 418c3a209ccbacb42909436e7af53034bd733c0c | 8C0413 falsely encoded as BCD: reinterpret binary volume while preserving the null/sentinel case; flags and dated targets |
| kamwater.xmq | 0cd7d5c6fbcb9d3381b7d18966ebab1a53e12485 | Compressed/compact DIF/VIF profiles, status variations and security |
| apator162.xmq | 116907a3e22314ccc52a0f4cd1ab8ddb10eaab27 | Manufacturer-specific IXML grammar: repetition, alternatives, checksums and FF padding |

## Status

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
the expected total becomes **112 compared fields across 31 distinct
original fixtures**, *pending the new CI run*. This is not full
driver-output parity: other Kamwater status variants, history,
manufacturer-specific fields and encrypted-radio fixtures remain.
The host-only source is not imported into production firmware.

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
