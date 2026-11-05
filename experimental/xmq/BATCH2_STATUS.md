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

**Compact-profile pilot (host-only):** Added a shared, fixed-capacity `compact_dif_vif.h` expander which requires an explicitly supplied, validated DIF/VIF template. Two official Kamwater templates (`02FF2004134413615B6167` and `02FF20041392013BA1015B8101E7FF0F`) are each compared byte-for-byte with paired public compact/full examples, with readings also checked where present. The wrapper does not automatically choose a profile, validate format signatures, decrypt ELL, or publish sensors. Unknown and truncated compact content fails closed.

**Imported as source and regression fixtures, not yet compiled into live RF
sensor decoding.** They are enumerated in `BATCH2_PENDING`, not `SUPPORTED`.
The auto registry may identify their manufacturer/version/type metadata but
does not authorize decode or publication. The ESPHome YAML validator must
reject them as `meter_drivers` until their complete decode contracts pass.

`test_batch2_upstream_fixtures.py` checks each original fixture inventory
and blocks accidental enablement; CI also retains all established T1/C1 and
old driver regressions.

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
