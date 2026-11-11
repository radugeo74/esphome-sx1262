> **Bază corectată (11.11.2026):** această ramură pornește din [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf), commit `c4284739be7f9a5ee251de268ae3904068906ca4`. Păstrează motorul istoric wmbusmeters C++ (`wmbus_common`, `wmbus_meter`, `wmbus_radio`) și include separat driverul `components/sx126x` ESP-IDF patch-uit, preluat ca întreg din commitul `bd3035ed`. Cele două recepții NU sunt încă integrate: un adaptor RF spre motorul XMQ trebuie implementat și testat. Motorul XMQ dinamic și actualizările fără OTA sunt obiective, **nu funcții existente**.

# Arhitectură propusă — XMQ Runtime Engine

> **Design / NU este încă implementat.** Nu confunda acest document cu firmware deja funcțional.

## 1. Cerința și alegerea arhitecturală

Un fișier `.xmq` este sursa canonică pentru comportamentul unui driver wmbusmeters. Dacă sursa se modifică, sistemul trebuie să preia automat noile definiții **fără modificarea codului C++ și fără compilare/flash al firmware-ului ESPHome**, cu condiția ca noul XMQ să folosească operatori deja acceptați de motor.

Este intenționat diferit de vechiul model **XMQ → C++ per-driver → firmware nou**. Noul model va fi **XMQ → descriptori de date compactați → același interpret C++**, cu schimbarea descriptorului la runtime.

Portarea integrală a motorului desktop `driver_dynamic.cc`/parserului XMQ/IXML pe ESP32-S3 ar mări semnificativ codul și RAM-ul. Acesta rămâne un experiment de evaluat, nu o cerință inițială. Varianta preferată este compilarea semantică **pe Home Assistant** și interpretarea **pe Heltec**. „Compilare” aici înseamnă conversia XMQ în **date**, nu în cod executabil sau firmware.

## 2. Flux logic

~~~text
wmbusmeters *.xmq upstream ──┐
                              ├──> depozit de XMQ curate, urmărite de HA
XMQ locale ale utilizatorului ─┘             │
                                            modificare/hash/versiune
                                              ↓
                                 parser XMQ + IXML pe HA
                                              ↓
                           normalizare semantică + compilator
                         DIF/VIF, formule, lookup, date, IXML
                                              ↓
                             fixture-uri upstream + teste
                                 (respinge necunoscut)
                                              ↓
                                pachet de reguli versiuni
                         driver, sursă SHA, schema ABI, integritate
                                              ↓
                           transfer LAN către Heltec autentificat
                                              ↓
                  slot staging → validare → activare atomică A/B
                                              ↓
                  interpretul C++ unic + transport/securitate
                                              ↓
                                 senzori Home Assistant
~~~

Nu există actualizări „magice”: pentru surse modificate ce introduc **un nou operator** XMQ/IXML, noua versiune este raportată `UNSUPPORTED` și rămâne activ vechiul pachet până la implementarea operatorului în motorul comun. Aceasta este singura situație în care o nouă versiune de firmware poate deveni necesară.

## 3. Despărțirea responsabilităților

### Radio / transport — cod existent

Recepția existentă este `wmbus_radio/transceiver_sx1262.cpp` (T1/C1) din `native-espidf`. Am importat separat `components/sx126x` din implementarea patch-uită (BUSY înainte de SPI, fail-fast, stream T1), dar **nu există încă un adaptor** care să alimenteze `wmbus_meter` cu acesta. Verificările DLL CRC, ID, TPL/ELL și securitate trebuie păstrate și testate independent înaintea interpretării XMQ.

### Securitate — cod existent și extensii controlate

Chei AES doar în configurația privată `!secret`, asociate explicit ID-ului/driverului; nu în XMQ, fișier transferat sau loguri. Un cadru cu criptare sau MAC nerecunoscut nu este decodat ca plaintext. Update-ul unei definiții nu poate transforma un caz „NOKEY în fixture” într-un caz „RF fără criptare”.

### Compilator XMQ host — o singură implementare generică

