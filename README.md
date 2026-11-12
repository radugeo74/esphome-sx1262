# ESPHome wM-Bus — două radiouri: SX126x + SX127x

**Ramură activă:** `experimental/dual-radio-sx126x-sx127x`, derivată din `experimental/xmq-runtime-engine` (12 noiembrie 2026).

Acest branch are două variante hardware selectabile prin `wmbus_radio`. YAML include **exact una** dintre proprietățile `sx126x_id` sau `sx127x_id`; decodoarele `wmbus_common` și senzorii `wmbus_meter` rămân comune.

- **SX126x:** `components/sx126x` — receptor T1/C1 existent, verificat pe Heltec; nu se modifică pentru experimentele SX127x.
- **SX127x:** `components/sx127x` — extensie a componentei ESPHome 2026.10.1 pentru **T1 + C1/A + C1/B** în FSK, cu citire DIO1/FifoEmpty din FIFO în task dedicat. În modul combinat SX127x sincronizează pe `54`, apoi clasifică `3D` (T1 sau C1/B) și `CD` (C1/A). Candidaturile ambigue T1/C1-B sunt verificate separat prin CRC înainte de publicare. **Recepție T1 demonstrată pe LILYGO T3-S3, dar sensibilitate și C1/A încă nevalidate ca stabilitate.**
- **Partajat:** `components/wmbus_radio` validează simbolurile 3-of-6, lungimea cadrului, CRC și ID DLL/TPL înainte de a publica valori; telegramele străine păstrează mesajele de diagnostic și linkurile de analiză.

**Teste:** `tests/sx126x-wmbus-bridge.yaml`, `tests/sx127x-wmbus-lab.yaml`, `tests/sx127x-wmbus-c1-lab.yaml`, `tests/lilygo-t3s3-sx1276-ci.yaml`, `tests/test_sx127x_t1_length.cpp`, `tests/test_c1_crc_safety.py`. Workflow: `.github/workflows/native-espidf-sx126x-smoke.yml`. GPIO-urile testului SX127x sunt doar exemple și nu autorizează flash.

**Reglaj AFC experimental (12 noiembrie 2026):** receptorul SX127x T1/C1 setează acum explicit `RegAfcBw(0x13)=0x10` (~333 kHz) și afișează la boot valorile reale pentru registrele RxBw, AfcBw, RxConfig, AfcFei, PreambleDetect, SyncConfig și RssiThresh. Pentru comparația A/B cu Heltec păstrează `bandwidth: 250_0kHz`, `rx_floor: -94`, `preamble_detect: 2`, `preamble_errors: 10`. `bandwidth` și `rx_floor` sunt deja opțiuni YAML SX127x; **nu** schimbăm acum 200 kHz, prioritatea taskului, FIFO sau backendul SX126x. Vezi `tests/lilygo-t3s3-sx1276-ci.yaml` și `tests/README.md`. Efectul AFC necesită măsurat pe RF real.

Pentru RX SX127x, folosește `external_components` incluzând override-ul local `sx127x`; selectează `wmbus_radio.sx127x_id` și configurează `dio1_pin`, `wmbus_t1_mode: true`, `packet_mode: true`, `sync_value: [0x54, 0x3D]`, `preamble_polarity: 0x55`, `bitrate: 100000`, `payload_length: 0`. Pentru **T1_C1**, schimbă `sync_value` în `[0x54]` și `wmbus_radio.native_stream_mode: T1_C1`; vezi `tests/sx127x-wmbus-c1-lab.yaml`. Diagnostic SX127x opțional: `wmbus_radio.native_phy_diagnostics: true`, sumar agregat la 60s, fără ID-uri sau payloaduri. Pentru LILYGO T3-S3 SX1276 am adăugat fixture ESP32-S3 ESP-IDF `tests/lilygo-t3s3-sx1276-ci.yaml`, numai pentru compilare. Pentru detalii vezi [tests/README.md](tests/README.md).

**Important:** versiunile mai vechi din această documentație descriu istoric ramura SX126x/XMQ, nu starea completă a ramurii dual-radio de acum.

---

## Context istoric moștenit din ramura XMQ

# ESPHome wM-Bus — XMQ runtime pe baza native-espidf

