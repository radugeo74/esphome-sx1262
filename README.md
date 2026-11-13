## SX1276: recepție continuă 54 cu detector de preambul (13 noiembrie 2026)

**Branch:** `experimental/sx127x-continuous-preamble`. Rezultat al testului precedent: `54` + `preamble_detect: 0` + `rx_floor: -110` a produs aproximativ 97.000 sincronizări, dintre care peste 96.000 marcaje nevalide, iar C1/A aproape a dispărut. Această încercare păstrează recepția comună continuă `54`, dar reface declanșarea AFC/AGC prin detectorul de preambul și pragul RSSI al configurației anterioare. **Nu afirmăm încă recepție performantă T1+C1/A: necesită test hardware simultan cu Heltec.**

În YAML LILYGO, `external_components` trebuie să indice branch-ul de mai sus. În secțiunea `sx127x:` păstrează restul pinilor și al parametrilor FSK existenți, schimbând doar:

```yaml
wmbus_t1_mode: true
wmbus_dual_sync: false
wmbus_continuous_dual: true
wmbus_c1a_only: false
sync_value: [0x54]
preamble_detect: 2
rx_floor: -94
```

Păstrează `wmbus_radio.native_stream_mode: T1_C1`, driverele XMQ și cele 12 contoare; nu adăuga senzori. Opțiunea `wmbus_continuous_dual` rămâne opt-in și este acum validată pentru `preamble_detect >= 1`, nu pentru valoarea 0. Cu `preamble_detect: 2`, receptorul folosește triggerul de preambul existent, nu triggerul pe RSSI. Recuperarea FIFO și verificările CRC sunt neschimbate. Fixture-ul `tests/lilygo-t3s3-sx1276-ci.yaml` este doar pentru compilare, nu instalare.

**Test recomandat:** loguri simultane Heltec/LILYGO 10–15 minute și comparație a cadrelor CRC-valide T1/A, C1/A, a marcajelor `3D/CD/other`, a `rearm` și a contoarelor configurate procesate. **Rollback:** revino la `experimental/sx127x-hybrid-dual-sync` cu `wmbus_dual_sync: true`, `wmbus_continuous_dual: false`, `preamble_detect: 2`, `rx_floor: -94`.

---

## SX1276: experiment recepție continuă T1 + C1/A (13 noiembrie 2026)

Branch izolat: `experimental/sx127x-continuous-t1-c1`, derivat din experimentul de alternare hardware. **Nevalidat RF și fără CI confirmat**. Nu modifica firmware-ul Heltec.

Scop: eliminarea ferestrelor de 280/120 ms și păstrarea permanentă a unei singure sincronizări hardware `54` (1 octet). Primul octet FIFO după sync trebuie să fie `3D` (T1 sau C1/B) sau `CD` (C1/A). Nu se modifică interpretarea datelor, cheile AES, filtrele configurate ori verificarea CRC. Noua opțiune `sx127x.wmbus_continuous_dual` este implicit `false`; când este `true`, validează că `wmbus_dual_sync: false`, `wmbus_c1a_only: false`, `sync_value: [0x54]` și `preamble_detect: 0`. Acesta este un test al achiziției radio cu AFC/AGC declanșate pe RSSI, fără detectorul de preambul ca poartă. Detecția de sync rămâne activă.

În **YAML-ul real LILYGO**, schimbă URL-ul `external_components` pe branch-ul de mai sus și setează în secțiunea `sx127x:`:

```yaml
wmbus_t1_mode: true
wmbus_dual_sync: false
wmbus_continuous_dual: true
wmbus_c1a_only: false
sync_value: [0x54]
preamble_detect: 0
rx_floor: -110
```

Păstrează `native_stream_mode: T1_C1`, `native_phy_diagnostics: true`, ceilalți parametri FSK și cele 12 contoare. Nu crea senzori noi. `tests/lilygo-t3s3-sx1276-ci.yaml` este un fixture sintetic **build-only**, nu un YAML de instalat pe placa reală.

**Criteriul de succes:** captură simultană cu Heltec de minimum 10–15 minute; verifică T1 valide, C1/A valide, `marker_3D`/`marker_CD`, `bad_crc`, `overrun`, `frame_timeout` și câte contoare configurate sunt procesate. Acest experiment poate crește recepția C1/A, dar poate produce și mai multe false candidate sau pierde cadre slabe din cauza pragului RSSI. Nu afirma recepție combinată rezolvată înainte de rezultate RF.

**Rollback sigur:** pe același branch setează `wmbus_continuous_dual: false`, `wmbus_dual_sync: true`, `preamble_detect: 2`, `rx_floor: -94`, păstrând `sync_value: [0x54]` pentru revenirea la ferestrele hibride; sau revino la branch-ul anterior.

---

## Experiment SX1276: sincronizare hardware hibridă T1/C1/A (13 noiembrie 2026)

