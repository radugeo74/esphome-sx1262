# Stage A closeout — shared C++ algorithm, native ESP-IDF boundaries

**Scope:** consolidation and source ownership, **not** complete wmbusmeters
semantic parity, encrypted-frame admission, or live driver enablement.
The user-reported previous Stage A CI passed; this closeout change itself
requires a fresh CI run before being marked green.

## One source of truth

| Logic | Canonical source | Compatibility tests |
|---|---|---|
| Apator162 already-clear IXML field subset | `experimental/xmq/portable_cpp/apator162_ixml_fields.h` | Old host include forwards to portable source |
| Kamwater clear status/duration | `components/wmbus_native_sx126x/kamwater_status_semantics.h` | Old host include forwards to native source |
| Kamwater clear numeric | `components/wmbus_native_sx126x/kamwater_numeric_semantics.h` | Old host include forwards to native source |
| Kamwater limited already-clear TPL status | `components/wmbus_native_sx126x/kamwater_tpl_status_semantics.h` | Old host include forwards to native source |
| Minomess wired M-Bus envelope | `components/wmbus_native_sx126x/minomess_wired_frame.h` | Old host include forwards to native source |
| ELL AES-CTR keyless IV/counter | `components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h` | Old host include forwards to native source |

Existing shared DIF/VIF, compact records, Techem generated XMQ runtime,
Minomess wireless clear application, and Kamwater clear/compact parsing remain
in place; they were not rewritten in Stage A.

## CI evidence boundaries

- Original public `wmbusmeters` XMQ JSON fixtures are compared to the actual
  C++ host binaries via the existing golden tests.
- The dedicated `stage-a-espidf-s3-portable-link` job compiles and links the
  shared Apator162, Kamwater status/numeric/TPL, and wired Minomess headers
  with the ESP-IDF v5.5.6 ESP32-S3 toolchain. Its embedded `app_main`
  checks are **not executed on physical hardware** by the CI build.
- The keyless IV/counter has its original host-side regression and structural
  guards but **does not have its own ESP-IDF app_main fixture**. Its native
  header has no OpenSSL dependency; do not call that full on-device validation.
- `test_stage_a_shared_portable_boundary.py` verifies single-source
  includes, ESP-IDF compile wiring, unchanged driver manifest and missing
  live RF calls. `test_kamwater_host_only_isolation.py` permits the
  keyless IV/counter but rejects host-only decrypt/secure ingress in native.
- The ESPHome native receiver itself is still build-checked by CI; no new
  sensor publication or unreviewed crypto path has been added.

## Live safeguards preserved

- Official ESPHome SX126x native radio remains the only active radio owner.
- `fhkvdataiii` and `mkradio4` continue to use the preexisting generated
  XMQ runtime. `iperl` and `hydrus` are the other manifest-supported
  plaintext drivers.
- `minomess`, `kamwater`, `apator162` are **BATCH2_PENDING**:
  they cannot be selected via YAML or dispatched to live sensor publication.
- Wired M-Bus uses its own SUM checksum and is never promoted to wireless
  T1/C1 CRC validity. No host-only OpenSSL AES-CBC, AES-CTR, CCM or unverified
  decryption is enabled in the native receiver.
- A verified DLL CRC is not cryptographic authentication. End-to-end encrypted
  RF/identity/replay/compact CRC tests remain required for applicable modes.

## Next phase (not Stage A)

1. Expand shared DIF/VIF and IXML behavior to match remaining original driver
   fixtures. Preserve exact upstream sentinels, status, time and numeric
   semantics; measure field-by-field coverage.
2. Implement and verify cryptographic backends for actual ESP-IDF using vetted
   primitives and fail-closed admission. Verify the independent ELL/TPL
   integrity and security requirements; do not treat a CRC as authentication.
3. Test complete configured transport -> decode -> sensor publication per driver
   and radio mode. Only then promote `BATCH2_PENDING` individually.
4. Preserve the live Heltec configuration until a separately authorized,
   explicitly build-tested canary firmware is available.

**Stage A disposition:** source/CI consolidation implemented. Close only
after the updated GitHub CI is green; that does not imply Stage B parity.
