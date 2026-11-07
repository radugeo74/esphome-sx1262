# Native XMQ — batch 1 status

This batch uses only upstream public `.xmq` driver fixtures; no personal
meter IDs, private radio captures, or AES keys are stored in this repository.

| Driver | Live native status | Verified boundary |
| --- | --- | --- |
| `iperl` | Cleartext CI=7A only | SEN 68/7C; mode-zero TPL; total and max flow; fail closed for encrypted/invalid |
| `hydrus` | Cleartext CI=72/7A only | DME/HYD; mode-zero TPL; standard DIF/VIF quantities; encrypted frames rejected |
| `mkradio4a` | **Configured secure AES only** (CI `42401195133` green; device gate OPEN) | TCH/95/37 converter + HYD/FE/06 or WEH/FE/07 long TPL, configured two IDs, AES Mode-5 and validated target/date; both public clear applications wrapped in independent synthetic AES/CRC tests |

The generated and portable live driver catalog is controlled by
`experimental/xmq/tools/native_driver_specs.py`.
`SECURE_ONLY = ("mkradio4a",)` is intentionally excluded from
generic plaintext `SUPPORTED`, `CONFIGURABLE` and
`xmq_native_compiled.h`, but `secure_meters` now accepts it
with an explicitly configured key and converter ID. A valid
CRC, AES-CBC padding or 2F2F marker alone is insufficient
to authenticate an encrypted C1/T1 frame.

Run the batch contract as part of the native SX126x CI workflow:
`experimental/semtech-radio/test_xmq_batch1_driver_contract.cpp`.

The same workflow still tests `fhkvdataiii` and `mkradio4` regressions and
compiles the Heltec laboratory firmware **without flashing a device**.

## Exit criteria before the next batch

- Native packet tests and Heltec build jobs green on the final commit.
- Existing T1 `on_verified` accepts all CRC-verified frames regardless of
  YAML configuration, while XMQ decoding and sensor updates require a
  configured, authorized meter.
- No encrypted live RF data is passed to the cleartext APIs.
- **Implemented in experimental software:** configured `mkradio4a`
  secure long-TPL route with explicit AES-128 key and matching
  DLL/TPL identities; test source applications under independent
  synthetic encryption and DLL CRC. **Still required:** real
  MK Radio4a radio capture, actual-key validation, HA sensor
  observations, and authorized canary/rollback before field use.
