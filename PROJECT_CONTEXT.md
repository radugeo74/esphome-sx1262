# Context curent — dual radio SX126x + SX127x

## AGC/LNA readback FSK SX1276 (13 noiembrie 2026)

Audit confirmă AFC și AGC active în `RegRxConfig=0x1E`, `RegAfcBw=0x10` (333.3 kHz), `RegRxBw=0x01` (250 kHz), `RegAfcFei=0x01`. `RegLna` nu era setat explicit. În această etapă se adaugă exclusiv citiri boot-only `RegLna[0C]` și `RegRssiConfig[0E]` în structura FSK snapshot, afișate și decodate în `dump_config()` prin Wi-Fi API. Cu AGC activ, câștigul LNA se schimbă în RX; snapshotul nu este gain live. FEI trebuie măsurat în preambul, nu la sfârșitul cadrului. Nu modificăm parametrii RF, ISR, FIFO, decodor, SX126x sau YAML. Test de contract și host RF extinse. Pas următor: log real pentru HF boost și smoothing.


## Test C1/A-only pe SX1276 cu hardware sync complet 54 CD (13 noiembrie 2026)

Logurile simultane Heltec/LILYGO confirmă `marker_CD=0`, iar Heltec recepționează repetat C1/A; ca experiment izolat se activează opt-in `sx127x.wmbus_c1a_only: true` + `sync_value: [0x54, 0xCD]`, păstrând `wmbus_radio.native_stream_mode: T1_C1` pentru validatorul CRC comun. Hardware-ul SX1276 consumă ambii octeți 54 CD, deci în FIFO primul octet este `L`; codul nu mai încearcă să citească un fals marcaj, iar fiecare candidat C1/A este reconstruit cu prefix `54 CD` înainte de CRC. Diagnostic nou `hw_sync54CD_fifo` distinct de `marker_CD` (care rămâne 0 în acest profil). Așteptare RegSyncConfig `0xB1` în `dump_config()`; T1 este deliberat indisponibil. Compatibilitatea strictă a configului este verificată (fără flag + full sync -> reject, flag + sync incomplet -> reject). Test host de lungime/reconstituire, contract, YAML/compilare pe LILYGO. Nicio modificare la path SX126x, modul normal T1_C1, FIFO/watchdog, prioritatea taskului, parametrii AFC sau decoderul `wmbus_radio`. După test revenire la sync `[0x54]` și eliminare flag.

## Diagnostic C1/A înainte de verificarea CRC (12 noiembrie 2026)

Pentru problema de pe LILYGO T3-S3 (T1 mai puține decât Heltec și aproape niciun C1/A), pe SX1276 am separat marcajul FIFO imediat după sincronizarea hardware 0x54: 0x3D (T1/C1-B), 0xCD (C1/A posibil), altele și citire eșuată. Folosim bitii `PreambleDetect=0x02` și `SyncAddressMatch=0x01` din `RegIrqFlags1` deja citit în task, numai când FIFO are date. După 0xCD, se numără: L-field/antet plauzibil, antet invalid, corp complet, punere în coada decodorului, queue drop, abort citire și C1/A CRC-valid (contor existent). Două loguri cumulative la 60 secunde: `SX127x SYNC54 probe` și `SX127x C1A stages`. Modificările sunt exclusiv diagnostic SX127x și sunt activate de `wmbus_radio.native_phy_diagnostics: true`; nu adaugă SPI reads, IRQ logging, schimbare RF, priorități, FIFO, senzori sau modificări SX126x. IRQ bits eșantionați la FIFO nu pot detecta preambul/sync RF ratat. Teste noi `tests/test_sx127x_c1_probe.cpp`, `tests/test_sx127x_c1_probe_contract.py`.

## Snapshot FSK la reconectare API ESPHome (12 noiembrie 2026)

Rezultatele testului LILYGO după AFC 333 kHz au venit prin loguri ESPHome API pornite după boot, deci lipseau mesajele de citire directă hardware. `configure_fsk_ook_()` stochează acum cele 7 valori ale registrelor SX1276 și rezultatul verificării `matches_expected`, numai în etapa de inițializare. `dump_config()` le afișează la conectarea ulterioară a clientului log ESPHome, fără SPI (critică pentru FIFO streaming 100 kb/s). Test contract `tests/test_sx127x_snapshot_contract.py` și build CI ambele backenduri. Nicio schimbare în SX126x sau funcția radio în timp real. Snapshotul este de la boot, nu citire în timp real.