> **Native-only radio (11 November 2026):** `wmbus_radio` requires `sx126x_id` and uses patched SX126x exclusively. Legacy `transceiver_sx1262.*`/`transceiver.*` and the legacy YAML radio schema were removed from this branch. `wmbus_common`/`wmbus_meter` still decode and publish meter readings. The native T1/A bridge was previously hardware-validated with 12/12 configured meters; **this source cleanup itself still requires a fresh CI build and hardware regression test**. Dynamic XMQ reload is not implemented.

**Branch:** `experimental/xmq-runtime-engine`  
**Bază:** [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf) la commit `c4284739be7f9a5ee251de268ae3904068906ca4`.  
**Stare (11 noiembrie 2026):** puntea `sx126x → wmbus_radio → wmbus_meter` primește și publică T1/A pe Heltec pentru **12/12 contoare configurate (7 căldură, 5 apă)**. `wmbus_common` decodifică în continuare datele de aplicație; vechiul receptor radio și schema lui YAML au fost eliminate din această ramură; rollback se face dintr-un commit anterior (sau din firmware-ul salvat). **C1-B nu este validat pe hardware; motorul dinamic XMQ NU este implementat.**

## Obiectiv

Păstrăm recepția și decodarea existente ca referință și construim **un singur motor comun care interpretează reguli XMQ**. O modificare compatibilă în fișierul `.xmq` original trebuie să se aplice automat (detectare, validare, pachet de date, transfer, reload) **fără recompilarea/flash-uirea firmware-ului**. Nu dezvoltăm câte un `driver_*.cc` sau `*_plain.h` pentru fiecare nou contor.

Nu înseamnă executarea fără limite a limbajului XMQ pe ESP32. Preferăm compilarea XMQ/IXML în **date** pe Home Assistant și un interpret C++ cu operatori tipizați pe ESP32-S3. Dacă un nou XMQ folosește un operator nesuportat, rămâne activ ultimul pachet valid; extinderea motorului comun poate necesita OTA.

## Ce avem efectiv în această ramură

### Motorul wmbusmeters existent din native-espidf

- `components/wmbus_common`: decodoarele `driver_*.cc`, DIF/VIF, formule, chei și funcții de interpretare, **cod legacy păstrat pentru paritate/rollback**.
- `components/wmbus_meter`: contoare configurate și senzori numeric/text pentru Home Assistant.
- `components/wmbus_radio`: punte exclusiv SX126x nativă T1/A (C1-B experimental) către `wmbus_meter`; fără vechiul `transceiver_sx1262.cpp`.
- `components/socket_transmitter`: opțional, precum în ramura originală.
- `NATIVE_ESPIDF_REPORT.md`: raportul buildurilor native ESP-IDF din 21 octombrie 2026, înaintea noii ramuri. Nu înseamnă test hardware al noului cod.

