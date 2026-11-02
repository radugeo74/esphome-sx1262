# Native SX126x diagnostic (opt-in, zero meter publications)

First real integration milestone: a passive `SX126xListener` registered
against ESPHome 2026.10.1 official `sx126x`. The component is
`wmbus_native_sx126x`, with required `sx126x_id` to match the official
radio's `id`. It prints packet length, RSSI, SNR, first three bytes and
count. **It does not claim these bytes are valid wM-Bus frames.**

## Example fragment (NOT a working board YAML)

```yaml
# after configuring one official sx126x component with id: my_radio
wmbus_native_sx126x:
  sx126x_id: my_radio
```

Do not load the legacy wmbus_radio SX1262 concurrently on the same chip.
Do not copy or guess GPIO pin mappings: official sx126x requires a BUSY pin
and its own SPI/IRQ/TCXO/RF-switch configuration.

## Why observe packets before decoding?
The official `sx126x` packet-mode listener receives bytes extracted
from RX FIFO after the configured hardware sync word. The old
wmbus_radio::Packet converter expects T1/C1 radio bytes in a different
layout. A direct `Packet` call would silently misclassify or reject frames.

Further gating: determine exact sync configuration, reconstruct packet
boundaries, and verify valid T1/C1 parity against known reference captures.
Only then connect probe to radio-agnostic packet conversion and XMQ.

The `library.xmq` catalog at https://wmbusmeters.org/drivers/library.xmq.html
provides normalized application field templates (total_m3, target_m3, status,
etc.) and is useful later in driver decoders, not at the FSK RX boundary.
