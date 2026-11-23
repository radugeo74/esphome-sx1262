# XMQ standalone — isolated experimental branch

This branch is a separate migration line. The working Heltec Stage 6
firmware remains on `experimental/xmq-runtime-stage-a`.

## Scope implemented
- Pure XMQ decoding contract for **7 heat + 5 water logical configurations**
  through the two new-engine drivers, using public synthetic inputs.
- No calls to legacy `handleTelegram()`, legacy Telegram snapshots, or
  legacy sensor setters in the standalone processor.
- **Fail closed** on unknown security or unverified ciphertext.
- Host-only first gate; **no installable Heltec YAML or OTA image yet**.

## Remaining engineering gates before real 12-meter installation
1. Native CRC-verified T1/C1 radio callback directly into independent XMQ
   transport adapter, with **no legacy full Telegram decoder**.
2. Independent TCH proprietary A0/A2 plaintext/security classification.
   A2 structural signatures alone are **not** proof of unencrypted data.
3. Independent OMS TPL security and AES handling for every encrypted
   meter/mode actually present; prevent false verification.
4. Bind **all 12 real meter IDs** privately to separate XMQ entities and
   publish all existing HA fields, including RSSI; no secrets in Git.
5. Verify actual frames from all 12 meters, no missed updates, bounded
   loop time, heap stability, and OTA rollback.

No one should flash this branch while these gates are open.
