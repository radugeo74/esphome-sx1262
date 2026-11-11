> **Bază corectată (11.11.2026):** această ramură pornește din [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf), commit `c4284739be7f9a5ee251de268ae3904068906ca4`. Păstrează motorul istoric wmbusmeters C++ (`wmbus_common`, `wmbus_meter`, `wmbus_radio`) și include separat driverul `components/sx126x` ESP-IDF patch-uit, preluat ca întreg din commitul `bd3035ed`. Cele două recepții NU sunt încă integrate: un adaptor RF spre motorul XMQ trebuie implementat și testat. Motorul XMQ dinamic și actualizările fără OTA sunt obiective, **nu funcții existente**.

# Criterii și teste de acceptare — XMQ Runtime Engine

**Aceste teste sunt PLANIFICATE**, nu rapoarte de execuție. Testele moștenite nu dovedesc încă hot reload.

## A. XMQ → descriptori, fără C++ nou

- [ ] Parsează originale `iperl.xmq`, `mkradio4.xmq`, `fhkvdataiii.xmq`; raportează exact câmpurile, operatorii și fixture-urile suportate.
- [ ] Compilatorul host generează aceeași serializare și SHA pentru același conținut.
- [ ] Schimbarea unui selector `match` sau a unui nume/expresii valide produce descriptor nou, nu schimbare de cod C++.
- [ ] Construcție XMQ/IXML necunoscută → eroare explicită, nu ignorare.
- [ ] Fixture deja-decriptat nu primește drept de decodare RF `NOKEY`.
- [ ] Limite impuse pentru fișier, nesting, număr câmpuri, lungimi, overflow și calcul.

## B. Un interpret C++ comun

- [ ] Un singur binar rulează regulile din doi descriptori XMQ diferiți fără recompilare.
- [ ] Teste de paritate JSON față de wmbusmeters pentru volum/debit, temperaturi, date, lookup-uri și status.
- [ ] Fiecare selector DIF/DIFE/VIF/VIFE, storage, tariff și subunit este verificat independent; duplicatele se resping când nu sunt admise.
- [ ] BCD/semn/scalare, calendares, formule și IXML au teste de limite și cazuri negative.
- [ ] Fuzz: descriptor/telegramă invalide nu produc crash sau acces în afara bufferului.

## C. Motorul pe ESP32-S3

- [ ] CRC DLL și clasificarea TPL/ELL sunt obligatorii înainte de XMQ.
- [ ] ID contor neconfigurat sau cheie AES greșită → fără publicare.
- [ ] Test cu pierderea alimentării în fiecare etapă de actualizare; ultima versiune validă se încarcă la reboot.
- [ ] Test de modificare `.xmq` cu firmware SHA neschimbat și valori de ieșire schimbate conform fixture.
- [ ] Update incomplet sau incompatibil → rollback și raport de eroare, nu schimbare parțială.
- [ ] Heap liber/minim, largest block, fragmentare, mărime flash și durata decodării măsurate pe hardware real.
- [ ] Wi-Fi indisponibil: continuă decodarea cu ultimul pachet bun.

## D. Sincronizare Home Assistant

- [ ] Watcher local observă schimbările `.xmq` prin hash (nu numai mtime).
- [ ] Compilerul host verifică toate fixture-urile relevante înainte de transfer.
- [ ] Transfer doar pe canal local autentificat, fără a expune porturi noi la internet.
- [ ] După ACK, HA și dispozitiv raportează același hash activ.
- [ ] La update refuzat, eroarea și versiunea activă rămân vizibile în HA.
- [ ] Chei și date private nu ajung în XMQ, artefacte GitHub, loguri sau pachete distribuite.

## E. Regresie și compatibilitate

- [ ] Paritate cu cele 6 contoare din locuință înainte de înlocuirea vechiului motor.
- [ ] Nu se schimbă semnificația câmpurilor `current_hca` și `previous_hca` fără o migrare explicită și verificată.
- [ ] Nu se șterg sau redenumesc neintenționat entitățile Home Assistant.
- [ ] Teste native pe PC, ESP-IDF ESP32-S3, ESPHome Heltec: toate SUCCESS.
- [ ] Niciun flash automat pe Heltec; activarea noului firmware doar cu aprobarea utilizatorului.

## Definiții precise de stare

- **IMPLEMENTAT**: cod prezent pe ramura curentă și conexiune reală între componente.
- **TESTAT SOFTWARE**: teste CI asociate commitului curent sunt verzi.
- **VALIDAT HARDWARE**: experiment pe Heltec și Home Assistant real, cu măsurători și loguri.
- **PLANIFICAT**: specificație, fără afirmații că funcționează.

Nu raporta „driver XMQ dinamic” până când criteriul critic de **actualizare de date cu firmware SHA identic** este trecut.