## AFC + registru read-back SX1276 (12 noiembrie 2026)

Comparație simultană Heltec SX1262 vs LILYGO T3-S3 SX1276: în fereastra comună de ~138 s, Heltec a validat 66 T1 și 28 C1/A, LILYGO doar 21 T1 și 0 C1/A. Cauza nu este demonstrată. Am comparat `SzczepanLeon/esphome-components` și am preluat exclusiv reglajul AFC util: în configurația SX127x **native wM-Bus FSK** se scrie `RegAfcBw 0x13=0x10` (333,3 kHz), în loc de reset `0x0B` (50 kHz). `RegAfcFei=0x01` (AfcAutoClearOn, bit 0) rămâne corect; nu copiem eroarea din upstream `0x10` (AgcStart). La configurare se citesc din hardware `RegRxBw`, `RegAfcBw`, `RegRxConfig`, `RegAfcFei`, `RegPreambleDetect`, `RegSyncConfig`, `RegRssiThresh`, logate înainte de pornirea taskului FIFO; divergențele sunt avertizate.

În fixture-ul ESP-IDF LILYGO: `bandwidth: 250_0kHz`, `rx_floor: -94` (opțiuni YAML deja existente), `preamble_detect: 2`, `preamble_errors: 10`. Nu modificăm Rx bandwidth la 200 kHz, task/core/FIFO sau componentele SX126x. Următorul pas este un nou test simultan pentru cuantificarea efectului AFC, mai ales T1 slab și C1/A. Testele host validează codificarea registrelor, CI verifică compilarea, dar **efectul RF nu este încă confirmat**.


## Etapa SX127x 3/4/5 — diagnostic PHY, regresii reale și LILYGO T3-S3 (software, 12 noiembrie 2026)

Pentru SX127x s-au adăugat contoare atomice pe taskul FIFO și pe callbackul decodorului: starturi RX, candidați și telegrame valide T1/C1-A/C1-B, invalidări 3-of-6/CRC/length, FIFO overrun, timeout frame/modem, IRQ ratat, marker/prefix invalid, pierdere în coadă și RX rearm. `native_phy_diagnostics: true` afișează agregatele o dată pe minut, fără chei, ID-uri de contor sau payloaduri. Testul `tests/test_c1_crc_safety.py` folosește decodorul 3-of-6 și rutinele CRC reale din proiect, cu cadre T1/C1 complete, corupte, trunchiate și consecutive, inclusiv limita C1/B de 128 octeți. `tests/lilygo-t3s3-sx1276-ci.yaml` validează/compilează ESP32-S3 cu placa `lilygo-t3-s3` și pinii SX1276 (SPI 5/6/3, CS7, RST8, DIO0=9, DIO1=33). Toate sunt build/test-only. RF real pe placă rămâne nevalidat; SX126x nu a fost modificat.

## SX127x FIFO + auto-recovery (software, 12 noiembrie 2026)

Pe backendul SX127x am înlocuit citirile individuale per octet cu SPI în rafale de 16 octeți atunci când bitul FSK `FifoLevel` confirmă pragul de 15. Coada finală de sub 16 octeți se citește numai cât timp DIO1/FifoEmpty indică date disponibile; `FifoOverrun` respinge cadrul. Recepția completă T1/C1-A/C1-B are termen-limită de 120ms, watchdog la 1s pentru întreruperi ratate, `RX timeout`/modem ieșit din RX și rearmare după 60s de inactivitate. Recuperarea resetează FIFO-ul și starea RX fără publicarea cadrelor parțiale.

Acest calup privește **numai SX127x**; SX126x și decodoarele aplicației sunt nemodificate. Testele host verifică deciziile FIFO/watchdog; hardware RF T3-S3 este încă nevalidat.


**Ramură:** `experimental/dual-radio-sx126x-sx127x`. SX126x nativ funcționează pe Heltec (T1/C1) cu filtrare ID DLL/TPL; SX127x T1 și C1-A/B sunt implementate experimental în cod și încă nevalidate pe RF. `wmbus_radio` folosește `sx126x_id` **sau** `sx127x_id`, niciodată ambele. Ambele folosesc `wmbus_meter` și `wmbus_common` existente.

