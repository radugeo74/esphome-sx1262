# Native SX126x migration — first code milestone

Experimental branch `experimental/semtech-sx126x-sx127x`.

The initial implementation adds `components/wmbus_radio/native_packet_input.h`,
a radio-agnostic, bounded and owning input boundary for packets supplied by
ESPHome's official SX126x `on_packet` callback. This input layer is explicitly
future-compatible with SX127x.

Reference API verified against ESPHome 2026.10.1:
`SX126xListener::on_packet(const std::vector<uint8_t>&, float rssi, float snr)`.

This is a **foundation**, not a running native radio bridge: no ESPHome YAML
wiring or callback registration yet; no claim of over-the-air decoding. The
legacy receiver continues unmodified.

Host test:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I. experimental/semtech-radio/test_native_packet_input.cpp -o /tmp/test-native-packet-input
/tmp/test-native-packet-input
```

Next integration milestone: register a native SX126x listener and enqueue
complete received bytes for the existing packet converter. Avoid reusing the
two-stage SPI read flow for complete native packets, and prevent activating
both radio owners on the same chip. Prove packet framing and C1/T1 parity
before enabling the adapter in a production YAML.
