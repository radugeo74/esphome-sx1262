> **Bază corectată (11.11.2026):** această ramură pornește din [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf), commit `c4284739be7f9a5ee251de268ae3904068906ca4`. Păstrează motorul istoric wmbusmeters C++ (`wmbus_common`, `wmbus_meter`, `wmbus_radio`) și include separat driverul `components/sx126x` ESP-IDF patch-uit, preluat ca întreg din commitul `bd3035ed`. Cele două recepții NU sunt încă integrate: un adaptor RF spre motorul XMQ trebuie implementat și testat. Motorul XMQ dinamic și actualizările fără OTA sunt obiective, **nu funcții existente**.

# Roadmap de implementare — XMQ Runtime Engine

**Aici începem de la zero la nivel de arhitectură dinamică.** Baza `native-espidf` a avut builduri native validate istoric; componenta SX126x ESP-IDF separată a fost importată, dar combinația trebuie recompilată și testată, dar nu este o implementare de reload dinamic.

## M0 — Ramură nouă din native-espidf și patch radio

**Stare:** DOCUMENTAT.

- Ramură separată din `native-espidf` la commitul `c4284739`, plus subarborele SX126x patch-uit din `bd3035ed`.
- README, PROJECT_CONTEXT, AGENTS și planuri scrise pentru noua direcție.
- Separarea clară dintre codul vechi de referință și motorul nou.
- Fără flash/OTA.
- Componenta `components/sx126x` inclusă intactă, dar **adaptorul spre `wmbus_meter` este încă de implementat**.

**Criteriu de ieșire:** documentația și noua ramură sunt create în GitHub.

## Etapă intermediară — integrarea SX126x în pipeline-ul legacy

**Stare:** implementare experimentală în noua ramură; verificare prin compilare ESPHome și apoi prin testare RF. Componenta radio patch-uită livrează cadrele validate către interfața `wmbus_radio::Frame` consumată de contoarele existente. Vechiul transceiver rămâne pentru rollback. Fără a confunda această etapă cu motorul XMQ actualizabil dinamic.

## M1 — Un motor generic de date, iPERL (fără HA)

**Stare:** DE IMPLEMENTAT.

- Schema binară/CBOR (decizie după măsurători) pentru operatorii de bază DIF/VIF, scalare și status.
- Parser/validator XMQ generic pe PC, nu generator C++ per-driver.
- Interpreter C++ generic încărcând un descriptor din buffer la runtime.
- Aceeași aplicație C++ decodează versiunile A/B ale `iperl.xmq` fără recompilare.
- Mutations/reject, token-limit/stack-limit, reproducibilitate SHA, validare ABI.

**Criteriu:** același hash al binarului, descriptor diferit, valori corecte, erori respinse. Nu numi motorul „dinamic” fără demonstrație.

## M2 — Reîncărcare și persistare pe Heltec

**Stare:** DE IMPLEMENTAT.

- Persistență A/B în flash, instalare staging și activare atomică.
- Reload fără restart ESPHome dacă este sigur; altfel restart controlat dar **fără flash firmware**.
- Fallback după power loss, fișier corupt, memorie insuficientă și Wi-Fi offline.
- Măsurare flash, heap minim, fragmentare, latență și watchdog.

**Criteriu:** update repetat iPERL pe dispozitiv, reconectare/reboot fără pierdere de date și fără firmware nou.

## M3 — Sincronizare automată Home Assistant

**Stare:** DE IMPLEMENTAT.

- Urmărirea directoarelor `.xmq`, SHA-256 și proveniență.
- Fixture-uri testate automat în serviciul host; format pachet determinist.
- Distribuție autentificată către Heltec; ACK/rollback/status.
- Schimbare locală XMQ → nou comportament RF fără rebuild firmware, fără intervenție manuală pentru modificări compatibile.
- Configurație explicită a surselor; upstream wmbusmeters opțional.

**Criteriu:** test e2e „editez XMQ pe HA, motorul preia și aplică noile reguli”, fără comandă de flash.

## M4 — IXML și calcule: contoarele reale

**Stare:** DE IMPLEMENTAT.

- Extindere de **operatori comuni** pentru gramatici IXML, date calendaristice, `calculate`, `lookup`, `library.use`.
- `mkradio4` și `fhkvdataiii`: verificare paritate pentru fixture-urile originale și pentru contoarele configurate în HA.
- Cheile rămân strict separate; orice variantă criptată necunoscută se respinge.
- Fixarea semnificației câmpurilor `current_hca` vs. noi profile OMS (nu se substituie în tăcere).

**Criteriu:** valorile celor 6 contoare reale rămân corecte, update de logică XMQ fără firmware nou, benchmark de resurse pe Heltec.

## M5 — Catalogul de 52 de familii

**Stare:** DE IMPLEMENTAT.

- Migrare pe grupe de funcții XMQ, **nu** pe noi decodoare C++.
- Fiecare familie intră numai cu matrice de compatibilitate, fixture-uri și limite declarate.
- Profilele AES sunt separate de cele clear; nu există auto key discovery.
- Compatibilitate cu câmpurile vechi HA și respingere a modelelelor neconfigurate.

**Criteriu:** 52/52 profile validate pentru subsetul XMQ documentat; posibilitatea de a încărca un XMQ nou compatibil fără modificări C++ individuale.

## M6 — Stabilizare, eliminarea fallbackului și instalare controlată

**Stare:** DE IMPLEMENTAT.

- Benchmark RAM/flash și test de durată.
- Opțiuni de activare opt-in pentru experimente, fallback anterior până la confirmare RF reală.
- Eliminare a vechilor decodoare exclusiv după paritate și hardware.
- Release note, backup, rollback și instrucțiuni pentru flash autorizat de utilizator.

**Ordinea imediată:** M1, apoi M2 și M3. Nu începem din nou migrarea în calupuri de 2–3 drivere înainte să trecem demonstrabil de M1.