SX127x are componentă locală `components/sx127x` (derivată din ESPHome 2026.10.1), citire FIFO prin DIO1, lungime T1 determinată prin 3-of-6, task pentru captură, coadă pentru procesare în loop și verificarea finală a cadrului prin CRC comun. Fixture-urile `tests/sx127x-wmbus-lab.yaml` (T1) și `tests/sx127x-wmbus-c1-lab.yaml` (T1+C1/A+B) sunt **build-only**. În modul combinat sync hardware este numai `54`; al doilea octet (`3D` sau `CD`) rămâne în FIFO. T1 și C1/B cu `54 3D` sunt păstrate ca potențiali candidați până la verificarea strictă a CRC de către pipeline-ul comun. Nu instala pe hardware până când recepția T1 și C1 este validată.

Menține intactă implementarea SX126x și evită introducerea de date private/chei în fixture-uri. Testul C++ `tests/test_sx127x_t1_length.cpp` verifică T1 (84/89 octeți), C1/A, C1/B, prefixurile comune `54 3D`, simbolurile 3-of-6 invalide și limitele de memorie. CI compilează ambele backenduri.

---

## Context moștenit istoric — XMQ runtime

# PROJECT_CONTEXT — native-espidf + SX126x patch + XMQ runtime

**Ramură de lucru:** `experimental/xmq-runtime-engine`  
**Bază reală:** `native-espidf`, commit `c4284739be7f9a5ee251de268ae3904068906ca4`.  
**Patch radio importat din:** `experimental/xmq-multidriver-auto`, commit `bd3035edf2a0ee1f87ad827c344b47975061923a`, subarborele `components/sx126x`.

## Decizia utilizatorului

Am abandonat strategia de a rescrie câte un decodor C++ sau un generator per driver XMQ. Se cere motor comun, cu actualizare automată a fișierelor `.xmq` compatibile, fără recompilarea firmware-ului. **Punctul de plecare ales explicit este `native-espidf`, NU ramura `experimental/xmq-multidriver-auto`.** Din cea din urmă preluăm **numai patch-urile/componenta SX126x**, nu seria decodoarelor generate sau structura XMQ-only.

## Ce există acum

- `components/wmbus_common`: decodoarele vechi `driver_*.cc` + parserul wmbusmeters C++, pentru referință/paritate.
- `components/wmbus_meter`: entitățile și configurarea contoarelor din ESPHome.
- `components/wmbus_radio`: receptorul vechi `transceiver_sx1262.cpp` (T1/C1).
- `components/socket_transmitter`: facilități opționale moștenite.
- `components/sx126x`: componenta ESPHome SPI/GFSK SX126x importată separat, cu patch-urile `wait_busy_`, fail-fast, RX T1/packet transport.
- `NATIVE_ESPIDF_REPORT.md`: raportul buildurilor din ramura de bază; nu se interpretează ca dovadă a buildului noii ramuri.
- `tests/sx126x-native-espidf.yaml`: fixture de compilare izolat, fără autentificare sau contoare reale.

**Limitare:** `components/sx126x` nu este încă conectat la `wmbus_radio` sau `wmbus_meter`; două componente radio nu trebuie să revendice simultan același transceiver. Motorul XMQ dinamic, descărcarea/reloadul `.xmq` și sincronizatorul Home Assistant sunt în faza de proiectare, nu implementate.

## Hardware / constrângeri

Heltec WiFi LoRa 32 V3, ESP32-S3, 8 MB flash, SX1262, ESP-IDF. Componentele vechi și patch-urile noi trebuie validate la aceeași versiune ESPHome și același framework înainte de integrare. Configurația reală urmărește 2 repartitoare `fhkvdataiii` și 4 apometre `mkradio4`; cheile și ID-urile rămân private.

## Plan ordonat

1. **M0:** pornire corectă din `native-espidf` + importul complet, nemodificat al `components/sx126x`; documentație și fixture build-only.
2. **M1:** adaptor ESPHome cu `sx126x` ca unic proprietar SPI, care furnizează cadre T1/C1 validate către partea wM-Bus existentă; fără duplicate radio și fără schimbarea senzorilor HA.
3. **M2:** interpret C++ comun care primește regulile pentru `iperl.xmq` ca **date**, schimbate la runtime în același binar.
4. **M3:** stocare sigură A/B, rollback, sincronizare din Home Assistant cu încărcare automată a XMQ compatibile.
5. **M4:** IXML `mkradio4` și `fhkvdataiii`, apoi generalizarea către toate familiile validate; eliminarea decodoarelor vechi numai după paritate și hardware.

