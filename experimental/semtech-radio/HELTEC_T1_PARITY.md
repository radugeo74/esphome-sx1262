# T1/C1 native-radio parity before flashing Heltec

The known-working custom `transceiver_sx1262.cpp` uses **868.950 MHz, 100 kbit/s,
50 kHz deviation, 234.3 kHz bandwidth and sync 54 3D**. Its packet parser
classifies C1 when its first collected byte is 54; otherwise it attempts T1
3-of-6 decoding. Thus the old driver does *not* prove that selecting
`link_mode: T1` alone will make the native ESPHome listener receive T1.

## Crucial differences still needing hardware evidence

- Old transceiver uses **fixed-length RX = 255 bytes** and reads an initial
  sample followed by the rest once L is inferred. The current native
  fixture uses `payload_length: 0`, which selects variable-length packet
  handling in ESPHome's radio configuration. It may consume a length byte
  that wM-Bus does not transmit as a separate radio length prefix.
- Old code writes a six-byte 54 3D 00 00 00 00 sync register with a **16-bit
  sync length**. The native callback's post-sync bytes must be captured
  before their alignment is assumed.
- No claim of simultaneous T1 and C1 operation from a single native
  listener has been demonstrated.
- The existing native T1 framing helper expects **3-of-6 at the first input
  bit**. We must verify that against real FIFO bytes, including potential
  sync residues or bit shifts.
- The custom transceiver sets **DIO3 TCXO to 3.0 V**, whereas the trial
  YAML currently requests **1.8 V**. Verify the correct board electrical
  setting before flashing.
- The current test YAML is built without Wi-Fi, OTA and API. Flashing it to
  the deployed Heltec would interrupt all existing 12 entities until
  rollback. Do not deploy until fallback/recovery is prepared.

## Required next checks

1. Compare actual official ESPHome SX126x FIFO/packet semantics, fixed vs
   variable length and sync handling, against the custom receiver.
2. Provide a **hardware-safe, explicitly diagnostic** T1-only configuration
   with confirmed board pin/TCXO settings and a rollback plan.
3. Receive and log real packet lengths, raw first bytes, RSSI and CRC
   classifications; never publish sensors from invalid frames.
4. Only after T1/C1 reception parity connect the XMQ registry and remove
   the custom SX1262 driver.

All 12 meters supplied in the user's working YAML are configured T1.
This note documents a migration blocker; it is not a report of tested
on-device behavior.


## Confirmed official SX126x FSK receive limitation (source inspection)

In the official ESPHome SX126x source, `set_packet_params_` sets byte 5 to
`0x00` (fixed) when `payload_length_ > 0`, and `0x01` (variable)
otherwise. A variable FSK packet uses the *radio's length-prefix
interpretation*, not the wM-Bus T1 3-of-6 L-field framing. The official
`loop()` waits for RX_DONE, reads RADIO_GET_RXBUFFERSTATUS, resizes the
packet to the reported length and then invokes listeners.

Implications for T1 (based on code, **not** a verified field test):
- `payload_length: 0` is not a reliable wM-Bus T1 solution: there is no
  independent Semtech variable-packet prefix preceding a T1 telegram.
- `payload_length: 255` configures fixed RX size, but the callback may
  only be invoked after 255 radio bytes rather than after a shorter
  telegram. It does **not** implement the legacy mid-frame L-field
  length adjustment.
- Changing just the YAML cannot establish equivalent T1 operation.
  Need a receiver enhancement in upstream `sx126x` (bounded
  FIFO/progressive access, dynamic per-frame length or IRQ-driven
  completion), or a separate compatible non-owning API that remains
  faithful to ESPHome's official SPI owner. Do not create a second
  SX1262 driver controlling the same SPI/IRQ.
- Preserve a recoverable production firmware and do not flash the
  present laboratory YAML to the sole active Heltec.

Reference implementation inspected: ESPHome SX126x source `set_packet_params_`
and `loop()`: https://api-docs.esphome.io/sx126x_8cpp_source
