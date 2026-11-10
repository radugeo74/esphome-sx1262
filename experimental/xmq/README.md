## Supercom587: generare și paritate pentru toate câmpurile XMQ (2026-11-10)

- Driverul `supercom587` utilizează acum modele și reguli DIF/VIF
  generate din `supercom587.xmq` în
  `xmq_generated_clear_profiles.h`, inclusiv offseturile determinate
  din **ambele exemple originale**. În firmware se validează modelul
  SON/3C/06 (apă caldă) și SON/3C/07 (apă rece), CI=7A, profilul
  necriptat, ID-ul configurat și forma de 163 octeți.
- Datele din JSON-ul oficial sunt reproduse integral pentru măsurare
  și diagnostic:
  `total_m3` = 5.548 / 4.989,
  `software_version` = `010002` pentru ambele,
  `status` = `OK` pentru ambele.
  `software_version` păstrează zerourile inițiale din cele 3 octeți
  BCD și este publicabil prin `xmq_text_sensors`; `status` este
  emis doar pentru indicatori de eroare 0. Indicatori necunoscuți,
  BCD invalid, înregistrări lipsă sau antet criptat sunt respinși.
- `name` este eticheta configurată în ESPHome, `id` este citit
  din antet și corelat cu configurația, iar `media` corespunde
  codului de aparat 06/07. `timestamp=1111-11-11...` din
  fixture este **un placeholder al testului**, nu timpul emis
  de contor: în Home Assistant se folosește timpul real al citirii.
- Exemplu pentru un contor real, după înlocuirea ID-ului fictiv
  `12345678` cu identificatorul lui și configurarea
  secțiunilor existente `wmbus_native_sx126x`:

```yaml
  meter_drivers:
    - meter_id: "12345678"  # exemplu public XMQ, nu un ID de productie
      driver: supercom587   # sau auto, cu verificare MVT/ID
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Supercom 587 consum total"
        unit_of_measurement: "m³"
        accuracy_decimals: 3
  xmq_text_sensors:
    - meter_id: "12345678"
      field: software_version
      sensor:
        name: "Supercom 587 versiune software"
    - meter_id: "12345678"
      field: status
      sensor:
        name: "Supercom 587 stare"
```

- Testele verifică toate cele două exemple și toate câmpurile JSON
  măsurabile, plus exemple negative de corupere sau modificare XMQ.
  Decodoarele celorlate drivere și firmware-ul instalat nu sunt modificate.
  O compilare CI verde nu este echivalentă cu o probă RF reală.

## Vario451: XMQ hex-IXML compilat în firmware (2026-11-10)

- `generate_vario451_ixml.py` parcurge gramatica IXML originală
  `vario451.xmq` (`hex` reprezintă câte un nibble), derivă pozițiile
  `DV_PREV` și `DV_CURR`, verifică mapările `027E/027F` și
  formulele XMQ pentru conversia din mGJ în kWh.
- Generează `xmq_generated_vario451.h` pentru decodorul C++ nativ
  `vario451_plain.h`. Limitele de securitate, modelul admis
  `TCH/27/C3`, antetul CI=A2, ID-ul configurat, verificarea CRC
  externă și layoutul radio rămân stricte. Modelul `TCH/27/04`
  nu devine publicabil fără fixture radio verificat.
- Fixture-ul original verifică toate cele trei valori calculate;
  build-only CI include și teste negative. Nu se fac flashuri.

# Native XMQ wM-Bus on official ESPHome SX126x

**Current branch:** `experimental/xmq-multidriver-auto` (native-only,
November 2026). This branch no longer packages the legacy
`wmbus_common`, `wmbus_meter`, `wmbus_radio` or
`socket_transmitter` components.

## Active sources

- `components/sx126x/`: patched official ESPHome SX126x T1/C1 streaming.
- `components/wmbus_native_sx126x/`: bounded native frame receiver,
  CRC/integrity gating, configured meter binding, strict XMQ AUTO and
  explicit decoding, application security and Home Assistant sensor output.
- `experimental/xmq/drivers/`: 52 upstream-anchored XMQ definitions.
- `experimental/xmq/tools/native_driver_specs.py`: explicit native
  decoder manifest. `generate_ixml_cpp.py` and
  `build_native_generated.py` produce the compile-time descriptors.
- `experimental/xmq/tools/generate_clear_profiles.py`: fixture-checked
  compile-time model and DIF/VIF record-key tables for munia, cma12w,
  lansendw and lansenpu. Their native security, numeric and status
  validation is retained; this is a bounded compiler, not full XMQ.
- `experimental/xmq/portable_cpp/`: portable XMQ records/math/date
  implementation and **offline** comparison catalog/parity helpers.
- `experimental/xmq/legacy_reference/`: 15 isolated, header-only
  historical `wmbus_pure` reference implementations for host tests.
  **Not part of the deployed SX126x native firmware.**
- `experimental/semtech-radio/`: native RF/host fixture tests,
  build-only Heltec YAML and CI guards.
- `experimental/xmq/espidf_cpp_probe/`: native ESP-IDF link/portability
  validation. It does not register the old wM-Bus radio component.

## Build/test contract

Use `.github/workflows/native-sx126x-adapter.yml` as the authoritative
continuous-integration workflow. The Heltec lab YAMLs import only
`sx126x` and `wmbus_native_sx126x`, never the old radio. CI also
validates fixture parity, test-only portable legacy references, the
security catalog and ESP-IDF/ESPHome builds.

Native receiver publication requires a configured meter identity,
validated packet/frame integrity and exact supported driver/security
profile. `driver: auto` is constrained by the same identity and
security rules as explicit drivers. `NOKEY` in an upstream fixture does
**not** prove an RF telegram is unencrypted.

These checks do not establish live RF parity, so do not flash experimental
lab firmware without separate radio/board verification. Real meter IDs,
cryptographic keys and HA API credentials must remain in private YAML
secrets rather than committed public configuration.

## Historical notes

Older XMQ shadow, `handleTelegram` and custom transceiver documentation
refer to retired branches, not to this native-only branch. Git history and
`docs/history/` preserve migration context. See top-level `README.md`
and `PROJECT_CONTEXT.md` for the driver batch chronology.
