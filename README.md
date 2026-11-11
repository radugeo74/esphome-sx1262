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
