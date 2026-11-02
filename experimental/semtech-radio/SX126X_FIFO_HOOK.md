# IMPORTANT: proven long-frame algorithm located (2026-11-01)

This section supersedes the earlier speculative GetRxBufferStatus approach.
Source: https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/transceiver_sx1262.cpp

The working SzczepanLeon receiver already solves the 256-byte FIFO problem
in `SX1262::read_in_task()` (around lines 126-161):

- IRQ trigger is `SYNC_WORD_VALID`, **not RX_DONE** (line ~76).
- A receiver task reads 3 encoded bytes, calls
  `Packet::calculate_payload_size()`, then reads the rest (see
  `components/wmbus_radio/component.cpp`, `Radio::receive_frame()`).
- During read, the task polls register `0x0803` (`RX_ADDR_PTR`) and
  updates `0x06BB` (`RX_TX_PLD_LEN`) to `write_ptr-1`, avoiding the
  255-byte fixed-length stop while receiving circular FIFO data.
- Read pointer is `offset + bytes_already_read` modulo 256; wrap reads
  are split at the end of the ring; RX is re-armed after each frame.
- The user's known-working branch also has the 3-byte / L-field / remaining
  bytes flow, though its current `transceiver_sx1262.cpp` differs from
  upstream in its interrupt and read details. **Do not copy one source
  blindly over the other.**

## Enforced architecture decision

**Retain official ESPHome 2026.10.1 `sx126x` as the single hardware owner.**
Adapt the above *algorithm inside the official SX126x component* in an
opt-in experimental patch. Do not add a separate transceiver, a second SPI
radio controller, or a replacement external radio implementation.

Suggested narrow implementation:
1. Add an opt-in wM-Bus T1 streaming mode to the official SX126x C++ class.
   No change to default behavior, other protocols, or normal RX_DONE listener.
2. Only in this mode configure GFSK fixed length 255 and enable sync-valid
   IRQ; on that event start a **nonblocking bounded** receive state machine
   or safely scheduled worker owned by SX126x.
3. Read the already-proven 3-of-6 first encoded bytes, determine full length
   using `t1_packet_framing.h` (or equivalent existing `Packet` formula),
   read remaining encoded bytes progressively with pointer and payload-length
   register technique noted above, accounting for 256-byte wrap.
4. Pass the completed data into `t1_fifo_contract.h` and the verified
   CRC/header path, then to XMQ. No invalid or unconfigured meter publishes.
5. Restore the original payload length, clear IRQ, re-arm RX, with explicit
   recovery on timeouts, symbol errors and FIFO overflow.
6. Keep CI compile checks, then test radio acquisition against real T1 meters
   on a safely recoverable Heltec before considering production migration.

The existing `fifo_progress_probe` using GetRxBufferStatus before RX_DONE
is now **noncritical / exploratory**. It must not block implementing the
documented pointer-based stream approach. No claim of working on Heltec
without physical logs.

---

# Native ESPHome SX126x T1 progressive FIFO integration

This is a **design checkpoint**, not a working radio driver. Production Heltec
firmware must remain untouched.

## Source inspection (ESPHome 2026.10.1)

In `esphome/components/sx126x/sx126x.cpp`, `SX126x::loop()` (around
lines 365–395) returns until DIO1 is high. It reads `RADIO_GET_IRQSTATUS`,
clears IRQ bits, and only if `IRQ_RX_DONE` is present reads
`RADIO_GET_RXBUFFERSTATUS`, then `read_fifo_()` and calls listeners.
`SX126x::set_mode_rx()` (around line 427) enables
`IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR`. No RX FIFO-progress
callback is available to the external wM-Bus listener.

Source: https://github.com/esphome/esphome/blob/2026.10.1/esphome/components/sx126x/sx126x.cpp

## Radio boundary that must not be glossed over

SX126x hardware has a 256-byte RX buffer and ESPHome caps a packet at 255
bytes. A Format A T1 DLL with decoded length 255 needs
`ceil(255 * 12 / 8) = 383` encoded bytes. Therefore receiving a full-size
T1 telegram **cannot** be implemented by reading FIFO only after RX_DONE.
The FIFO must be drained progressively and the radio's packet completion
strategy must match the L-derived frame length.

Even a short T1 telegram must not be confused with a Semtech GFSK
variable-length packet: `payload_length: 0` does not enable incremental
reading of wM-Bus frames. `payload_length: 255` is also not a
length-from-L radio decoder.

## Required future implementation

1. Make an experimental, **single-owner SX126x** receiver patch/fork derived
   from the exact ESPHome 2026.10.1 implementation. Do not instantiate a
   second SPIDevice or legacy radio controller on the same hardware.
2. Add an explicitly opt-in GFSK progressive receive mode. Its radio-owned
   implementation needs a safe FIFO-watermark/progress notification and a
   bounded read offset with handling for circular-buffer wrap/overflow.
3. At a new frame boundary call `FifoContract::begin_frame()`. For each
   contiguous new byte span call `feed_bytes(bytes, size)`. Call
   `end_frame()` on timeout/reset. On `RxState::Verified`, hand off a
   copied verified DLL frame to the application layer; never retain pointer
   aliases to radio memory. Only the radio owner may change RX mode,
   sync configuration, IRQ mapping or FIFO offsets.
4. Test re-arming, loss of sync, invalid 3-of-6, CRC failure, variable
   lengths, interruptions, timeout and long frames before attempting XMQ.
5. Verify Heltec TCXO supply and physical T1 sync/bit alignment on a
   recoverable test unit. Do not flash a production unit based solely on CI.

This contract's standalone C++ tests prove software state transitions;
they **do not** prove receive capability of upstream SX126x.