Citește `README.md`, `AGENTS.md`, `docs/SX126X_NATIVE_BASELINE.md` și `docs/XMQ_RUNTIME_*.md` înainte de modificări. Nu copia automat fișiere XMQ-only din alte ramuri și nu schimba driverul SX126x cu versiunea nepachetată. Nu face flash/OTA fără solicitarea expresă a utilizatorului.

## Criterii de acceptare

- Păstrare exactă a fișierelor `native-espidf` în afara documentelor rescrise și a componentului nou.
- Identitatea de conținut a `components/sx126x` cu sursa patch-uită `bd3035ed`.
- Validare YAML și C++ pentru ESP-IDF, apoi verificare recepție RF reală.
- Testul principal XMQ: **același firmware SHA + o versiune nouă de reguli XMQ → rezultat actualizat fără rebuild/flash**.
- Refuz pentru fișiere invalide, cadre neconfigurate, criptare necunoscută; rollback la ultima versiune bună.

**La crearea ramurii:** doar noul baseline/structură/doc/test fixture. Motorul XMQ nu există încă.

## Bridge SX126x → wmbus_meter (în implementare/test software)

- Noua cale `wmbus_radio: {sx126x_id: ..., native_stream_mode: T1}` folosește SX126x patch-uit ca unic proprietar SPI și callback-ul T1 3-of-6. Opțional `T1_C1_B` este experimental; C1-A nu este suportat pe această cale.
- Adapterul reunește fragmente limitate, verifică structura și toate CRC-urile DLL prin funcțiile wmbusmeters, apoi trimite `Frame` către aceiași handleri `wmbus_meter`, cu ID/cheie/link-mode neschimbate.
- Configurăria de test e în `tests/sx126x-wmbus-bridge.yaml`. Verificările software vor rula în CI.
- `transceiver_sx1262.cpp/.h` rămâne doar pentru rollback până la teste RF reale confirmate de utilizator; nu îl șterge pe baza unui simplu build.
- Motorul dinamic XMQ și hot reload-ul nu sunt încă implementate. Vezi `docs/SX126X_WMBUS_BRIDGE.md`.


## Actualizare hardware — 11 noiembrie 2026 (Heltec, log 75)

**M1 T1/A: VALIDAT PE HARDWARE (recepție și publicare).** În configurația reală cu `sx126x_id`, un singur transceiver SX126x nativ livrează cadre CRC-verificate către `wmbus_radio`, apoi către decodoarele **existente** `wmbus_common` / `wmbus_meter`. În captura din 11 noiembrie au publicat valori **12/12 contoare configurate**: 7 repartitoare `fhkvdataiii` (HCA, anterior, temperaturi, RSSI) și 5 apometre `mkradio4` (target/total, RSSI). Această captură confirmă T1/A pe hardware, nu C1 și nu o probă de stabilitate pe termen lung.

**Mesaje / starea corectă:** prefixul de log `wmbusmeters` arată **decodorul de aplicație încă activ**, nu că driverul vechi de transceiver SX1262 a fost pornit. În configurația `sx126x_id`, receptorul vechi nu este instanțiat; fișierele `transceiver_sx1262.cpp/.h` există încă în surse pentru rollback. Mesajele `Telegram not handled` pentru ID-uri neconfigurate sunt normale; `3-of-6 symbol rejected` indică respingerea cadrelor invalide, nu erori ale valorilor deja publicate.

**Stare rămasă:** C1-B pe noua punte, testele RF de durată și scenariile negative (cheie greșită / corupere / recuperare) nu sunt confirmate de această captură. Motorul comun XMQ și actualizarea regulilor fără OTA **NU sunt implementate**. Nu elimina `wmbus_common` sau decodoarele sale: ele furnizează acum valorile publicate. Ștergerea definitivă a transceiverului legacy rămâne separată de această validare și necesită criteriile de rollback/regresie din `docs/SX126X_WMBUS_BRIDGE.md`.


## Etapa M1 — curățare native-only (11 noiembrie 2026)

**IMPLEMENTAT ÎN SURSE, CI/HARDWARE PENTRU NOUL COMMIT ÎNCĂ NEVALIDATE:** `wmbus_radio` acceptă doar `sx126x_id` și folosește exclusiv componenta `sx126x` pentru SPI/IRQ. Au fost eliminate schema `radio_type`, clasele `RadioTransceiver`, `transceiver.cpp/.h`, `transceiver_sx1262.cpp/.h`, taskul FreeRTOS și coada radio legacy. `Frame` a rămas, iar `packet.cpp/.h` conțin exclusiv API-ul `Frame` folosit de `wmbus_meter`.