Configurația ESPHome legacy este documentată în [README-ul original](https://github.com/radugeo74/esphome-sx1262/blob/native-espidf/README.md). **Nu înlocui configurația activă până la validarea unei punți între cele două implementări radio.**

### Componenta nouă pentru SX126x nativ ESP-IDF

Am importat `components/sx126x` **integral** din commitul [`bd3035ed`](https://github.com/radugeo74/esphome-sx1262/commit/bd3035edf2a0ee1f87ad827c344b47975061923a) al ramurii experimentale. Include:

- `wait_busy_()` cu timeout / propagarea erorii și verificarea BUSY **înainte de tranzacții SPI**;
- oprirea configurării la `is_failed()`, bufferul versiunii inițializat și patch-urile GFSK/RSSI / recepție T1 pe care le conține acel snapshot;
- `packet_transport` și fișierele asociate componentului, cu dependențele ESPHome SPI.

**Important (actualizare):** `sx126x` este acum conectat la `wmbus_radio/wmbus_meter` prin `sx126x_id`; în acest mod nu se instanțiază receptorul SPI legacy. Nu configura două drivere SPI pe **același SX1262**. Exemplul [`tests/sx126x-native-espidf.yaml`](tests/sx126x-native-espidf.yaml) este izolat, doar pentru validarea compilării; nu este configurație de instalat.

## Ce construim în continuare

```text
wmbusmeters *.xmq (GitHub sau director HA)
     │ schimbare detectată (hash/proveniență)
     ▼
parser/compiler XMQ/IXML pe Home Assistant
     │ date tipizate, fixture-uri și validări
     ▼
pachet compact de reguli XMQ (nu firmware/C++)
     │ transfer autentificat, staging A/B
     ▼
Heltec: un singur interpret C++ + receptor SX126x nativ
     │ driver de reguli schimbat fără OTA
     ▼
entități și senzori Home Assistant
```

**Adaptorul SX126x → wM-Bus este implementat și verificat pentru T1/A pe hardware.** Recepția nativă ajunge în parserul și decodoarele aplicației existente. Pasul următor este proiectarea/implementarea interpretului comun XMQ cu reguli actualizabile fără OTA, nu pretinderea că există deja sau eliminarea prematură a `wmbus_common`.

## Documente

- [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) — stare și cerințe pentru dezvoltare.
- [`AGENTS.md`](AGENTS.md) — instrucțiuni Codex.
- [`docs/SX126X_NATIVE_BASELINE.md`](docs/SX126X_NATIVE_BASELINE.md) — originea și integrarea corectă a radio.
- [`docs/XMQ_RUNTIME_ARCHITECTURE.md`](docs/XMQ_RUNTIME_ARCHITECTURE.md) — designul nou.
- [`docs/XMQ_RUNTIME_SOURCE_UPDATE.md`](docs/XMQ_RUNTIME_SOURCE_UPDATE.md) — actualizare automată și rollback.
- [`docs/XMQ_RUNTIME_ROADMAP.md`](docs/XMQ_RUNTIME_ROADMAP.md) — milestones.
- [`docs/XMQ_RUNTIME_ACCEPTANCE.md`](docs/XMQ_RUNTIME_ACCEPTANCE.md) — condiții de validare.
- [`docs/LEGACY_NATIVE_SNAPSHOT.md`](docs/LEGACY_NATIVE_SNAPSHOT.md) — referința/rollback.
- [`experimental/xmq/runtime/README.md`](experimental/xmq/runtime/README.md) — spațiul viitorului compilator.

## Restricții

Fără chei AES, ID-uri private sau configurări personale în Git. Nu se ignoră CRC, TPL, criptarea sau filtrele de ID. Nu se acceptă operatori XMQ necunoscuți în mod implicit și nu se publică valori nedovedite. **Nu se face flash automat pe Heltec**. Separăm explicit „importat”, „testat software”, „validat hardware” și „implementat dinamic”.


## Combined T1/C1 receiver status (12 November 2026)

The working default remains `native_stream_mode: T1`. The opt-in `native_stream_mode: T1_C1` adds experimental C1 Format A (`54 CD`) and C1 Format B (`54 3D`) parsing, exact physical length checks, complete DLL CRC checking and a corrected bounded second CRC in the existing `wmbus_common` Format-B routine. The previous experimental name `T1_C1_B` is still accepted as an alias for the same combined mode. PHY length remains limited to 255 bytes including the C1 mode prefix.

**Status:** implemented in code, pending CI and real C1 RF validation. The known-good Heltec T1/A configuration does not need to change. Do not mistake CRC unit tests or an ESPHome compile for confirmation of simultaneous T1/C1 reception.


### Native T1/C1 PHY diagnostics (12 November 2026)

The native-only `wmbus_radio` now exposes an opt-in `native_phy_diagnostics: true` (default false). Combine it with `native_stream_mode: T1_C1` **only for a controlled RF test**. It logs classified C1-A/C1-B PHY prefixes and late FIFO drops in the SX126x layer, plus C1 CRC/length rejections and validated A/B counts in the radio adapter. Counters do not log meter IDs, AES keys or frame payloads. A T1 encoded length exceeding the 255-byte FIFO cap is rejected, protecting against FIFO wrap ambiguity. This was added after the real Heltec log (76), which confirmed many T1/A frames but no verified C1 frames.

**Known limitation:** the combined mode still uses the hardware sync configuration. Seeing `Sync Value: 0x543d` alone does not prove simultaneous T1, C1-A and C1-B capture. Until live C1 reception is verified, do not claim universal dual-mode reception; keep the known-good Heltec firmware and its T1 configuration.
