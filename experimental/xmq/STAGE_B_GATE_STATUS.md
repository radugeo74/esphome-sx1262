# Stage B gate status — full-driver acceptance not yet complete

Updated 2026-11-06 for branch `experimental/xmq-multidriver-auto`.
This is an evidence ledger, not a release approval. The 177/181 field
and host CCM regression calup was owner-confirmed green. The newer ESP-IDF
mbedTLS build-probe calup still requires a fresh CI run.

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