**PĂSTRAT FĂRĂ ÎNLOCUIRE:** `wmbus_common` și driverele `fhkvdataiii` / `mkradio4`, identificarea contoarelor, verificarea CRC, RSSI, senzorii HA. Mesajele de aplicație `wmbusmeters` rămân corecte. Nu există încă interpret XMQ dinamic.

**Rollback:** în această ramură schema legacy nu mai este validă; recuperarea cere commit Git anterior / firmware salvat. Testul CI `sx1262-legacy-rollback.yaml` verifică acum **respingerea** vechii scheme. Captura hardware cu 12/12 contoare validează baza de dinaintea curățării, nu dovedește încă noul commit. Nu face OTA până la CI verde și verificarea YAML-ului privat.


## C1 Format A/B — etapă software (12 noiembrie 2026)

**IMPLEMENTAT ÎN COD, ÎNCĂ NEVALIDAT CI/RF PENTRU ACEST COMMIT:** `wmbus_radio` acceptă C1 Format A (`54 CD`) și Format B (`54 3D`) în `native_stream_mode: T1_C1`; vechiul `T1_C1_B` rămâne alias compatibil. Pentru C1-A se verifică lungimea fizică cu toate blocurile CRC și se folosește `trimCRCsFrameFormatA`. Pentru C1-B se verifică lungimea fizică și `trimCRCsFrameFormatB`; în `wmbus_common/wmbus.cc`, lungimea CRC2 a fost corectată la `crc2_pos - (crc1_pos + 2)`, cu limite explicite, eliminând citirea în afara vectorului. Nu se mai respinge arbitrar orice B peste 128 octeți. Limita actuală de recepție PHY SX126x pentru fluxul combinat rămâne 255 octeți cu prefix inclus.

CI: `tests/test_c1_crc_safety.py` extrage funcțiile reale Format A/B din `wmbus.cc` și le verifică pe date sintetice sub ASan/UBSan; se validează schema YAML combinată. **Aceasta NU este validare RF C1**. Nu modificăm calea T1/A, `wmbus_meter`, catalogul driverelor sau configurația reală de pe Heltec până la testul C1 controlat.


## Etapa 2 — diagnostic și protecții PHY T1/C1 (12 noiembrie 2026)

În `components/sx126x/sx126x.cpp` sunt adăugate contoare de prefixe C1/A (`54 CD`) și C1/B (`54 3D`), C1 FIFO întârziat și lungime fizică invalidă. În modul combinat se afișează sumarul `COMBINED PHY health` la 30 s. Mesajele detaliate de prefix FIFO se activează numai prin `native_phy_diagnostics: true` din `wmbus_radio`. În `component.cpp` se numără distinct C1/A și C1/B validate CRC, precum și respingerile CRC/lungime. Toate sunt diagnostice; nu publică telegrame fără CRC valid.

Protecție suplimentară: T1 este respins înainte de FIFO dacă lungimea finală 3-of-6 este peste 255 octeți. `wmbus_common` și driverele rămân nemodificate în această etapă. CI verifică schema YAML de diagnostic. **Hardware C1 nevalidat**: logul Heltec (76) dovedește T1/A dar niciun C1 valid. Sync-word-ul hardware `0x543d` singur nu dovedește recepție simultană T1 și C1/A+B. Configurația T1 funcțională rămâne implicită.


## FIFO-ahead C1 test (12 noiembrie 2026)

Logul hardware (77) a capturat 2 evenimente C1/A `L=0x36`, `target=65`, `RX_ADDR_PTR=75/81` imediat după boot. Codul anterior respingea aceste două cadre doar fiindcă pointerul depășea lungimea. Noua regulă în `components/sx126x/sx126x.cpp` numără evenimentul ca `c1_late`, citește **doar prefixul de `target` octeți** (maxim 255, fără wrap) și livrează candidatul adaptorului unde `trimCRCsFrameFormatA/B` validează toate CRC-urile. Nu se face publicare fără CRC valid. Noul mesaj este `COMBINED C1 FIFO ahead ... bounded CRC candidate`, nu `discarded`. `c1_late` este număr de tentative, nu de telegrame recuperate; pentru rezultat se compară `C1 PHY adapter verified A/B` cu `C1 CRC rejected`. Calea T1/A este nemodificată. Necesită CI verde + verificare hardware, fără garanție de recuperare pentru FIFO deja suprascris.