**Branch izolat:** `experimental/sx127x-hybrid-dual-sync` (derivat din `experimental/dual-radio-sx126x-sx127x`). **Cod încă nevalidat prin CI sau RF real**; nu se afirmă că recepția combinată este rezolvată.

În logurile simultane LILYGO cu `sync_value: [0x54]` a decodat multe T1, dar doar foarte puține C1/A; profilul anterior `[0x54, 0xCD]` primea C1/A, însă excludea T1. Am introdus `sx127x.wmbus_dual_sync: true` (implicit `false`) pentru un **experiment de recepție prin ferestre alternante**, nu demodulare simultană: 280 ms cu sincronizare completă `54 3D` (T1 sau C1/B), apoi 120 ms cu `54 CD` (C1/A). Schimbarea se face în taskul FIFO numai când FIFO este gol și receptorul nu indică sync în desfășurare; se trece prin STANDBY și se re-armează RX. În ambele ferestre, hardware-ul consumă marcajul din FIFO și software-ul reconstruiește `54 CD` numai pentru validarea C1/A; verificările CRC și dispatch-ul contoarelor rămân nemodificate. Prin natura acestei soluții sunt posibile pierderi T1 și C1 în fereastra opusă.

**În YAML-ul LILYGO real**, păstrează `wmbus_t1_mode: true`, `wmbus_c1a_only: false`, `sync_value: [0x54]`, `bandwidth: 200_0kHz`, `wmbus_radio.native_stream_mode: T1_C1`, `wmbus_radio.native_phy_diagnostics: true`, și adaugă **doar** `wmbus_dual_sync: true` sub `sx127x:`. La `external_components`, folosește acest branch pentru componentele `sx127x, wmbus_common, wmbus_radio, wmbus_meter`. Fără schimbări de radio Heltec, XMQ, chei sau senzori.

**Criteriu de test:** 10–15 minute loguri simultane; urmărește `Native SX127x verified T1/A`, `Native SX127x verified C1/A`, `Telegram handled by 1 handlers`, `SX127x PHY verified T1=... C1A=...` și diferența față de Heltec. Configurația de CI cu ID sintetic este `tests/lilygo-t3s3-sx1276-ci.yaml`, **nu se instalează** pe placa reală. Dacă C1/A nu crește sau T1 pierde prea multe contoare, revenire: scoate `wmbus_dual_sync: true` (modul combinat precedent), ori revino la `wmbus_c1a_only: true` și `sync_value: [0x54, 0xCD]` pentru C1/A-only.

---

# ESPHome wM-Bus — două radiouri: SX126x + SX127x

**Ramură activă:** `experimental/dual-radio-sx126x-sx127x`, derivată din `experimental/xmq-runtime-engine` (12 noiembrie 2026).

Acest branch are două variante hardware selectabile prin `wmbus_radio`. YAML include **exact una** dintre proprietățile `sx126x_id` sau `sx127x_id`; decodoarele `wmbus_common` și senzorii `wmbus_meter` rămân comune.

- **SX126x:** `components/sx126x` — receptor T1/C1 existent, verificat pe Heltec; nu se modifică pentru experimentele SX127x.
- **SX127x:** `components/sx127x` — extensie a componentei ESPHome 2026.10.1 pentru **T1 + C1/A + C1/B** în FSK, cu citire DIO1/FifoEmpty din FIFO în task dedicat. În modul combinat SX127x sincronizează pe `54`, apoi clasifică `3D` (T1 sau C1/B) și `CD` (C1/A). Candidaturile ambigue T1/C1-B sunt verificate separat prin CRC înainte de publicare. **Recepție T1 demonstrată pe LILYGO T3-S3, dar sensibilitate și C1/A încă nevalidate ca stabilitate.**
- **Partajat:** `components/wmbus_radio` validează simbolurile 3-of-6, lungimea cadrului, CRC și ID DLL/TPL înainte de a publica valori; telegramele străine păstrează mesajele de diagnostic și linkurile de analiză.

**Teste:** `tests/sx126x-wmbus-bridge.yaml`, `tests/sx127x-wmbus-lab.yaml`, `tests/sx127x-wmbus-c1-lab.yaml`, `tests/lilygo-t3s3-sx1276-ci.yaml`, `tests/test_sx127x_t1_length.cpp`, `tests/test_c1_crc_safety.py`. Workflow: `.github/workflows/native-espidf-sx126x-smoke.yml`. GPIO-urile testului SX127x sunt doar exemple și nu autorizează flash.

