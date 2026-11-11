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
