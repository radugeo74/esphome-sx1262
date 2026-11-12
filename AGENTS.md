# Instrucțiuni specifice ramurii dual-radio (12 noiembrie 2026)

Pe `experimental/dual-radio-sx126x-sx127x` sunt **două backenduri**: SX126x stabil și SX127x experimental. Alege exact un radio per configurație, fără doi proprietari SPI. Nu elimina/adapta SX126x ca să rezolvi SX127x. Pentru SX127x folosește override-ul `components/sx127x`, `dio1_pin` și `wmbus_t1_mode: true`. T1 și C1/A+B sunt implementate software, **nevalidate RF**; configurația combinată SX127x folosește `sync_value: [0x54]` și `native_stream_mode: T1_C1`. Nu declara firmware-ul SX127x gata de instalare doar pe baza CI. Adresele străine trebuie să păstreze mesajele `Unhandled meter address` și URL-ul analizorului, fără broadcast către toți handlerii.

**Documentație curentă:** primele secțiuni din `README.md` și `PROJECT_CONTEXT.md`. Restul instrucțiunilor de mai jos descriu ramura XMQ din care s-a derivat această ramură.

---

# AGENTS.md — proiect XMQ runtime din native-espidf

**IMPORTANT:** această ramură se bazează pe `native-espidf` commit `c4284739`, nu pe ramura XMQ-only. Păstrăm vechiul wmbusmeters C++ ca referință. Componenta radio `components/sx126x` se copiază nemodificată din commit `bd3035ed`.

## Reguli pentru Codex și asistenți

1. Înainte de orice cod, citește `README.md`, `PROJECT_CONTEXT.md`, `docs/SX126X_NATIVE_BASELINE.md` și roadmap-ul.
2. **Radio:** nu înlocui `components/sx126x` cu varianta oficială nepachetată și nu pierde fixurile BUSY înaintea SPI, `is_failed()`, stream T1 și `packet_transport`. Nu lega două drivere la același SX1262 în YAML.
3. **Integrare:** noul `components/sx126x` nu este conectat automat la `wmbus_radio`. Construiește un adaptor/test clar înainte de a afirma că pipeline-ul modern e activ; păstrează paritatea RF și senzorii existenți.
4. **XMQ:** `.xmq` original wmbusmeters este sursa; fără noi `driver_*.cc`, `*_plain.h` sau `generate_<driver>.py` pentru fiecare driver. Creează compiler de date generic + interpret C++ comun.
5. **Actualizări:** modificare compatibilă `.xmq` → pachet de date → reload runtime fără firmware nou; operator nou = `UNSUPPORTED`, nu interpretare ghicită.
6. **Securitate:** nu relaxa CRC/TPL/ELL, ID configurat și AES. Nu publica chei/ID-uri/private logs în Git.
7. Testează fixture-uri originale (clear, encrypted și post-decrypted separate), mutații, rollback, C++ host, ESP-IDF S3 și ESPHome; nu confunda verde CI cu validarea pe Heltec.
8. Păstrează `NATIVE_ESPIDF_REPORT.md` ca raport istoric fără rescriere. Nu flash-ui dispozitive și nu modifica configurația activă.
9. După milestone actualizează `PROJECT_CONTEXT.md` și `docs/XMQ_RUNTIME_ROADMAP.md` cu **implementat / testat / planificat**.

11. Puntea radio nouă: schema `wmbus_radio.sx126x_id` trebuie să creeze un singur proprietar SPI, iar `wmbus_meter` să primească exclusiv cadre cu CRC verificat. Vezi `docs/SX126X_WMBUS_BRIDGE.md` și fișierul de test. Nu șterge vechiul SX1262 până la validarea RF reală și confirmarea rollbackului.