**Test controlat RX bandwidth 200 kHz, C1/A-only, LNA boost OFF (13 noiembrie 2026):** după comparația simultană 250 kHz cu boost OFF/ON (aprox. 42–44% cadre comune cu Heltec; fără beneficiu convingător de la boost), următorul test schimbă **exclusiv** `sx127x.bandwidth: 200_0kHz` în YAML-ul LILYGO existent și setează `wmbus_lna_boost_hf: false` (sau elimină opțiunea). Se păstrează `wmbus_c1a_only: true`, `sync_value: [0x54, 0xCD]`, `wmbus_radio.native_stream_mode: T1_C1`, preambul, AFC, AGC, frecvența, bitrate și antena. În logul după boot trebuie să apară `RegRxBw[12]=0x09`, `RegAfcBw[13]=0x10`, `RegSyncConfig[27]=0xB1`, `LNA HF boost: FALSE`, `FSK profile matches expected: TRUE`. Fixture-ul `tests/lilygo-t3s3-sx1276-c1a-rxbw200-ci.yaml` este **doar pentru CI** (ID sintetic, fără senzori), nu pentru flash pe dispozitivul real. Evaluăm 5–10 minute simultan cu Heltec și comparăm cadre C1/A CRC-valide, identice și CRC respinse, normalizate pe timp; nu afirmăm un câștig RF pe baza compilării. Revenire: `bandwidth: 250_0kHz`. Nu sunt necesare modificări de cod, radioul SX126x și profilul dual T1+C1 nu sunt schimbate.

**Experiment opt-in LNA HF boost pe SX1276 (13 noiembrie 2026):** am adăugat sub `sx127x:` opțiunea `wmbus_lna_boost_hf: true` (implicit `false`). Numai în FSK nativ wM-Bus, imediat înainte de RX, software-ul citește `RegLna[0C]` în STANDBY, iar dacă opțiunea este activată modifică **doar biții [1:0]** la `11` (boost HF), prin read-modify-write și readback; dacă citirea nu confirmă activarea, RX nu pornește și apare eroare. Cu opțiunea absentă, registrul nu este scris. `dump_config()` afișează `STANDBY pre-RX RegLna` înainte/după și statusul; toate citirile au loc înainte de taskul FIFO. Nu schimbă AFC, AGC, bandwidth, preambul, sync, FIFO, decodor, SX126x sau YAML-ul actual. Comparația RF C1/A trebuie făcută pe același interval cu Heltec și cu același loc/antenă. Sunt incluse două fixture-uri build-only: `tests/lilygo-t3s3-sx1276-c1a-only-ci.yaml` (OFF) și `tests/lilygo-t3s3-sx1276-c1a-lna-boost-ci.yaml` (ON); nu flash-ui fișierul de CI cu ID sintetic. Revenire: elimină linia `wmbus_lna_boost_hf: true` din YAML real.

**Verificare AGC/LNA/RSSI fără acces SPI în timpul recepției (13 noiembrie 2026):** snapshotul FSK SX1276 include acum `RegLna[0C]` și `RegRssiConfig[0E]`, citite o singură dată la boot și afișate la reconectarea ESPHome API. Mesajul decodifică AGC/AFC auto, HF boost și RSSI smoothing. Câștigul LNA din snapshot nu este cel din timpul recepției unui cadru (AGC îl poate ajusta). FEI nu este citit după payload deoarece datasheet-ul cere măsurare în timpul preambulului. Fără schimbări RF, YAML, FIFO, SX126x, T1+C1 sau senzori.

**Test separat SX1276 C1/A-only, sincronizare hardware completă `54 CD` (13 noiembrie 2026):** doar pe LILYGO experimental, adaugă sub `sx127x:` `wmbus_c1a_only: true` și `sync_value: [0x54, 0xCD]`. Păstrează `wmbus_radio.native_stream_mode: T1_C1` pentru decodorul CRC, deși RF ascultă **numai C1/A** (T1 nu se recepționează în acest test). Primul octet FIFO este direct `L`, iar receptorul reconstruiește înainte de CRC octeții `54 CD` consumați de hardware. În log urmărește `hw_sync54CD_fifo` (nu `marker_CD`, care rămâne zero în acest profil), `header_ok`, `queued`, `crc_verified`. Snapshot-ul registrelor trebuie să arate `RegSyncConfig=0xB1`. Test/compilare: `tests/lilygo-t3s3-sx1276-c1a-only-ci.yaml`, cu ID sintetic și fără entități senzor. **Profilul actual T1+C1 și SX126x rămân neschimbate.** Revenire: șterge `wmbus_c1a_only`, reintrodu `sync_value: [0x54]`.

**Diagnostic C1/A SX1276 înainte de CRC (12 noiembrie 2026):** în modul `T1_C1`, cu `native_phy_diagnostics: true`, noul sumar `SX127x SYNC54 probe` înregistrează primii octeți FIFO `3D` (T1/C1-B), `CD` (potențial C1/A), alții și citiri nereușite, precum și indicatoare IRQ preambul/sync eșantionate la FIFO deja activ. `SX127x C1A stages` separă marcajul, antetul, corpul, coada decodorului, abandonul și verificarea CRC. **Nu contorizează transmisiile RF care nu au ajuns în FIFO.** Se folosesc exclusiv citirile SPI deja existente, fără schimbări de comportament radio sau SX126x. Detalii în `tests/README.md`.

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
