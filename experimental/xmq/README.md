# wmbusmeters XMQ / IXML feasibility prototype

**Status: Stage 0 — source packaging and reproducible fixture inventory only.**
This prototype does **not** yet execute XMQ drivers, interpret IXML, decode real telegrams,
or provide an ESPHome component. It does **not** establish ESP-IDF compilation or RAM use.

The production wM-Bus SX1262 implementation is unchanged. This lives only on
`experimental/wmbusmeters-xmq`.

## Upstream sources

Two original, GPL-3.0-or-later driver definitions copied from
https://github.com/wmbusmeters/wmbusmeters/tree/master/drivers/src :

- `fhkvdataiii.xmq` — Techem heat cost allocation, IXML payload decoder
- `mkradio4.xmq` — Techem water meter, IXML and calculated fields

Their embedded `tests { test { ... } }` blocks provide known telegram/expected-output pairs.
Record the upstream commit SHA before any production release so updates are reproducible.
Retain original copyright and license notices.

## Reproduce source embedding

```bash
python3 experimental/xmq/tools/embed_drivers.py
python3 experimental/xmq/tools/embed_drivers.py --check
```

Generator produces `experimental/xmq/generated/embedded_xmq_drivers.h`.
This header stores XMQ definitions as firmware constants, **not compiled meter decoders**.
The stage-0 cost is approximately the input text size plus C++ structure overhead;
actual flash use depends on the linker and whether a runtime references the data.

## Planned stage 1 and 2

1. Vendor or package the necessary upstream libxmq C sources under their licenses.
2. Build libxmq against ESP-IDF 5.5.x, inspect missing POSIX/libxml dependencies.
   The upstream xmq.h documents XMQ_NO_LIBXML, XMQ_NO_JSON, XMQ_NO_XMQ_PRINTING;
   verify these switches against real compilation rather than assuming support.
3. Port the dynamic-driver execution graph (IXML, expressions, field extraction,
   units, meter data) behind a narrow `decode(frame) -> fields` boundary.
4. Run official driver fixtures on a host first; then compile for ESP32-S3 and
   measure firmware size and peak heap; finally compare decoded water/heat
   readings from the same recorded telegrams against the existing implementation.
5. Only after parity, hook it into ESPHome without replacing the stable receiver.

**Important:** Using these XMQ files directly in ESPHome without a runtime will
not decode any meter; source embedding is only the first independent milestone.
