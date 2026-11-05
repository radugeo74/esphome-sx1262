# Native XMQ — batch 1 status

This batch uses only upstream public `.xmq` driver fixtures; no personal
meter IDs, private radio captures, or AES keys are stored in this repository.

| Driver | Live native status | Verified boundary |
| --- | --- | --- |
| `iperl` | Cleartext CI=7A only | SEN 68/7C; mode-zero TPL; total and max flow; fail closed for encrypted/invalid |
| `hydrus` | Cleartext CI=72/7A only | DME/HYD; mode-zero TPL; standard DIF/VIF quantities; encrypted frames rejected |
| `mkradio4a` | **Not live** (post-decryption-only) | Public NOKEY already-clear application decoded; live CI=72 encrypted converter frames **not** decoded/published |

The generated and portable live driver catalog is controlled by
`experimental/xmq/tools/native_driver_specs.py`.
`POST_DECRYPT_ONLY` is intentionally excluded from `SUPPORTED`,
`xmq_native_compiled.h`, and ESPHome `meter_drivers` validation. A valid
CRC alone is insufficient to interpret an encrypted C1 frame.

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
- For a future live `mkradio4a` driver: implement and validate AES/security
  processing and resolve converter DLL versus target TPL identity first.