Parcurge structura declarativă XMQ, verifică modelele și interpretează semantic:
- `detect/mvt`, identitate și metadate;
- `fields.field` cu `name`, `quantity`, unități și chei;
- `match`: `difvifkey`, funcție, VIF/VIFE, index/storage/tariff/subunit și combinații;
- `calculate`: AST cu operatori aritmetici/calendaristici acceptați;
- `lookup/map`, status, `library.use`;
- `ixml`: gramaticile proprietare ale producătorului, transformate în reguli portabile de parsare;
- fixture-urile originale XMQ, inclusiv clase „on-air clear”, „post-decrypted”, „encrypted”; niciodată cu echivalarea acestor clase.

Parserul trebuie să refuze construcțiile necunoscute; nu sunt permise ignorări silențioase sau transformări care schimbă unitățile.

### Pachetul de driver — numai date

Formatul final încă nu este ales. Cerințe minime:
- identificator de schemă + versiune ABI, versiune driver, sursa commit/hash SHA-256;
- semnătură de integritate/autorizare sau autentificare echivalentă a originii;
- lista de modele și selectori, operatori tipizați, constante/lookup și secvențe IXML;
- lista câmpurilor expuse și compatibilitatea lor cu entitățile Home Assistant;
- limite explicite pentru numărul de reguli, lungimea fișierelor, adâncimea AST și memoria alocată;
- serializare deterministă, fără cod arbitrar, fără bytecode nativ executabil, fără chei AES.

Transferul poate folosi un protocol propriu local/ESPHome API ori o resursă HA locală autentificată. Trebuie aleasă **o singură cale suportată** pe baza unui prototip, nu deschiderea unui nou port public.

### Interpretul C++ din firmware

Încarcă numai descriptorii verificați, compatibili cu ABI-ul local. Evaluează operatori tipizați într-un interpretor bounded. Nu folosește heap necontrolat și nu alocă proporțional cu telegrama nesigură. Nu conține reguli de format specifice fiecărui model.

Decoderul va produce `numeric`, `text`, `date`, `raw` și `status` printr-un catalog de câmpuri. Numele și valorile vin din reguli; transportul/senzorii rămân stabili.

## 4. Persistență și actualizare sigură

- Starea activă: ultimul pachet compatibil, verificat și aprobat automat.
- Staging: pachet nou într-un slot separat, cu manifest și SHA/semnătură.
- Activare: exclusiv la o limită sigură între telegrame; noile reguli se aplică integral, niciodată parțial.
- Reboot / pierdere alimentare: reia ultimul slot verificat. Nu șterge ambele sloturi la un update nereușit.
- Dacă modelul nou nu trece probele locale sau limitele de resurse, respinge update-ul. Păstrează decodarea stabilă cu driverul anterior.
- Dacă un câmp nou necesită entitate HA nouă, acesta este înregistrat/activat controlat; nu redenumi în tăcere câmpurile existente.

## 5. Declanșarea schimbării XMQ

Sursa practică inițială: director de fișiere urmărit pe Home Assistant, nu acces GitHub direct de pe Heltec. Modificarea determină recalcularea hashului și validare/regenerare de *date*, urmată de transfer local și reload. Upstream GitHub poate fi o sursă suplimentară, printr-o sincronizare care compară commituri și verifică proveniența.

**Auto-apply** este permis numai pentru pachetele de date care au trecut validarea și folosesc operatori existenți. Alte pachete sunt `NEEDS_REVIEW`/`UNSUPPORTED`; motorul nu adoptă semantică necunoscută.

## 6. Limitări hardware și evaluarea prototipului

Heltec: ESP32-S3, 8 MB flash, Wi-Fi anterior aprox. −82…−86 dBm și heap liber istoric aprox. 158–160 KB. Acestea sunt **observații anterioare**, nu bugete garantate.

Se vor măsura înainte de alegerea formatului final: heap minim, cel mai mare bloc liber, fragmentare, mărimea descriptorilor, flash ocupat, latența de decodare, reboot și recuperare după conexiuni pierdute. Nu declara capacitate pentru 52 de fișiere încărcate simultan înainte de măsurători.

## 7. Relația cu proiectul moștenit

Implementările native din commitul de bază rămân disponibile pentru:
- verificarea rezultatelor de referință și a securității;
- compatibilitate până la aprobarea pe RF a motorului nou;
- comparații exacte înainte de eliminarea de cod duplicat.

Nu se adaugă noi `<driver>_plain.h` ca direcție de dezvoltare. Parserul XMQ existent în Python și decodoarele C++ existente **nu devin automat** un motor cu hot reload prin această schimbare de documentație.
