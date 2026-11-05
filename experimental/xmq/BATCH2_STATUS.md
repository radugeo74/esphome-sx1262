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
