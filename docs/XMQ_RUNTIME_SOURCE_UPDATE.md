> **Bază corectată (11.11.2026):** această ramură pornește din [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf), commit `c4284739be7f9a5ee251de268ae3904068906ca4`. Păstrează motorul istoric wmbusmeters C++ (`wmbus_common`, `wmbus_meter`, `wmbus_radio`) și include separat driverul `components/sx126x` ESP-IDF patch-uit, preluat ca întreg din commitul `bd3035ed`. Cele două recepții NU sunt încă integrate: un adaptor RF spre motorul XMQ trebuie implementat și testat. Motorul XMQ dinamic și actualizările fără OTA sunt obiective, **nu funcții existente**.

# Protocol propus: actualizare automată a driverelor XMQ

> **Stare: SPECIFICAȚIE, NEIMPLEMENTAT.** Scop: schimbare fișier `.xmq` fără recompilarea/flash-uirea firmware-ului.

## Sursa și evenimentul

Sursa implicită: un director controlat de utilizator pe Home Assistant, în care se află fișierele XMQ originale. Un serviciu urmărește **conținutul**, nu doar timestamp-ul (SHA-256). Surse alternative: upstream wmbusmeters la un commit verificat sau o copie locală revizuită. Toate păstrează identitatea/versiunea sursei.

Exemplu semantic, **nu YAML valid implementat**:

~~~yaml
xmq_runtime:  # SPEC, NU ESTE CONFIGURARE ESPHOME ACTUALĂ
  sources:
    local_directory: /config/wmbus-xmq/drivers
    upstream: optional
  update:
    on_file_change: true
    apply_compatible_after_tests: true
    preserve_last_known_good: true
~~~

## Mașina de stări a actualizării

`DISCOVERED → VALIDATING → COMPILED → TESTED → STAGED → ACTIVATED`

- `DISCOVERED`: fișierul diferă ca hash de versiunea activă.
- `VALIDATING`: sintaxă și identitate XMQ valide, seturi de operatori acceptați; fișiere/chei neacceptate respinse.
- `COMPILED`: rezultă descriptor **de date**, cu schema ABI corespunzătoare motorului C++ aflat pe Heltec.
- `TESTED`: toate fixture-urile originale relevante, inclusiv cazuri negative; verificată maparea numelor/unităților.
- `STAGED`: pachetul este transmis autentificat în memoria/slotul secundar al dispozitivului; verificare integritate și compatibilitate.
- `ACTIVATED`: noul driver intră în vigoare între două telegrame, cu confirmare de la dispozitiv și ultimul pachet vechi păstrat.
- `REJECTED`: orice eroare de sintaxă, lipsă operator, criptare/config neacceptat, depășire buget, integrare eșuată.
- `ROLLED_BACK`: un pachet anterior activat este retras după eroare detectată; se restaurează ultima versiune bună.

Nicio tranziție eșuată nu autorizează publicarea unor valori necunoscute.

## Exemplu de comportament așteptat

1. `iperl.xmq` valid, versiunea A, este activ pe Heltec.
2. Utilizatorul modifică o definiție de câmp compatibilă în fișierul HA, păstrând fixture-urile/regulile consistente.
3. Watcherul detectează noul hash și pornește automat verificările.
4. Compilerul produce pachetul B; firmware-ul rămâne *identic*.
5. Heltec primește, validează, activează B și confirmă hashul B.
6. Telegrama următoare este decodată cu noile reguli, senzorii rămân coerenți.

**Exemplu negativ:** se adaugă în `.xmq` o funcție IXML/XPath pe care motorul nu o înțelege. Compilerul o respinge, dispozitivul păstrează pachetul A, iar HA marchează update-ul ca nereușit. Nu se face OTA și nu se presupune un rezultat.

## Upstream automat vs. sursă locală

- **Fișiere locale:** modificarea este detectată și aplicată automat dacă validarea și testele trec.
- **wmbusmeters upstream:** detectarea modificărilor poate fi automată; nu se execută arbitrar semantici noi. Fișierele noi/necunoscute se pun în staging până când motorul poate demonstra compatibilitatea.
- **Fără internet:** rămâne disponibilă ultima configurație/versiune activată; recepția contoarelor nu este dependentă de Github.

## Protocol/transport — de implementat

- Endpoint de transfer autorizat prin conexiunea locală HA ↔ ESPHome / canal dedicat autentificat, fără porturi expuse pe internet.
- Mesaje `BEGIN` (driver, hash, lungime, ABI), blocuri cu offset și CRC, `VERIFY`, `ACTIVATE`, `ACK` și `ROLLBACK`.
- Retry și reluare blocuri după Wi-Fi instabil, rată limitată și timeout finit.
- Verificare SHA-256/semnătură, limite stricte de mărime și validare înainte de utilizare.
- Un update nu modifică niciodată chei sau lista de ID-uri autorizate; acestea rămân în configurația securizată.

## Observabilitate

HA afișează: driver, versiune sursă, SHA activ, SHA descoperit, etapă, eroare, data activării, pachet anterior disponibil. Nu se loghează conținut criptat sensibil, chei, identificatori privați inutili.

Testul de acceptare critic: **diferență de hash și de rezultat la modificarea XMQ, fără diferență de firmware SHA**.
