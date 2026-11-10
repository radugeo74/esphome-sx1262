## Etapa initiala a compilatorului XMQ comun (2026-11-10)

Dupa auditul celor 52 de surse, `iperl` si `munia` sunt
primele doua decodoare portabile care folosesc efectiv selectori
`match.measurement_type`, `match.vif_range`, unitati si
nume de iesire **generate din XMQ la compilare**, prin:
`generate_standard_selectors.py` ->
`xmq_generated_standard_selectors.h` ->
`xmq_standard_match_runtime.h`.

Nu mai aleg manual din C++ intervalele VIF pentru volum/debit
iPERL si temperatura/umiditatea Munia. Numele senzorilor
numerici `total_m3`, `max_flow_m3h`,
`current_temperature_c` si `current_relative_humidity_rh`
sunt emise din declaratiile XMQ. Testul de mutatie verifica
ca redenumirea unui camp schimba iesirea generata, iar o
semantica incompatibila blocheaza generarea.

**Aceasta este doar o migrare partiala.** Formatul RF, recunoasterea
ID-ului, criptarea, codurile de stare si constrangerile
`fixture` sunt pastrate separat in C++ pentru siguranta.
Nici `iperl`, nici `munia` nu sunt inca decodoare complet
declarative, iar celelalte 41 de decodoare portabile au in
continuare algoritmi manuali. Numericul de 2 drivere generate
IXML complet (`mkradio4`, `fhkvdataiii`) NU a fost modificat.

## Contract obligatoriu: XMQ upstream -> generator nativ (2026-11-10)

**IMPORTANT:** Toate cele 52 de familii folosesc implementari native
ESPHome, dar **NUMAI 2** (`mkradio4`, `fhkvdataiii`) sunt generate
functional din IXML/XMQ. Cele 43 de decodoare portabile au inca logica
C++ manuala, chiar daca unele consuma tabele XMQ generate. Nu declara
migrarea terminata pana cand selectorii, valorile, formulele si
statusurile sunt compilate efectiv din sursa XMQ si verificate CI.

Auditul comparativ upstream la revizia
`f6ab1c28dab6f953198a4bfcbcfc353378951847`
a gasit 51 din 52 surse XMQ identice; `fhkvdataiii` este in urma,
cu doua fixture-uri suplimentare si un profil OMS 0x08 diferit.
Nu actualiza sau instala orbeste: trebuie protejate numele de camp
`current_hca`/`previous_hca` fata de `current_consumption_hca`
si `consumption_at_set_date_hca`.

Vezi [planul de migrare si auditul surselor](experimental/xmq/UPSTREAM_SOURCE_MIGRATION.md).
Auditul are acum un instrument de comparare/staging fara modificarea
sursei active: `experimental/xmq/tools/audit_upstream_drivers.py`.
Un update al XMQ implica regenerare, teste, compilare si firmware nou,
**nu** update dinamic la runtime. Nicio schimbare in recepția SX126x.

## Landis+Gyr Ultraheat — clear RF, TPL status și data cu secunde (2026-11-10)

Cele trei exemple oficiale `ultraheat.xmq` sunt diferite: `MyUltra` /
`70444600` este **M-Bus cablat**, `MyUltra2` / `71635605` este
**wireless CI=7A cu TPL mode 0**, iar `MyUltra3` / `72905678`
este **ELL/AFL cu AES-CMAC și TPL AES-CBC**, cu aplicație deja
decriptată în fixture-ul original. Nu transformăm `NOKEY` ori
datele în clar dintr-un exemplu în dovadă de RF necriptat.

Numai exemplul `71635605` este admis în decodorul nativ no-key,
după verificarea separată DLL CRC, ID configurat, model LUG/04/04,
securitate, octet de status `0x20`, configurație `0x2000` și
înregistrările originale. Câmpurile originale sunt:

| Câmp | Valoare |
| --- | ---: |
| `heat_kwh` | 24277 |
| `volume_m3` | 5699.39 |
| `power_kw` | -0.2 |
| `flow_m3h` | 1.83 |
| `flow_c` | 35.1 |
| `return_c` | 35.2 |
| `status` (text) | TPL_STS_32 |
| `meter_datetime` (text) | 2023-05-20 10:08:12 |
| `tpl_status_raw` și `tpl_status_unknown_raw` | 32 |
| `error_flags_raw` și `error_flags_unknown_raw` | 0 |
| Toți cei șase `status_*` standard | 0 |

**TPL_STS_32 este bitul TPL necunoscut `0x20`**, nu
`POWER_LOW`, `ALARM` ori un cod de eroare inventat.
`06 6D` este decodat complet cu secundele păstrate și calendar
validat. Înregistrarea `02 FD 17` trebuie să fie zero.

Cele 10 câmpuri diagnostice sunt opționale în `xmq_sensors`;
`status` și `meter_datetime` sunt disponibile în
`xmq_text_sensors`, pentru ID-uri configurate explicit. Exemple
de configurare (ID demonstrativ din fixture-ul original):

```yaml
  meter_drivers:
    - meter_id: "71635605"
      driver: auto    # alternativ: ultraheat
  xmq_sensors:
    - meter_id: "71635605"
      field: heat_kwh
      sensor:
        name: "Ultraheat energie"
        unit_of_measurement: "kWh"
    - meter_id: "71635605"
      field: tpl_status_unknown_raw
      sensor:
        name: "Ultraheat status brut necunoscut"
  xmq_text_sensors:
    - meter_id: "71635605"
      field: status
      sensor:
        name: "Ultraheat stare"
    - meter_id: "71635605"
      field: meter_datetime
      sensor:
        name: "Ultraheat data contorului"
```

Teste native: 16 câmpuri numerice și două text, dată cu
secunde, refuzul stărilor și erorilor nevalidate, al datelor
invalide, cheii/transportului necunoscut, ID-urilor greșite,
capacității insuficiente și al cadrelor ELL. Testul Python
verifică toate cele trei JSON-uri originale, MVT și proveniența
de transport M-Bus/Mode0/ELL-AFL. Nu au fost modificate
recepția radio, firmware-ul instalat sau driverele existente.

## ESP-IDF ESP32-S3 probe — rezolvare dublare antet (2026-11-10)

În workflow #848, jobul ESP-IDF `stage-a-espidf-s3-portable-link`
a eșuat deoarece un singur fișier `main.cpp` includea
`pure_frame_header.h` prin două căi fizice diferite:
`components/wmbus_native_sx126x` și
`experimental/xmq/legacy_reference`. Cele două fișiere aveau
conținut identic; `#pragma once` nu acoperă fișiere distincte.
Am introdus un **include guard comun** în ambele copii, fără
să schimbăm codul de parsing, decodoarele, securitatea sau API-ul.
Acest lucru previne redeclararea `HeaderStatus`, `Header`
și `parse_header`. Fixul trebuie confirmat și prin jobul
ESP-IDF în CI; nu reprezintă probă RF.

## Sensus iPERL — diagnostice complete și proveniența criptării (2026-11-10)

Driverul `iperl` suportă în software cele două telegrame originale
cu antet TPL cu adevărat necriptat, ID `33225544`, model
`SEN/68/07`, `CI=7A`:

| Câmp | TPL=0x00 | TPL=0x06 |
| --- | ---: | ---: |
| `total_m3` | 123.529 | 123.529 |
| `max_flow_m3h` | 0 | 0 |
| `status` (text) | OK | ERROR POWER_LOW |
| `tpl_status_raw` | 0 | 6 |
| `status_error` | 0 | 1 |
| `status_power_low` | 0 | 1 |
| `status_alarm`, `status_busy`, `status_permanent_error`, `status_temporary_error`, `tpl_status_unknown_raw` | 0 | 0 |

Cei opt senzori numerici de diagnostic sunt opționali în
`xmq_sensors`, fără crearea automată a entităților. `status`
rămâne în `xmq_text_sensors`. Se acceptă numai TPL=0x00/0x06;
alte stări sunt respinse în locul publicării unor diagnostice
neconfirmate. `driver: auto` sau `driver: iperl` poate fi
folosit pentru ID-urile explicit configurate.

**Atenție la primul fixture original** (ID `12345699`):
sursa spune explicit „after decryption” și antetul are `N=1`,
deși `args` conține `NOKEY`. Valorile `total_m3=7.704`,
`max_flow_m3h=0` se verifică **doar** prin parserul de aplicație
deja decriptată. Telegrama respectivă nu este autorizată
pentru publicare directă din RF fără o decriptare autentificată
și o verificare separată a legării la dispozitiv.

Exemplu (ID public de fixture, nu un contor real):

```yaml
  meter_drivers:
    - meter_id: "33225544"
      driver: auto  # sau iperl
  xmq_sensors:
    - meter_id: "33225544"
      field: total_m3
      sensor:
        name: "iPERL consum apă"
        unit_of_measurement: "m³"
        accuracy_decimals: 3
    - meter_id: "33225544"
      field: max_flow_m3h
      sensor:
        name: "iPERL debit maxim"
        unit_of_measurement: "m³/h"
        accuracy_decimals: 3
    - meter_id: "33225544"
      field: status_error
      sensor:
        name: "iPERL eroare (0/1)"
    - meter_id: "33225544"
      field: status_power_low
      sensor:
        name: "iPERL baterie scăzută (0/1)"
  xmq_text_sensors:
    - meter_id: "33225544"
      field: status
      sensor:
        name: "iPERL stare"
```

Un nou test al XMQ-ului original verifică toate cele trei
fixture-uri, MVT, volume/debite, textul de stare și distincția
dintre post-decriptare și TPL mode zero. C++ verifică
`driver: auto`, ID configurat, toate câmpurile publicate,
capacitatea bufferului, cadrele invalide și CRC format B într-o
anvelopă sintetică; nu sunt probe RF fizice. Nu s-au modificat
tehnica radio, cheile, celelalte drivere sau firmware-ul instalat.

## WEP Munia — temperatură, umiditate și diagnostice complete (2026-11-10)

Cele două exemple originale din `munia.xmq` sunt `WEP/02/1B`
(ID `00013482`) și `WEP/04/1B` (ID `00220111`).
Au fiecare 47 de octeți (inclusiv octetul inițial de lungime
`0x2E`), CI=7A și TPL mode 0, fără criptare.

| Câmp | 00013482 | 00220111 |
| --- | ---: | ---: |
| `current_temperature_c` | 20.1 | 21.7 |
| `current_relative_humidity_rh` | 65.7 | 61.0 |
| `status` (text) | OK | OK |
| `tpl_status_raw`, `error_flags_raw` | 0 | 0 |
| `tpl_status_unknown_raw`, `error_flags_unknown_raw` | 0 | 0 |
| Toți cei șase indicatori `status_*` standard | 0 | 0 |

În `xmq_sensors` sunt disponibili opțional `tpl_status_raw`,
`tpl_status_unknown_raw`, `error_flags_raw`,
`error_flags_unknown_raw`, `status_alarm`, `status_busy`,
`status_error`, `status_permanent_error`,
`status_power_low` și `status_temporary_error`, pe lângă
temperatură și umiditate. În `xmq_text_sensors` rămâne `status`.

Se publică `OK` și indicatoarele zero **numai** dacă ambele
câmpuri de status (TPL și înregistrarea `02FD971D`) sunt
zero, după verificarea DLL CRC, a ID-ului configurat, a modelului,
a securității și a profilului BCD. Telegramelor cu erori,
criptate sau necunoscute li se refuză publicarea; nu se ghicește
semnificația biților suplimentari.

Exemplu cu ID din fixture (înlocuiți-l pentru un dispozitiv real):

```yaml
  meter_drivers:
    - meter_id: "00013482"
      driver: auto  # sau munia
  xmq_sensors:
    - meter_id: "00013482"
      field: current_temperature_c
      sensor:
        name: "Munia temperatură"
        unit_of_measurement: "°C"
        accuracy_decimals: 1
    - meter_id: "00013482"
      field: current_relative_humidity_rh
      sensor:
        name: "Munia umiditate"
        unit_of_measurement: "%"
        accuracy_decimals: 1
    - meter_id: "00013482"
      field: error_flags_raw
      sensor:
        name: "Munia erori brute"
    - meter_id: "00013482"
      field: tpl_status_raw
      sensor:
        name: "Munia TPL brut"
  xmq_text_sensors:
    - meter_id: "00013482"
      field: status
      sensor:
        name: "Munia stare"
```

Generatorul verifică ambele MVT/ID-uri, toate cele trei chei DIF/VIF,
valorile BCD, valoarea de eroare, lungimea, padding-ul și întregul
JSON de referință din sursa XMQ. Testele native includ câmpurile
numerice și text, selectarea `auto`, configurații invalide,
capacitate insuficientă și stări cu erori nenule. Nu au fost
modificate partea radio, alte drivere sau firmware-ul instalat.
Acestea sunt teste software; nu probe RF live.

## Elvaco CMA12w — paritate completă pentru două exemple XMQ (2026-11-10)

Cele două telegrame publice originale `ELV/20/1B` și `ELV/1F/1B`
(`CI=7A`, 40 octeți fiecare incluzând octetul de lungime)
sunt decodificate exclusiv după CRC DLL verificat, ID configurat,
mode-zero și toate cele patru înregistrări obligatorii.

| Câmp | ID 66666666 | ID 68003902 |
| --- | ---: | ---: |
| `current_temperature_c` | 23.34 | 23.34 |
| `average_temperature_1h_c` | 23.28 | 23.37 |
| `battery` (text) | BATTERY_330 | BATTERY_4230 |
| `software_version` (text) | 4.0.0 | 3.0.5 |
| `status` (text) | OK | OK |
| `battery_raw` (numeric) | 816 (0x0330) | 16944 (0x4230) |
| `tpl_status_raw`, `tpl_status_unknown_raw` | 0 | 0 |
| Toate cele șase `status_*` standard | 0 | 0 |

Valoarea `battery_raw` **nu** reprezintă un procent sau o tensiune:
este doar numărul brut folosit de originalul XMQ pentru `BATTERY_%X`.
Câmpurile numerice suplimentare pot fi selectate în `xmq_sensors`:
`battery_raw`, `tpl_status_raw`, `tpl_status_unknown_raw`,
`status_alarm`, `status_busy`, `status_error`,
`status_permanent_error`, `status_power_low`,
`status_temporary_error`. Cele trei câmpuri text se configurează
în `xmq_text_sensors` cu `status`, `battery`, `software_version`.

Exemplu pentru un ID din fixture (înlocuiți ID-ul pentru un dispozitiv real):

```yaml
  meter_drivers:
    - meter_id: "66666666"
      driver: auto  # alternativ: cma12w
  xmq_sensors:
    - meter_id: "66666666"
      field: current_temperature_c
      sensor:
        name: "Elvaco temperatura"
        unit_of_measurement: "°C"
    - meter_id: "66666666"
      field: average_temperature_1h_c
      sensor:
        name: "Elvaco media 1h"
        unit_of_measurement: "°C"
    - meter_id: "66666666"
      field: battery_raw
      sensor:
        name: "Elvaco baterie cod brut"
    - meter_id: "66666666"
      field: status_power_low
      sensor:
        name: "Elvaco baterie scăzută (0/1)"
  xmq_text_sensors:
    - meter_id: "66666666"
      field: status
      sensor:
        name: "Elvaco stare"
    - meter_id: "66666666"
      field: battery
      sensor:
        name: "Elvaco baterie cod"
    - meter_id: "66666666"
      field: software_version
      sensor:
        name: "Elvaco versiune firmware"
```

Generatorul XMQ verifică strict ambele cadre și toate câmpurile din
JSON-ul original (inclusiv codul bateriei și versiunea inversată).
Testele native verifică toți cei 11 senzori numerici, cele trei
câmpuri text și refuzul telegramelor criptate, cu stare TPL
neconfirmată, cu ID greșit sau cu date invalide. Nu se creează
automat entități Home Assistant; niciun flash sau test RF real.

## Lansen Pulse LAS/14/00 — complete standard status diagnostics (2026-11-10)

The original unencrypted `00010206` fixture (`LAS/14/00`, `CI=7A`,
TPL status `0x04`) is already decoded as `a_counter=4711`,
`b_counter=1234`, text `status=POWER_LOW`. The configured native
decoder now also offers the four remaining standard Analyzer booleans
`status_alarm`, `status_busy`, `status_error`,
`status_temporary_error`, and `tpl_status_unknown_raw`, all zero
for the three admitted public Lansen Pulse fixtures. The existing
`status_power_low`, `status_permanent_error`,
`status_sabotage_enclosure`, `tpl_status_raw`, A/B counts and status
text are unchanged. TPL 0x40 is recognized as Lansen tamper, not
an unknown bit. Only XMQ-proven TPL values 0x00/0x04/0x4C are
accepted; unseen states and encrypted payloads still fail closed.

The five additional fields are **optional** `xmq_sensors` bindings
for explicitly configured IDs; no new Home Assistant entities are
created automatically. Tests cover all numeric and text fields
in all three upstream telegrams, including A-only records and
the existing invalid-status/security cases. The source length
field 0x23 denotes 35 octets **following** the length byte
(36 bytes total in the XMQ fixture). No physical RF test or
Heltec flash was performed.

## Lansen Door/Window `lansendw`: paritate Analyzer + diagnostice (2026-11-10)

Ambele telegrame originale LAS/07/1D (CI=7A, 47 octeți,
mode-zero și CRC verificat separat) sunt validate integral din
`lansendw.xmq`: starea ușii `status` (text `CLOSED` /
`OPEN`), `error_flags` (text cu indicatorii necunoscuți
păstrați literal), `a_counter` = 22 / 23, `b_counter` = 0.
Verificarea din generator respinge modificări ale intrărilor XMQ
sau ale mapărilor OPEN/CLOSED.

**Semnificație status:** în ambele telegrame bitul standard
`PERMANENT_ERROR` este activ; ceilalți cinci indicatori standard
din Analyzer sunt `false`. Înregistrarea de eroare conține
`0x0001` — `ERROR_FLAGS_1`, cu semnificație neidentificată.
TPL este `0x48` (72) pentru CLOSED și `0x68` (104) pentru OPEN.
Biții TPL neinterpretați sunt păstrați distinct drept
`TPL_STS_64` (0x40) și `TPL_STS_96` (0x60);
**nu** le atribuim sens comercial sau defecte inexistente.

Pe lângă cele două valori număr de impulsuri, proiectul oferă
**opțional** prin `xmq_sensors`:
- `digital_input_raw` = 17 / 85; `door_open` = 0 / 1,
  derive doar din codurile explicite `0x11` și `0x55`;
- `error_flags_raw` = 1 și `error_flags_unknown_raw` = 1;
- `tpl_status_raw` = 72 / 104;
  `tpl_status_unknown_raw` = 64 / 96;
- `status_permanent_error` = 1,
  `status_alarm` = 0, `status_busy` = 0,
  `status_error` = 0, `status_power_low` = 0,
  `status_temporary_error` = 0.

În `xmq_text_sensors` rămân `status` (`CLOSED`/`OPEN`)
și `error_flags`, cu întregul șir XMQ original:
`ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_64` sau
`ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_96`.

Exemplu — **ID-ul este numai cel fictiv din XMQ**, înlocuiți-l
cu ID-ul unui dispozitiv configurat:

```yaml
  xmq_sensors:
    - meter_id: "00010205"
      field: a_counter
      sensor:
        name: "Lansen deschideri sau închideri"
    - meter_id: "00010205"
      field: door_open
      sensor:
        name: "Lansen ușă deschisă (0/1)"
    - meter_id: "00010205"
      field: tpl_status_raw
      sensor:
        name: "Lansen stare TPL brută"
    - meter_id: "00010205"
      field: tpl_status_unknown_raw
      sensor:
        name: "Lansen biți TPL neinterpretați"
    - meter_id: "00010205"
      field: error_flags_unknown_raw
      sensor:
        name: "Lansen biți eroare necunoscuți"
    - meter_id: "00010205"
      field: status_permanent_error
      sensor:
        name: "Lansen eroare permanentă (0/1)"
  xmq_text_sensors:
    - meter_id: "00010205"
      field: status
      sensor:
        name: "Lansen stare ușă"
    - meter_id: "00010205"
      field: error_flags
      sensor:
        name: "Lansen diagnostic erori"
```

Nu se creează automat entități suplimentare. Restul driverelor,
inclusiv `vario451`, sunt neschimbate. Valorile provin din
două fixture-uri originale; alte configurații de status și
criptare sunt respinse pentru a nu publica diagnostice false.
Nu a fost făcut flash pe Heltec; testele CI nu substituie
verificarea RF pe hardware real.

## Lansen Pulse `lansenpu`: stări TPL detaliate și paritate XMQ (2026-11-10)

Cele trei exemple originale sunt verificate integral, inclusiv cele
două telegrame **LAS/0B/02** pe care Analyzer le decodifică astfel:
- `TPL=0x00`: `b_counter=870`, `status="OK"`, cele trei
  indicatoare de mai jos sunt 0.
- `TPL=0x4C`: `b_counter=870`,
  `status="PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE"`,
  toate cele trei indicatoare de mai jos sunt 1.
- `TPL=0x04` (modelul public LAS/14/00):
  `a_counter=4711`, `b_counter=1234`,
  `status="POWER_LOW"`.

În plus față de `a_counter` și `b_counter` (senzori numerici) și
`status` (senzor text existent), sunt acum disponibili **opțional** în
`xmq_sensors` patru senzori numerici suplimentari:
- `tpl_status_raw` = 0 / 4 / 76 (valoarea brută a octetului);
- `status_power_low` = 0/1 (bitul 2, `0x04`);
- `status_permanent_error` = 0/1 (bitul 3, `0x08`);
- `status_sabotage_enclosure` = 0/1 (bitul proprietar LAS `0x40`).

Bitul LAS de sabotaj este verificat explicit în definiția XMQ
(`mfct_tpl_status_bits`, `mask_bits=0xE0`, valoare `0x40`).
Nu s-au inventat semnificații pentru alți biți: numai
stările `0x00`, `0x04` și `0x4C`, confirmate prin cele
trei fixture-uri, sunt acceptate. Restul sunt respinse pentru a
evita publicarea unei stări `OK` false. Indicatorii numerici
se publică doar după verificările existente DLL CRC, ID configurat,
mode-zero și DIF/VIF/BCD.

Exemplu de configurare, folosind ID-ul **fictiv** din sursa XMQ:

```yaml
  xmq_sensors:
    - meter_id: "00023750"
      field: b_counter
      sensor:
        name: "Lansen impulsuri B"
    - meter_id: "00023750"
      field: tpl_status_raw
      sensor:
        name: "Lansen stare TPL brută"
    - meter_id: "00023750"
      field: status_power_low
      sensor:
        name: "Lansen baterie slabă (0/1)"
    - meter_id: "00023750"
      field: status_permanent_error
      sensor:
        name: "Lansen eroare permanentă (0/1)"
    - meter_id: "00023750"
      field: status_sabotage_enclosure
      sensor:
        name: "Lansen carcasa deschisă (0/1)"
  xmq_text_sensors:
    - meter_id: "00023750"
      field: status
      sensor:
        name: "Lansen stare"
```

Nu se creează automat senzori noi și nu se modifică
configurația `vario451`. Valorile `structured.status_flags`
din Analyzer provin din interpretarea statusului, nu din
înregistrări DIF/VIF suplimentare. Nu este test RF live și
nu s-a făcut flash pe Heltec.

## Vario451: diagnostic complet al payloadului CI=A2 (2026-11-10)

Pe lângă paritatea 3/3 pentru rezultatele XMQ `previous_kwh`,
`current_kwh`, `total_kwh`, decodorul expune **opțional** și
cele două contoare brute (marcate `HIDE` în XMQ-ul original):
`prev_raw_counter=13112` și `curr_raw_counter=9826`
(în unitățile originale mGJ). Conversia rămâne XMQ
`previous=prev/3.6`, `current=curr/3.6`,
`total=(prev+curr)/3.6`.

Se poate configura și textul `mfct_payload_hex`: întreaga zonă
manufacturer-specific CI=A2 de **45 octeți**, păstrată ca 90 de
caractere HEX, fără interpretarea cozii proprietare Techem.
Câmpul este diagnostic brut, **nu** un senzor de energie;
niciun `status` sau alt câmp necunoscut nu este inventat.

```yaml
  xmq_sensors:
    - meter_id: "58234965"  # ID public fictiv din fixture
      field: prev_raw_counter
      sensor:
        name: "Vario451 contor brut anterior"
    - meter_id: "58234965"
      field: curr_raw_counter
      sensor:
        name: "Vario451 contor brut curent"
  xmq_text_sensors:
    - meter_id: "58234965"
      field: mfct_payload_hex
      sensor:
        name: "Vario451 payload CI-A2 HEX"
```

Aceste entități sunt create doar dacă sunt cerute în YAML. Nu
se schimbă `driver: auto` sau driverul explicit. Este admis
exclusiv formatul original TCH/27/C3, CI=A2, 56 octeți, cu
identificator configurat, validarea structurii și controlul
de publicare deja existent. Nu se introduce o cale generică
pentru CI=A2 sau pentru modelul neverificat TCH/27/04.
Testul pe telegrama publică verifică valorile brute, cele trei
energii, egalitatea exactă a payloadului HEX și respingerile
negative. Nu s-a făcut flash și nu s-a validat RF live.

## Supercal 739 + LSE/01/08: toate câmpurile XMQ și diagnostice (2026-11-10)

**Branch:** `experimental/xmq-multidriver-auto`; generatorul
`experimental/xmq/tools/generate_supercal_lse08.py` validează integral
cele două exemple originale XMQ, semnăturile MVT și toate
înregistrările DIF/VIF ale telegramelor. Produce
`xmq_generated_supercal_lse08.h` pentru ESPHome și testele native.
Verificarea CRC DLL și a ID-ului configurat rămâne obligatorie;
nu se acceptă automat profile criptate sau necunoscute.

**`supercal` (SON/1B/04, CI=7A, 69 octeți, 1/1 fixture):**
- `xmq_sensors`: `total_kwh=296`, `target_kwh=0`,
  `total_m3=44.26`, `target_m3=0`,
  `flow_temperature_c=40.67`, `return_temperature_c=31.14`,
  `volume_flow_m3h=0.285`, `power_kw=3.14`.
- `xmq_text_sensors`: `meter_datetime="2023-03-04 18:54"`,
  `target_date="2127-01-01"`, plus `status="OK"` din TPL 0.
  Data neobișnuită **2127** se păstrează exact, fără corectări
  speculative. Toate cele zece înregistrări CI=7A sunt decodate;
  registrele cu `storage_nr=20` sunt ținte, nu totaluri curente.

**`lse_08` (LSE/01/08, CI=7A, 42 octeți, 1/1 fixture):**
- `xmq_sensors`: `consumption_at_set_date_hca=321`,
  `duration_since_readout_h=2.4891667` (Analyzer rotunjește
  la 2.489167), `duration_since_readout_s=8961`,
  `tpl_status_raw=128`. Durata originală este 0x2301 secunde,
  convertită la ore /3600; nu este o durată în secunde inventată.
- `xmq_text_sensors`: `set_date="2003-01-31"`,
  `device_datetime="2003-02-15 14:26"`,
  `model_version="01"`, `status="TPL_STS_128"`.
  **Nu se publică OK** pentru codul TPL 128 și nu se deduce o
  semnificație de alarmă pentru biții necunoscuți. Nu există
  `current_consumption_hca` în această telegramă, deci nu se
  sintetizează un astfel de senzor din valoarea istorică.

Exemplu minimal al câmpurilor nou adăugate, cu ID-urile **fictive**
din fixture-urile originale (înlocuiți cu ID-urile proprii):

```yaml
  meter_drivers:
    - meter_id: "89508019"
      driver: supercal  # sau auto, exclusiv pentru ID configurat
    - meter_id: "04998541"
      driver: lse_08    # sau auto, exclusiv pentru ID configurat
  xmq_sensors:
    - meter_id: "89508019"
      field: target_kwh
      sensor:
        name: "Supercal energie la data țintă"
        unit_of_measurement: "kWh"
    - meter_id: "89508019"
      field: target_m3
      sensor:
        name: "Supercal volum la data țintă"
        unit_of_measurement: "m³"
    - meter_id: "04998541"
      field: duration_since_readout_h
      sensor:
        name: "LSE durata de la citire"
        unit_of_measurement: "h"
    - meter_id: "04998541"
      field: tpl_status_raw
      sensor:
        name: "LSE stare TPL brută"
  xmq_text_sensors:
    - meter_id: "89508019"
      field: target_date
      sensor:
        name: "Supercal data țintă"
    - meter_id: "89508019"
      field: meter_datetime
      sensor:
        name: "Supercal data și ora contorului"
    - meter_id: "04998541"
      field: set_date
      sensor:
        name: "LSE data bilanțului"
    - meter_id: "04998541"
      field: device_datetime
      sensor:
        name: "LSE ora dispozitivului"
    - meter_id: "04998541"
      field: model_version
      sensor:
        name: "LSE versiune model"
    - meter_id: "04998541"
      field: status
      sensor:
        name: "LSE stare"
```

Câmpurile de metadate `media`, `name`, `id` provin din antet
și configurație, iar `timestamp` din Analyzer este ora procesării,
nu un registru al contorului; Home Assistant folosește ora recepției.
Componentele de senzori se declară explicit în YAML, nu sunt
auto-create. Testele native acoperă toate valorile JSON originale,
diagnosticele brute și mutații negative; compilarea nu dovedește
recepție RF pe hardware. Niciun flash pe Heltec.

## Sontex868 + Maddalena: toate câmpurile verificabile din fixture-urile XMQ (2026-11-10)

**Ramura:** `experimental/xmq-multidriver-auto`. Generare și controale:
`experimental/xmq/tools/generate_clear_profiles.py` compilează din
`sontex868.xmq` și `maddalena.xmq` semnăturile MVT, cheile DIF/VIF,
pozițiile înregistrărilor și verificarea completă a câmpurilor JSON C!.
Tabelele sunt consumate de decodoarele native C++, fără vechiul runtime.

- **`sontex868`**, SON/16/08, 1/1 fixture, 176 octeți, CI=7A:
  câmpurile XMQ `current_consumption_hca`=119,
  `consumption_at_set_date_hca`=102, `current_temp_c`=27.33,
  `current_room_temp_c`=12.4, `max_temp_c`=27.33,
  `max_temp_previous_period_c`=0; `set_date`=2127-07-01,
  `device_datetime`=2020-10-31 10:04. Extras standard:
  `storage_48_consumption_hca`=0, `storage_48_date`=2019-05-01,
  `tariff_1_duration_minutes`=0, `tariff_1_date`=2000-01-01,
  `tariff_1_cumulation_counter`=0,
  `tariff_2_date`=2020-10-31,
  `software_version`=010301, `parameter_activation_state_raw`=2220.
  Vendor, **fără unități/sens inventate**:
  `manufacturer_ff2d_value`=1,
  `manufacturer_ff2d_tariff_2_value`=1,
  `manufacturer_ff2c_raw`=0.
  Înregistrarea proprietară CompactProfileWithRegister este
  `compact_profile_hex` (106 caractere HEX / 53 octeți), **nu** o
  istorie de consum interpretată. `status`=OK numai pentru TPL zero.

- **`maddalena`**, MAD/01/07, 1/1 fixture, 79 octeți, CI=7A:
  `total_m3`=0.167, `target_m3`=0,
  `target_date`=2000-01-01,
  `meter_datetime`=2024-11-22 12:14,
  `fabrication_no`=`FF0124018699`,
  `status`=OK. Extras standard:
  `storage_2_volume_m3`=0.074,
  `storage_2_date`=2024-10-31,
  `storage_3_max_flow_m3h`=1.351,
  `storage_3_datetime`=2024-11-21 20:27.
  `FF` final din numărul de fabricație este santinelă vendor,
  **nu** BCD numeric. Detectarea MAD/01/06 din metadatele XMQ
  **nu** autorizează decodarea RF fără fixture.

Toate câmpurile numerice sunt accesibile prin `xmq_sensors`,
iar datele, statusul, versiunea, numărul de fabricație și profilul
compact sunt disponibile prin `xmq_text_sensors`. Entitățile HA
trebuie configurate explicit; nu sunt create implicit pentru orice contor.
Data `timestamp` din fixture-urile XMQ este artificială; Home Assistant
folosește momentul recepției, nu un timestamp inventat din payload.
Datele `C?` sunt în mod deliberat numite după storage/tariff/VIF,
nu după un sens comercial nedemonstrat. Pentru telegrame criptate,
câmpuri invalide, statusuri de alarmă necunoscute sau layout
neconfirmat, **nu se publică date** (fail-closed).

Exemplu de configurare, cu ID-urile XMQ **fictive** și cu restul
configurației SX126x păstrate:

```yaml
  meter_drivers:
    - meter_id: "27282728"
      driver: sontex868
    - meter_id: "24018699"
      driver: maddalena
  xmq_sensors:
    - meter_id: "27282728"
      field: current_consumption_hca
      sensor:
        name: "Sontex HCA curent"
    - meter_id: "27282728"
      field: storage_48_consumption_hca
      sensor:
        name: "Sontex HCA registru 48"
    - meter_id: "27282728"
      field: manufacturer_ff2d_value
      sensor:
        name: "Sontex valoare vendor FF2D (unitate necunoscută)"
    - meter_id: "24018699"
      field: total_m3
      sensor:
        name: "Maddalena volum total"
        unit_of_measurement: "m³"
    - meter_id: "24018699"
      field: storage_2_volume_m3
      sensor:
        name: "Maddalena volum registru 2"
        unit_of_measurement: "m³"
    - meter_id: "24018699"
      field: storage_3_max_flow_m3h
      sensor:
        name: "Maddalena debit maxim registru 3"
        unit_of_measurement: "m³/h"
  xmq_text_sensors:
    - meter_id: "27282728"
      field: set_date
      sensor:
        name: "Sontex data bilanț"
    - meter_id: "27282728"
      field: device_datetime
      sensor:
        name: "Sontex ora contorului"
    - meter_id: "27282728"
      field: software_version
      sensor:
        name: "Sontex versiune software"
    - meter_id: "27282728"
      field: compact_profile_hex
      sensor:
        name: "Sontex profil compact brut"
    - meter_id: "24018699"
      field: target_date
      sensor:
        name: "Maddalena data de referință"
    - meter_id: "24018699"
      field: fabrication_no
      sensor:
        name: "Maddalena număr fabricație"
    - meter_id: "24018699"
      field: meter_datetime
      sensor:
        name: "Maddalena ora contorului"
```

Testele native compară toate câmpurile XMQ și toate valorile suplimentare
de mai sus, inclusiv probe negative de layout, BCD, calendar, ID și criptare.
RF pe hardware real nu a fost testat; nu s-a făcut flash Heltec.

## Supercom587: generare și paritate pentru toate câmpurile XMQ (2026-11-10)

- Driverul `supercom587` utilizează acum modele și reguli DIF/VIF
  generate din `supercom587.xmq` în
  `xmq_generated_clear_profiles.h`, inclusiv offseturile determinate
  din **ambele exemple originale**. În firmware se validează modelul
  SON/3C/06 (apă caldă) și SON/3C/07 (apă rece), CI=7A, profilul
  necriptat, ID-ul configurat și forma de 163 octeți.
- Datele din JSON-ul oficial sunt reproduse integral pentru măsurare
  și diagnostic:
  `total_m3` = 5.548 / 4.989,
  `software_version` = `010002` pentru ambele,
  `status` = `OK` pentru ambele.
  `software_version` păstrează zerourile inițiale din cele 3 octeți
  BCD și este publicabil prin `xmq_text_sensors`; `status` este
  emis doar pentru indicatori de eroare 0. Indicatori necunoscuți,
  BCD invalid, înregistrări lipsă sau antet criptat sunt respinși.
- `name` este eticheta configurată în ESPHome, `id` este citit
  din antet și corelat cu configurația, iar `media` corespunde
  codului de aparat 06/07. `timestamp=1111-11-11...` din
  fixture este **un placeholder al testului**, nu timpul emis
  de contor: în Home Assistant se folosește timpul real al citirii.
- Exemplu pentru un contor real, după înlocuirea ID-ului fictiv
  `12345678` cu identificatorul lui și configurarea
  secțiunilor existente `wmbus_native_sx126x`:

```yaml
  meter_drivers:
    - meter_id: "12345678"  # exemplu public XMQ, nu un ID de productie
      driver: supercom587   # sau auto, cu verificare MVT/ID
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Supercom 587 consum total"
        unit_of_measurement: "m³"
        accuracy_decimals: 3
  xmq_text_sensors:
    - meter_id: "12345678"
      field: software_version
      sensor:
        name: "Supercom 587 versiune software"
    - meter_id: "12345678"
      field: status
      sensor:
        name: "Supercom 587 stare"
```

- Testele verifică toate cele două exemple și toate câmpurile JSON
  măsurabile, plus exemple negative de corupere sau modificare XMQ.
  Decodoarele celorlate drivere și firmware-ul instalat nu sunt modificate.
  O compilare CI verde nu este echivalentă cu o probă RF reală.

## Vario451: generare din IXML-ul XMQ original (2026-11-10)

- `experimental/xmq/tools/generate_vario451_ixml.py` generează
  `xmq_generated_vario451.h`, consumat direct de
  `components/wmbus_native_sx126x/vario451_plain.h`.
- Nibble-grammar IXML produce offseturile radio-relative
  `DV_PREV=3` și `DV_CURR=7`; `calculate` validează coeficientul
  comun `3.6` pentru `total_kwh`, `current_kwh` și
  `previous_kwh`. Fixture-ul original verifică 6371.666667,
  2729.444444 și respectiv 3642.222222 kWh (1/1).
- Transformarea este limitată și fail-closed: modificări nevalidate
  ale gramaticii, formulelor, mapării DIF/VIF, modelului MVT sau
  telegramelor de exemplu provoacă erori de generare. Nu s-a introdus
  un interpret XMQ general pe ESP32-S3.
- Firmware-ul rămâne limitat la `TCH/27/C3`, CI=A2, telegrama
  originală de 56 octeți; `TCH/27/04` este doar metadată de detectare,
  nepublicabil fără fixture radio. Se păstrează verificarea DLL CRC,
  identificatorul configurat, controalele de criptare și validarea
  structurală existentă.
- Numărul celor 52 de drivere și decodoarele celorlalte modele nu s-au
  schimbat. Niciun flash pe Heltec. CI #816 a încheiat cu trei joburi
  verzi; #817 adaugă teste negative de fail-closed.
  https://github.com/radugeo74/esphome-sx1262/actions/runs/42775764071

## Native XMQ: primele 4 profile declarative generate (2026-11-10)

- Branch: `experimental/xmq-multidriver-auto`. Generator nou:
  `experimental/xmq/tools/generate_clear_profiles.py`.
  Drivere: `munia` (2 exemple), `cma12w` (2), `lansendw` (2)
  și `lansenpu` (3). Toate cele **9 exemple originale XMQ** au
  regresii native pentru câmpurile numerice și text de referință.
- Generatorul parsează XMQ-urile originale, validează semnăturile
  `detect.mvt`, selectorii `match` și prezența prefixelor de
  înregistrări în fixture-urile complete. Generează tabelele C++
  `xmq_generated_clear_profiles.h` cu modele și chei DIF/VIF.
  `build_native_generated.py` și codegen-ul ESPHome le regenerează.
- Cele patru decodoare native folosesc la runtime tabelele generate.
  Restul decodării (BCD, scalare, software_version, stări) și verificările
  stricte de ID, CRC, profil, status și criptare **rămân implementate
  și validate în C++**. Nu este încă un compilator general XMQ și
  nu s-au extins implicit profilele RF acceptate.
- CI #814: `native-packet-input`, `stage-a-espidf-s3-portable-link`
  și `native-esphome-build` toate SUCCESS; Heltec T1/C1 build-only.
  https://github.com/radugeo74/esphome-sx1262/actions/runs/42772697739
- Totalul celor 52 de drivere XMQ rămâne neschimbat. Nicio restaurare a
  vechiului wmbus_radio/meter stack, niciun flash și nicio confirmare RF
  pe dispozitive reale. Următorii candidați pentru generare se aleg după
  echivalență completă a exemplelor și a câmpurilor JSON.

## Completare dependențe portabile Heltec (2026-11-09)

- The failed experimental Heltec build after the six-header allowlist exposed
  `apator162_ixml_fields.h` as a seventh real native runtime dependency.
  It is now explicitly bundled, while offline `xmq_decode_catalog.h`
  and `xmq_field_parity.h` remain excluded.
- CI verifies native local C++ include references against the allowlist, so
  future missing portable dependencies fail the host guard immediately.
  No legacy runtime component or C++ driver was restored.

## Corecție build Heltec după curățenie (2026-11-09)

- Root cause #808: the native ESPHome `__init__.py` used a wildcard
  `portable_cpp/*.h` copy, which overwrote the actual native
  `xmq_decode_catalog.h` with the offline parity catalog moved during cleanup.
  Its `../legacy_reference/` includes then failed in the copied ESPHome
  component directory during the Heltec ESP-IDF build.
- Fixed with an explicit allowlist of **seven runtime portable headers**.
  Existing native-owned headers cannot be overwritten; a conflicting
  duplicate is rejected. `frame_bridge.h` is already owned by native.
- Added an offline CI invariant verifying the precise allowlist and
  preventing wildcard copying. No legacy wM-Bus components or drivers
  have been reinstated, and the 52 native XMQ drivers are unchanged.
- Earlier workflow #808 passed native host tests and ESP-IDF link, but failed
  the Heltec compile. The new workflow must complete before the cleanup
  can be called fully green.

## Audit și curățenie native-only (2026-11-09)

- Removed **108 obsolete files** from this branch: entire old
  `wmbus_common`, `wmbus_meter`, `wmbus_radio`, `socket_transmitter`
  components, stale legacy ESPHome and shadow fixtures, unneeded libXMQ C++
  feasibility probe, linker test and obsolete XMQ-only workflow.
- Preserved **15 portable `pure_*.h` reference headers** under
  `experimental/xmq/legacy_reference/` solely for offline host regression.
  Moved the old XMQ comparison catalog/parity headers to
  `experimental/xmq/portable_cpp/`; updated source tests and native CI.
- Native firmware is built only from `sx126x`,
  `wmbus_native_sx126x`, and 52 XMQ driver sources and supporting code.
  Configured meter admission, AES isolation and `driver: auto` behavior
  are unchanged. No real meter IDs, keys or deployed devices were touched.
- **Breaking branch cleanup:** obsolete `wmbus_radio`/`wmbus_meter`
  YAML configurations are intentionally not supported here. Use Git
  history or an older branch for the legacy stack. New CI guards reject
  legacy source tree regression and ensure portable parity fixtures remain
  compilable. CI is not a real-RF substitute.

## Curățenie: retragerea driverelor C++ vechi (2026-11-09)

- Deleted all **91** `components/wmbus_common/driver_*.cc` translation units
  from `experimental/xmq-multidriver-auto`. Files remain recoverable in Git
  history; the common legacy `meters.cc`, `dvparser.cc`, `aes.cc` and other
  shared sources are deliberately unchanged.
- Native `wmbus_native_sx126x` uses the independent XMQ metadata, native C++
  parsers and configured meter binding. Its **52 native driver families**,
  sensor mappings and `driver: auto` behavior are unchanged.
- **Compatibility break (intentional):** old `wmbus_meter` /
  `wmbus_radio` configurations that use legacy C++ drivers are not supported
  on this branch anymore. Use an earlier Git revision for the old pipeline.
  This cleanup does not itself change any flashed Heltec/HA device.
- A native-only CI guard rejects reintroduction of `driver_*.cc` and legacy
  components in the native Heltec build fixtures. CI success is a software
  check, not new real-radio verification.

## Calupul 51–52: Techem Compact5 + Vario451 (2026-11-09)

The `experimental/xmq-multidriver-auto` branch contains **52 source-anchored
native XMQ driver families**. Both additions require CRC-verified and
CRC-stripped SX126x frames, configured decimal DLL ID, exact MVT and CI:

- **`compact5`**: only two original TCH/39/43 CI A2 56-byte examples
  with `total_kwh=25250/58018`, `current_kwh=284/7567` and
  `previous_kwh=24966/50451`. The other two original TCH/45/43
  **CI A1** examples are NOT supported or used for publication; no
  relaxation of the generic publication gate.
- **`vario451`**: one genuine TCH/27/C3 CI A2 56-byte example;
  two 16-bit counters converted from mGJ to kWh by /3.6 yield
  `total_kwh=6371.666667`, `current_kwh=2729.444444`,
  `previous_kwh=3642.222222`.
- Both support configured `driver: auto` and explicit driver name,
  but no unconfigured publication, AES guessing, or proprietary
  status/alarm inference. All original fixtures and deliberate
  wrong-ID/layout/model negatives have a host CI test. Build-only
  Heltec YAML includes only public upstream example IDs.
- No deployed firmware, private meters, Home Assistant/OTA settings or
  security keys have been changed. Software CI is NOT hardware RF validation.

## Calupul 49–50: Apator E-ITN + Gran Systems (2026-11-09)

The `experimental/xmq-multidriver-auto` branch now contains **50
integrated native XMQ driver families**. Both additions are
source-anchored and require configured DLL meter identity and strict MVT:

- **`apatoreitn`** (distinct from `apatoreitn40`): three original
  radio samples, APA/04/08 `CI=A0` and APT/04/08 `CI=B6`.
  Source-bounded application sizes **26/38/42 bytes** validate
  `current_hca` and `previous_hca` plus two Q8.8 degree-C
  mean ambient temperature readings,
  `temp_room_avg_c` and `temp_room_prev_avg_c`.
  Optional text `status` may be **SEAL_BROKEN** or **OK** only
  after the original date/seal structure validates. Manufacturer
  `B6` uses a count/opaque-prefix followed by a nested `A0`
  record, admitted solely for the two observed **0A/0E** forms.
  The original frames use the documented M-field bit15-set aliases
  **0x8601** (APA) / **0x8614** (APT); configured AUTO maps only
  these exact profiles to upstream 0x0601/0x0614 signatures.
  This does NOT extend the general TPL publication/security gate
  to arbitrary CI B6 or claim encryption support. Unknown variants
  and altered IDs/records are rejected.
- **`gransystems`** GSS/01/02: two genuine **CI78** frames without
  a TPL header, **121/159 bytes**. Only unambiguous source-proven
  `total_energy_consumption_kwh` (**0.916 / 0**),
  `total_energy_consumption_tariff_1_kwh` (**0.873 / 0**)
  and `total_energy_consumption_tariff_2_kwh` (**0.043 / 0**)
  are published. `CI78` is permitted ONLY inside this model
  parser, NOT generically by the OMS publication gate.
  The three-phase sample reports **RTC_ERROR**; no invented
  `status: OK` or guessed vendor alarms are published.
- Both require independently validated and CRC-stripped SX126x
  radio frames, configured matching meter ID and strict model,
  CI, length and field offsets. Explicit YAML and `driver: auto`
  remain configured-only. No secret keys, no generic proprietary
  decode fallback, no publication from metadata alone.
- Build-only Heltec YAML carries public fixture IDs and mapped
  sample sensors. No changes to deployed Heltec firmware, user
  device keys, live meter configuration, or OTA/flash.
  RF validation remains a separate step from software CI.

## Calupul 47–48: Apator ELF + Engelmann FAW (2026-11-09)

The branch `experimental/xmq-multidriver-auto` now integrates
**48 native XMQ driver families**, including the first two
conservatively authenticated **dual-address, clear CI72 long-TPL**
profiles. Unlike the existing publication gate, this special
route requires TWO separately configured identities and independently
checks both on every CRC-verified/stripped RF packet:

- **`elf`** (APA/40/04): one original 82-byte clear Mode0 example
  has **DLL sender 00050901** (M-field 0x8601, V0x18, T0x37)
  and **TPL meter 01885619** (APA/40/04). Only validated
  `total_energy_consumption_kwh=3112.49977`,
  `total_volume_m3=201.364`,
  `total_energy_consumption_at_date_kwh=3047.8`,
  `flow_temperature_c=69` and `return_temperature_c=58`
  are published. The source reports `ERROR_FLAGS_2000000`;
  no `status: OK` or unverified alarm interpretation is published.
- **`engelmann_faw`** (EFE/00/07): one original 144-byte
  clear Mode0 fixture uses **DLL sender 43000255** (EFE/01/07)
  and **TPL meter 20254060** (EFE/00/07). It validates
  `consumption_at_reporting_date_m3=59.682`,
  `consumption_1_months_ago_m3=78.908`, and
  `consumption_2_months_ago_m3=77.23`.
  The source contains **no current `total_m3`**;
  all 15 monthly history slots are checked structurally, but
  only two published. The other upstream fixture is encrypted
  CI73 with a key and remains **unsupported**; no key bypass.
- YAML `meter_drivers` now supports an explicit
  `dll_meter_id` for these two named drivers and configured
  `auto`, with `meter_id` referring to the target TPL ID
  and all sensors binding the **TPL meter ID**. The existing
  single-address plaintext admission remains unchanged, as does
  its rejection of a converter/TPL mismatch. New driver-specific
  two-ID gate checks the original sender M/V/T, TPL M/V/T,
  both IDs, clear security Mode0 and strict original packet layout.
  AES, ELL, unknown models and wrong IDs fail closed.
- Heltec YAML remains **build-only**, with ONLY public upstream
  fixture IDs and no live user meter IDs, secrets, OTA or flash.
  Hardware RF verification remains separate from CI.

## Calupul 45–46: Ei6500 diagnostics + Apator E-ITN40 (2026-11-09)

The `experimental/xmq-multidriver-auto` branch now contains
**46 native XMQ driver families** with configured-only publication.

- **`ei6500`**: six public EIE/0C/1A Mode-0 short-TPL 89-byte
  fixtures confirm `alarm_counter` (historic count),
  `removed_counter`, `test_button_counter` and
  `dust_level_counter`. **This is diagnostic telemetry, NOT
  smoke/fire detection**. An upstream `status: OK` can coexist
  with an earlier alarm: therefore the native subset publishes
  **no live smoke alarm state and no `status: OK`**. The actual
  certified smoke alarm device must work independently; these
  Home Assistant fields must not be used for fire protection.
- **`apatoreitn40`**: three official Apator Metra E-ITN40
  CI `A0` manufacturer-specific, 65-byte unencrypted fixtures
  (`APA/09/08` and `APA/0D/08`) support
  `current_hca`, `previous_hca`, `previous_2y_hca`,
  `previous_3y_hca`, `current_month_hca`,
  `previous_month_hca`, `temp_room_c` and
  `temp_radiator_c`. The proprietary 27 little-endian
  16-bit words are decoded at exact source-proven offsets,
  not fed to the generic DIF/VIF parser. Historical monthly
  archive and calendar text are not published.
  `APA/0A/08` and `APA/0E/08` are **AES variants**
  and this cleartext decoder rejects them. Upstream additional
  `APT` detection signatures are not evidence of tested
  support for that on-air frame layout.
- Both require independently CRC-verified/CRC-stripped SX126x
  input, matching configured DLL ID, strict model/version and
  bounded source format. `driver: auto` and explicit selection
  never create unconfigured entities or bypass encryption.
- The Heltec lab YAML is **build-only** with public original
  fixture IDs, no live meter IDs, no keys, **no OTA/flash**;
  real RF validation remains separate.

## Calupul 43–44: Maddalena Microclima + ABB B23 (2026-11-09)

The `experimental/xmq-multidriver-auto` branch now integrates **44
native driver families**. Both additions have source-anchored,
configured-only cleartext decoding and negative transport tests:

- **`microclima`**: two original MAD/00/04 short-TPL Mode-0 examples,
  74 and 165 bytes. Both validate `total_energy_consumption_kwh`
  (1805/2119) and `total_volume_m3` (297.994/241.757 m³);
  the first additionally validates optional `flow_temperature_c`
  and `return_temperature_c` (21°C each). Manufacturer-specific
  parameter set, calendar and 16 prior billing months are **not**
  decoded in this native subset. Unsupported source revisions,
  wrong IDs, malformed BCD and non-Mode-0 telegrams fail closed.
- **`abbb23`**: one original ABB/20/02 133-byte short-TPL
  Mode-0 example proves `total_energy_consumption_kwh=5249.17`
  from six-byte BCD DIF `0E`, VIF `84 00`. Strict full-profile
  markers and bounded record parser are checked before publication.
  Import/export tariffs, manufacturer alarm flags and other power
  readings are **not** claimed without source evidence.
- The official SX126x native adapter still requires verified DLL
  CRC removal, exact configured meter ID, unambiguous MVT,
  verified TPL Mode-0 security and source-compatible records.
  `driver: auto` or explicit driver does not decrypt or guess.
  NOKEY is upstream fixture metadata, not an RF encryption bypass.
- `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml`
  remains **build-only** with public original fixture identifiers,
  no production credentials or private meter IDs, **no OTA/flash**.
  Real RF validation remains separate.

## Calupul 41–42: QHeat V2 + Zenner C5ISF (2026-11-09)

The `experimental/xmq-multidriver-auto` native branch contains
**42 integrated XMQ driver families**. These new profiles have
source-anchored, configured-only Mode-0 publication:

- **`qheatv2`**: only the original **LSE/23/04**, short-TPL
  47-byte upstream clear fixture is admitted. Its standard BCD
  `total_kwh=12560.3` and `target_year_kwh=11132` are
  published. The original DLL M-field is **0xB265** (LSE bit15
  set), whereas the upstream `detect.mvt` uses LSE/23/04
  (0x3265): AUTO has a single source-bounded alias, never a
  global manufacturer masking rule. Other upstream CI72/CI78
  and AES/WalkBy examples are not accepted by this decoder.
  The one supported zero-status source may publish optional
  `status: OK`; no unsupported error-bit interpretation.
- **`c5isf`**: the original **ZRI/88/0D** 230-byte Mode-0
  source fixture publishes `total_energy_consumption_kwh=0`,
  `total_volume_m3=0` and
  `total_energy_consumption_last_month_kwh=0`. This exact
  sample is an **error-state meter**, not healthy:
  `status: DRY ERROR RETURN_SENSOR_INTERRUPTED`, verified
  only for the original TPL/status flag combination. Historic
  `-2147483648` energy sentinels and `0xFFFF` date placeholders
  are excluded. Both other `NOKEY` examples have
  **TPL AES Mode-5 security headers**, and are rejected as
  unverified plaintext despite source-supplied readable payloads.
- The official SX126x native adapter must provide independently
  CRC-verified and CRC-stripped RF, matching configured DLL ID,
  strict MVT/alias, genuine Mode-0 TPL and exact validated
  application records. `driver: auto` and explicit YAML
  selection still fail closed on unsupported variants.
- Updated README, PROJECT_CONTEXT, provenance catalog and CI;
  lab Heltec YAML is **build-only** with public upstream fixture
  IDs. No changes to real Techem meters, credentials, radio
  hardware, OTA or deployed firmware. Hardware RF validation is
  separate from software CI.

## Calupul 39–40: Itron Heat + EMH eHZP (2026-11-09)

Native branch `experimental/xmq-multidriver-auto` now contains **40
integrated XMQ driver families** using configured-only, fail-closed
radio publication.

- **`itronheat`** (ITW/00/04): one genuine clear short-TPL
  Mode-0 65-byte upstream fixture validates
  `total_kwh=19529`, `total_m3=8401.224`, and
  `last_year_kwh=13978`. Its M-field uses the observed
  `0xA697` (ITW with bit15 set), and configured AUTO permits
  only this exact documented variant rather than globally
  changing manufacturer matching. A second public `NOKEY` fixture is
  **AES Mode-5 on air** and already carries supplied-clear
  application bytes; the RF decoder refuses it. Only verified
  04/06, 04/13, 44/06 standard DIF/VIF readings are exposed.
  Opaque manufacturer bytes and unknown status combinations
  remain unparsed.
- **`ehzp`** (EMH/00/02): one original genuine clear
  CI `7A` Mode-0 175-byte upstream sample validates
  `total_energy_consumption_kwh=95.7722`,
  `total_energy_production_kwh=0.0021`, and
  `current_power_consumption_kw=0.09`. The native subset reads
  the three independently framed standard numeric DIF/VIF
  records and strictly checks fixed profile markers;
  it does not guess proprietary historical/tariff records.
  Another `NOKEY` upstream fixture uses **ELL CI `8C`**
  with an already-clear supplied application and is NOT
  accepted as Mode-0 RF. The EMH/02/02 signature is
  recognizable but not admitted to this source-tested profile.
- Both profiles are `PORTABLE`, support configured
  `driver: auto` and explicit YAML drivers, and revalidate
  the configured DLL ID, MVT, CRC-stripped verified input,
  TPL Mode-0 status and bounded application records before
  any sensor value is published. Passive recognition is
  never treated as permission to decrypt.
- Heltec sample YAML remains **build-only** and uses public
  upstream fixture meter IDs, not live private IDs or keys.
  No OTA/flash or changes to live Techem meters.
  RF validation on hardware remains a separate step.

## Calupul 37–38: QHeat 5.5 US + Maddalena (2026-11-09)

Native configured-only `PORTABLE` support now covers **38 XMQ
driver families** on `experimental/xmq-multidriver-auto`.

- **`qheat_55_us`** (LUG/07-0A/04): 4 official upstream TPL Mode-0
  fixtures establish `total_energy_consumption_kwh` and
  `key_datetime_kwh` (billing energy), e.g. 9205/5182,
  60800/60691, 13706/10268 and 0/0 kWh. Source-backed `total_m3`
  is published **only when a standard volume record exists**.
  The 13706 kWh source fixture explicitly reports **F0_FLOW_ERROR**:
  this native numeric-only subset does not guess an "OK" status,
  interpret unverified manufacturer alarm flags or assert parity for
  all monthly readings.
- **`maddalena`** (MAD/01/07): one original 79-byte Mode-0 frame
  establishes `total_m3=0.167` and `target_m3=0`.
  Its fabrication number record `0E78` ends in `FF` sentinel,
  which is NOT valid numeric BCD. The strict per-profile decoder
  checks the full fixed layout, then validates the **two individual**
  standard DIF/VIF volume records, instead of weakening the common
  parser or claiming model-number extraction.
  Although upstream `detect.mvt` lists MAD/01/06, that water
  variant is NOT admitted to numeric publication without evidence.
- Configured `driver: auto` (or explicit driver) selects only by
  known MVT and DLL ID; the full source-backed decode, verified DLL
  CRC removal and TPL Mode-0 security gate remain mandatory.
  `NOKEY` in XMQ is NOT an on-air decryption guarantee.
- The Heltec `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml`
  is **build-only** with public test IDs. No live meter IDs, no keys,
  no OTA/flash, and no change to the existing production Techem firmware.
  Radio reception validation remains separate.

## Calupul 35–36: LSE 07/17 + WEP Munia (2026-11-09)

The native branch `experimental/xmq-multidriver-auto` now tracks **36
integrated XMQ driver families**.

- **`lse_07_17`** (LSE water): four public Mode-0 short-TPL
  upstream fixtures support optional `consumption_at_set_date_8_m3`
  (two archival frames with 7.000 m³), or `total_m3` and `due_m3`
  (two billing frames: 7.156/0 and 323.323/277.651 m³). Each
  publication requires that record to actually appear. The fourth
  source sample exposes DLL `version=0xD8`, which **is explicitly
  present** in the upstream `detect.mvt` table: both configured
  `driver: auto` and explicit `driver: lse_07_17` are tested.
  We never infer a model from missing metadata or publish unproven
  alarm/datetime fields.
- **`munia`** (WEP/02-04/1B room sensor): two original 47-byte
  Mode-0 short-TPL frames test `current_temperature_c` =
  20.1/21.7°C and `current_relative_humidity_rh` = 65.7/61.0%.
  Exactly verified zero-status `status: OK` is available as a text
  field. Unknown flags, invalid BCD, wrong model, unsupported
  transport, damaged padding or encrypted frames fail closed.
- Both are isolated native `PORTABLE` drivers using the official
  SX126x receiver's CRC verification and CRC removal; no legacy
  radio driver, no speculative plaintext claims from upstream
  `NOKEY` test metadata. AUTO requires a configured meter ID and
  an unambiguous upstream MVT signature, not a loose model match.
- The example Heltec configuration is **build-only**, with public
  upstream XMQ fixture IDs; no personal keys, no OTA/flash and no
  production Techem change. Live RF validation remains a separate step.

## Calupul 33–34: LSE 08 + Waterstarm (2026-11-08)

The native XMQ catalog on `experimental/xmq-multidriver-auto` now
contains **34 integrated driver families**, including two new strict,
configured-only Mode-0 profiles:

- **`lse_08`** (LSE/01/08, HCA): `consumption_at_set_date_hca`
  from the only public original XMQ short-TPL clear example, 321 HCA.
  Its TPL status is **`TPL_STS_128`**, not OK, and is published only
  as an optional text status from that verified profile. No claim about
  current HCA, other alarm states, or alternative firmware.
- **`waterstarm`** (DWZ/00/06 and DWZ/00/07): `total_m3` and
  `consumption_at_set_date_m3` from the two original 151-byte clear
  Mode-0 examples (55.360/27.650 and 6.800/4.819 m³).
  The wider historical volume series and EFE vendor variants are
  excluded. The remaining original examples include **four Mode-5
  AES TPL headers** (two with NOKEY supplied-clear application)
  and **two ELL transports**, all rejected for this no-key profile.
- Both support explicit `meter_drivers` and configured `driver: auto`.
  Admission requires the official native receiver's CRC-verified,
  CRC-stripped frame, a configured matching DLL meter ID, strict
  MVT/profile recognition, real Mode-0 security, and validated records.
  A NOKEY fixture is not decryption or RF proof.
- `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml`
  remains **build-only**, using public upstream fixture IDs, no actual
  personal meter identifiers or credentials, and **NO FLASH/OTA**.
  Existing production Techem firmware and runtime stay unchanged.

## Calupul 31–32: Itron + Qualcosonic (2026-11-08)

The `experimental/xmq-multidriver-auto` branch now contains
**32 integrated native XMQ driver families**, including two additional,
explicitly configured / configured-`auto` clear-only profiles.

- **`itron`** (ITW/00-03-33/07): bounded original 57-byte short-TPL
  CI `7A`, Mode-0 water records: `total_m3` and `target_m3`,
  from two independent upstream clear fixtures (4.605/3.888 m³,
  111.968/87.980 m³). Two upstream `NOKEY`-tagged **Mode-5 AES** fixtures (one long
  CI `72` and one short CI `7A`) are excluded from the no-key path. Proprietary status/identity bytes are not interpreted.
- **`qualcosonic`** (AXI/0B-0C/0D): three original Mode-0 short
  TPL 119/155-byte fixtures publish `total_m3`,
  `flow_temperature_c`, `return_temperature_c`, and `power_kw`
  from unambiguous standard DIF/VIF. The separate heating/cooling
  energies and target readings involve VIF modifiers; they are
  **not claimed** in this subset. Known clear zero-status only
  becomes optional `status: OK`; no guessed alarm states.
- Both use the official receiver's CRC-verified/CRC-stripped frame,
  configured DLL meter ID, exact MVT and clear-TPL publication gate.
  Recognition of a model is not authorization to publish a frame.
  `NOKEY` in XMQ does not prove a meter is unencrypted on RF.
- Updated `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml`
  is **build-only**, uses original public upstream fixture IDs, and
  must **not** be flashed. No user-private IDs/keys, no legacy radio,
  no changes to the production Heltec.

## Calupul 29–30: QWater V2 + Hydrodigit (2026-11-08)

Native `PORTABLE` Mode-0 profiles were added on
`experimental/xmq-multidriver-auto`, bringing the native catalog to
**30 XMQ driver families**.

- **Qundis QWater V2** (`qwaterv2`): `total_m3`,
  `target_year_m3`, `target_m3` from eight original, genuinely clear
  CI `7A` / Mode-0 XMQ examples. Limited to standard DIF/VIF 0C13,
  4C13, CC0813 volume records and demonstrated fixed application
  lengths. Other XMQ examples use WalkBy CI `78`, nested CI `72`,
  or an explicit AES key; **none** are admitted through this no-key path.
- **B.Meters Hydrodigit** (`hydrodigit`): `total_m3` from four
  original, genuinely Mode-0 CI `7A` examples, including 99999.999 m³
  and 0.030 m³. Historical monthly data, fraud/leak signals, backflow
  and battery voltage are vendor-specific and not published from
  unverified offsets. One NOKEY XMQ example is **Mode-5 AES** and
  another has ELL CI `8C`; both are excluded.
- Both support configured `driver: auto` and explicit driver settings,
  but only after CRC-verified / CRC-stripped RF, a configured meter ID,
  matching source MVT, Mode-0 security confirmation and strict standard
  record admission. Neither family is declared fully parity-complete
  with upstream XMQ and `NOKEY` is not interpreted as a universal
  no-AES statement.
- Build-only Heltec YAML uses **public XMQ fixture IDs**. No personal
  device identifiers or credentials were added. No flash/OTA has
  been performed and the live Techem pipeline is unchanged.

## Calupul 27–28: HCA E2 + BFW240radio (2026-11-08)

Two new portable native `meter_drivers` on
`experimental/xmq-multidriver-auto`, explicit or configured `driver: auto`,
with upstream XMQ and fixture regression tests.

- `hcae2` (Engelmann EFE/31/08): `consumption_hca` and
  `target_hca`. Two original Mode-0 T1 examples validate 0/0 HCA
  readings and a documented status. The original nonzero 242/373
  `ActiveLong` fixture has a **Mode-5 AES header**, even though
  upstream XMQ supplies readable application bytes: the original
  fixture MUST be rejected as on-air plaintext. A separately labeled
  **host-only synthetic** Mode-0 transport tests the numerical
  242/373 parser; it is NOT RF evidence. Other TPL status codes
  are not interpreted. The manufacturer flags are not a substitute
  for safety/tamper hardware. The two C1 upstream fixtures carry
  trailing CRC not claimed as CRC-stripped RF here; the 30
  half-month historical deltas are out of scope.
- `bfw240radio` (BFW/02/08): `current_hca` and `prev_hca`
  from six original public, Mode-0 XMQ examples, including
  904/961, 909/961 and 256/324 HCA. The manufacturer-specific
  18-month history is not claimed; only fixed-layout 55-byte
  application with checked date and official upstream counters.
- Receiver validation: source-only metadata and bound DLL IDs,
  strict CRC-verified/CRC-stripped radio frames, TPL Mode-0 gate,
  fixed model + subtype and complete structural decoding. No
  inferred decryption from upstream `NOKEY` fixture tags.
- `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml` is
  **build-only** with public test IDs, no real meter IDs, no flash.
  Existing Techem decoders and live Heltec remain unchanged.

## Calupul 25–26: EVO868 + Supercom 587 (2026-11-08)

Two more `PORTABLE` native families are integrated on
`experimental/xmq-multidriver-auto`: `evo868` (Maddalena MAD/50/06,07,16)
and `supercom587` (Sontex SON/3C/06,07). Both are selectable as
explicit `meter_drivers` or as configured `driver: auto`; passive
recognition alone never publishes measurements.

- EVO868: `total_m3` and `consumption_at_set_date_m3` from the
  two upstream short-TPL, actual Mode-0 public examples (IDs 79787776
  and 21031894). The wider monthly history and vendor data are not
  implemented in this bounded clear-only subset.
- Supercom 587: `total_m3` from the two upstream warm/cold-water
  short-TPL Mode-0 examples (IDs 12345678 and 11111111), reading the
  standard BCD record; no general manufacturer extension support.
- Both require the official receiver's validated/CRC-stripped DLL
  input, a configured matching meter ID, exact model/profile checks,
  a demonstrably unencrypted TPL and a structurally valid application
  subset. A `NOKEY` upstream sample is never itself treated as
  evidence that all firmware revisions are keyless.
- Example build-only mappings are in
  `experimental/semtech-radio/heltec-sx126x-combined-lab.yaml`.
  No personal IDs, no actual AES keys, no legacy receiver.
  No firmware was flashed and no RF operation is implied by CI.

## Calupul 23–24: RFM-AMB + Sensostar (2026-11-08)

`rfmamb` and `sensostar` are native `PORTABLE` clear-subset drivers.
Both are available to **configured** `meter_drivers`, including
`driver: auto` when the original manufacturer/version/medium matches.
All live input MUST be CRC-verified and stripped by the official
SX126x receiver, match an independently configured DLL meter ID and
pass strict CI=7A / Mode 0 / complete standard DIF-VIF decoding.

- `rfmamb` (BMT/10/1B) maps `current_temperature_c`,
  `average_temperature_1h_c`, `average_temperature_24h_c`,
  `current_relative_humidity_rh`,
  `average_relative_humidity_1h_rh`,
  `average_relative_humidity_24h_rh` using the single original
  clear RF example. The second original fixture is actual Mode-5
  ciphertext with a supplied test key, unsupported by the no-key route.
- `sensostar` (EFE/00/04) maps `total_kwh`, `total_water_m3`
  and optional `target_kwh` for the two original unencrypted radio
  examples. Upstream also includes wired M-Bus and an already-readable
  Mode-5 example; both are excluded from native plaintext publication.
- Software coverage is profile-scoped, not full upstream XMQ parity.
  No real meter identifiers or AES keys, no new legacy dependencies.
  The live Heltec firmware remains untouched. RF validation is separate.

## Calup 21–22 — QSmoke / QCaloric (2026-11-08)

Native configured-only clear-TPL subset for `qsmoke` and `qcaloric`.
Both have upstream GPL-3.0-or-later XMQ source in the native project.
Only CRC-verified DLL frames with a verified Mode 0 TPL are admitted;
`NOKEY` in fixtures does not prove that all deployed models are clear.

- QSmoke: `alarm_counter` and `test_button_counter` only, from
  the original QSmokep CI 7A/Mode 0 sample. The QSmokeo CI 78
  variant is NOT accepted. **These are historical test/alarm counters,
  NOT real-time fire/smoke alarm states. Do not use this decoder for
  life-safety notifications or actions.**
- QCaloric: `current_consumption_hca`, optional
  `consumption_at_set_date_hca`, only standard unencrypted CI 7A
  DIF/VIF HCA records. Manufacturer WalkBy CI 78, encrypted Mode 5
  and unverified proprietary formats are rejected; no claim of
  full driver parity with upstream.
- Both explicitly supported in `meter_drivers` and configured
  `driver: auto`; never authorize unconfigured readings.
- No Heltec OTA/flash, no modification to running Techem decoders.

## Calup native 19–20: Elvaco Sense + Lansen R4 (2026-11-08)

`elvsense` and `lansenrp` are `PORTABLE` and selectable as an
explicit `meter_drivers.driver` or as `auto` for a **configured** DLL ID.
Only original Mode-0 clear frames pass the native publication and
model-specific exact-record checks; passive AUTO never publishes.

- Elvaco Sense: native temperature (`temperature_c`), battery
  (`battery_v`), text `status=OK`. The only demonstrated clear profile
  is upstream ES1; ES2 and synthetic CO2/humidity example are NOKEY
  **Mode 5** and remain **blocked**. No keyless CO2 or RH claims.
- Lansen repeater R4: mapped `total_routed_messages_counter`,
  `used_router_slots_counter`, `seconds_to_mode_change_counter`,
  `listen_timer_value_counter`, `pause_timer_value_counter`,
  `start_time_value_counter`, `battery_v`, plus text `status` and
  `is_repeater_listening` (YES/NO). Only the two published
  same-layout clear fixtures are supported. Unknown variants reject.
- Source: original GPL-3.0-or-later wmbusmeters XMQ definitions, copied
  into `experimental/xmq/drivers`. No legacy receiver stack or private
  telegrams, IDs or credentials were added.
- Host tests prove parser/driver mapping, auto-selection and negative
  security checks. CI ESPHome build is software-only; no OTA/flash or
  on-air RF acceptance can be inferred.


## Lansen TH / SM — supplied-clear parsers (2026-11-08)

Offline, host-only C++ application parsers were added for `lansenth` and
`lansensm`, using original public XMQ records, no private meter IDs.
**These are NOT registered live native drivers**: both upstream NOKEY
fixtures advertise TPL AES Mode 5. NOKEY does not prove unencrypted RF.
The parsers accept only deliberately supplied clear applications, reject
unknown/duplicate/incomplete fields and reset outputs on error.
A standalone host CI test covers the unencrypted application records,
not a genuine RF decryption. A configured AES Mode-5 route with
independent identity, ciphertext and security-negative tests is required
before adding either to `SECURE_ONLY`, YAML or Home Assistant.
The current active 16-driver catalog remains unchanged. No flash/OTA.

## Lansen TH / SM — keyed decoder build verification (2026-11-08)
A model- and meter-ID-bound synthetic CBC Mode-5 native decoder has
been added for both original Lansen TH / SM profiles, exercising four
public already-clear XMQ payloads re-encrypted under synthetic keys.
It rejects wrong AES key, configured ID, MVT, CF mode, truncated frames
and raw supplied-clear NOKEY pretending to be on-air ciphertext.
The parser adds TH humidity one-byte VIF-1B variant from the second
original fixture. This step validates C++ decoder behavior only; do
not advertise AES availability in YAML until registration, catalog and
mapped HA integration are independently verified. CBC is not authenticated
and smoke outputs are NOT certified fire safety alarms. No real RF.

## Lansen TH / SM — explicit AES Mode 5 native route (2026-11-08)

Both are `SECURE_ONLY` on `experimental/xmq-multidriver-auto`:
never plaintext `meter_drivers` or AES `driver: auto`.
The software pipeline is CRC-verified T1/C1 -> explicit configured
meter ID, driver and AES-128 key -> strict LAS MVT and Mode-5 transport ->
two-block AES-CBC -> strict complete-application parser -> mapped numeric
fields and RSSI. The existing `secure_delivery` reject path ensures
zero sensor publication on mismatched sender or failed decrypt.

The four original public `NOKEY` XMQ examples are supplied-clear
applications under Mode-5 headers, NOT independent encrypted RF
golden vectors. Tests re-encrypt them with synthetic keys; live radio
interoperability has **not** been verified. AES-CBC does **not** provide
cryptographic authentication. The `lansensm` `smoke_detected` field is
**telemetry only**, NOT a certified fire/smoke alarm. Do not use it as
the sole source for life-safety actions.

Example: configure only if you possess the correct genuine AES key.
```yaml
wmbus_native_sx126x:
  secure_meters:
    - meter_id: "12345678"  # synthetic, replace with actual device ID
      driver: lansenth
      key: !secret lansen_th_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: current_temperature_c
      sensor:
        name: "Example Lansen TH temperature"
```
The YAML excerpt is illustrative; the complete SX126x configuration
is still required. No production OTA/flash was performed.

# Project context — `experimental/xmq-multidriver-auto`

> **Branch-specific context, updated 2026-11-07.** This section applies **only to `experimental/xmq-multidriver-auto`**. No default branch, legacy experimental branch, local YAML, hardware firmware or server configuration is changed by this documentation update. The older 2026-11-01 `xmq-independent-pipeline` notes are preserved below as historical context and **must not be interpreted as the active architecture on this branch**.

## Read-first instructions for development assistants (including Codex)

1. At the start of each new development session, **read this branch's `PROJECT_CONTEXT.md`**, `experimental/xmq/BATCH2_STATUS.md`, `experimental/xmq/STAGE_A_CLOSEOUT.md`, `experimental/xmq/tools/native_driver_specs.py`, and the relevant tests/code before editing. If instructions in historical sections conflict with current branch code, **verify code/CI and prefer the branch-specific section**, explaining discrepancies.
2. Work **only** on `experimental/xmq-multidriver-auto` unless the owner expressly requests a different target. No merge to `main`/`master`, no modification of other branches, no forced history rewrites and no production Heltec OTA/flash without explicit authorization.
3. Preserve **our native C++ receiver and algorithm** as the implementation. Use the *original* `wmbusmeters` XMQ definitions and public fixture JSON as an **independent compatibility reference**, not as authorization to revive the old driver/radio stack.
4. Prefer small, auditable commits; include positive **and negative** regression tests; await reported CI green before expanding behavior. Fix CI regressions before moving to the next calup. Never claim CI passed, an `app_main` self-test executed on hardware, or live field validation unless it actually did.
5. Never commit private meter IDs, captured production telegrams, encryption keys, Wi-Fi credentials, private YAML or other identifying telemetry. Synthetic IDs and original public upstream fixtures only.

## Audit point 7 — SX1262 GFSK RSSI source (2026-11-08)

**Follow-up GitHub 504 remediation (2026-11-08):**
ESPHome 2026.10.1's actual `esphome/espidf/framework.py` accepts
the semicolon-separated `ESPHOME_IDF_FRAMEWORK_MIRRORS` environment
variable; archive extraction detects ZIP/XZ via magic bytes rather
than the temporary download suffix. The `native-esphome-build` job
now first tries the pinned esphome-libs IDF 5.5.6 tar.xz, then a
fallback to Espressif's official 5.5.6 ZIP on
`dl.espressif.com/github_assets`. The
`IDF_GITHUB_ASSETS=dl.espressif.com/github_assets` flag uses the
official Espressif mirror for compiler/ROM tool assets. A
`~/.cache/esphome/idf` actions/cache key includes ESPHome 2026.10.1,
ESP-IDF 5.5.6, Python 3.12 and architecture to avoid stale
toolchains once the first successful installation seeds the cache.
`test_idf_mirror_ci_contract.py` is a host CI policy guard.
This modifies **only the CI download transport**. On-device RF
acceptance and exact-commit ESPHome build success are still pending
until confirmed by the current workflow run. No OTA/flash.

**Follow-up CI source audit:** The T1 lab YAML was incorrectly cloning
`experimental/semtech-sx126x-sx127x` even when the workflow ran on
`experimental/xmq-multidriver-auto`. That meant the first ESPHome
build could compile stale SX126x code; the 504 download failure masked
the issue before source compilation began. The T1 and combined Heltec
lab fixtures now use the checked-out `../../components` path,
consistent with the diagnostic lab and the SHA being tested.
`experimental/semtech-radio/ci/test_heltec_lab_source_ref.py` is
now a first-class host CI regression gate. No actual receiver source
or installed Heltec firmware was changed by this fix. The ESPHome
dependency download blocker is recorded at
https://github.com/radugeo74/esphome-sx1262/issues/1.
Do NOT mark RSSI audit fully green until exact-commit ESPHome
compilation completes. External GitHub HTTP 504 cannot be fixed
by changing the C++ parser.


Verified Semtech GFSK `GetPacketStatus(0x14)` after status byte:
`[0] RxStatus`, `[1] RssiSync` latched at synchronization,
`[2] RssiAvg` latched after RxDone. Both use UNSIGNED `-raw/2`
dBm. Standalone and combined T1/C1 here consume a progressive FIFO
stream, potentially before RxDone; do not substitute byte [2]
merely because it is commonly used by normal ESPHome RxDone.
The currently selected byte [1] was already correct in the
previous code checkpoint; this audit makes it explicit and
tests it against a pure shared helper, without silently changing
the measured field.

Implementation: `components/sx126x/gfsk_rssi.h` and
`components/sx126x/sx126x.cpp`; actual receiver calls the helper
and forwards `measurement.sync_dbm` before RX reset.
Opt-in `t1_raw_diagnostics: true` logs bounded sync and average
raw/dBm and a packet_received flag to identify stale pre-RxDone
averages in actual radio logs. This does NOT alter RSSI mapping,
numeric precision, decoding or any sensor publication policy.
Tests: `test_sx126x_gfsk_rssi.cpp` and
`ci/test_gfsk_rssi_source_contract.py` in native CI.
Unlike a lab compile, **only same-device RF observation can validate
whether the reported -114 dBm becomes realistic**.
No installed Heltec firmware was changed. Check current CI run
for exact commit before claiming green.

## Audit point 6 — NOKEY is not plaintext RF authorization (2026-11-08)

CI regression only (no runtime, hardware, crypto-key or YAML changes).
All 74 official XMQ fixtures from 16 drivers are re-read by
`experimental/xmq/tests/test_nokey_fixture_provenance.py`.
71 are `NOKEY`, yet 31 retain Mode-5 (AES) TPL security
headers. Never treat `NOKEY`, a `2F2F` filler or an XMQ
`_` clear-application separator as proof of an unencrypted
original radio capture. A separate iPERL NOKEY fixture has
M=0,N=1 (invalid Mode 0) and must be denied; iPERL M=N=0
is a positive real Mode-0 compatibility case.

`experimental/semtech-radio/test_nokey_rf_fail_closed.cpp`
compiles against **the actual production** `tpl_security::inspect`,
`publication::inspect` and
`batch2_plain::decode_configured` gates. Q400, Topas,
MK Radio4a and Minomess supplied-clear Mode-5 XMQ specimens
must NOT decode or publish on the no-key RF path. An iPERL
valid Mode-0 fixture still decodes; M=0,N=1 is rejected.
A previously concealed **decoder-level security gap was found by the
host regression**: `iperl_plain.h::decode` masked only four mode
bits and accepted a public NOKEY M=0,N=1 invalid Mode-0 header.
Actual T1/C1 HA publication already blocked it via
`publication::inspect`, but a direct decoder call could parse it.
Fix: iPERL's full RF `decode()` now calls the shared five-bit
`tpl_security::inspect`; the generic configured catalog adds its
own publication admission gate; a separate
`decode_clear_application()` preserves offline supplied-clear
parity without claiming an unencrypted RF packet. Added negative
tests for N!=0 and M=16, with positive Mode-0 N=0 preserved.
No real RF, keys, installed firmware or unrelated drivers changed. Verify *all applicable CI jobs*
for the exact commit; software green != hardware verification.

## Audit point 5 — OMS security evidence catalog (2026-11-08)

Catalog 16/16 native drivers:
`experimental/xmq/security_profile_catalog.json` and
`experimental/xmq/SECURITY_PROFILE_CATALOG.md`.
Records runtime clear/keyed capabilities, actual public XMQ
MVT signatures and fixture counts, and limits of inference from
`NOKEY`/pre-decrypted tests. OMS product certificate matching
is deliberately unknown for every entry until a specific SKU,
generation and certificate can be verified. The independent
`experimental/xmq/tests/test_security_profile_catalog.py`
guards consistency with `native_driver_specs.py`, all original
XMQ files, keyed decoder implementations and documentation;
it is included in the native GitHub Actions workflow.
**No firmware, active driver, encryption, YAML, keys or HA changes.**
Check CI for this commit before describing it as validated.

## Audit point 4 — Host integration through actual native delivery (2026-11-08)

Only on `experimental/xmq-multidriver-auto`. The secure publication
logic formerly embedded inside
`NativeReceiverProbe::publish_secure_batch2_()` is now factored into
the pure C++ `secure_meter_delivery.h` used by **both** the receiver
and `experimental/semtech-radio/test_secure_rx_to_mapped_sensors.cpp`.
The shared `evaluate()` matches exact configured DLL-sender IDs,
invokes the real `batch2_secure::decode_configured()` with that
meter's AES key, prevents partial/stale fields after rejection, and
returns `Unmatched`, `Rejected`, or `Ready`. A rejected secure
binding **consumes** the frame, with no clear fallback. `emit_mapped()`
calls exactly the numeric/text/RSSI callbacks also used by production
HA entities, and does so **only** after `Ready`. The receiver still
enforces configured sensor-ID/field bindings and rounds to each HA
sensor's configured precision. It never publishes from a passive
AUTO candidate.

CI's new host integration test exercises actual
`verified::process()` on T1 3-of-6/format-A CRC, C1-A CRC, C1-B
CRC; `registry::classify()`; runtime AES binding; the same
native AES decoder and mapped output callbacks. Positive:
original public encrypted Apator162 fixture, public
Topas and MK Radio4a application fixtures independently encrypted
with synthetic keys; numeric, text date and RSSI output gates.
Negative: corrupt DLL CRC (blocks before AES), wrong key,
meter ID, model, security mode, absent config, and already-clear
upstream `NOKEY` content that must not be treated as on-air AES.
No secret/real meter material was committed. Existing clear-profile
T1/C1 and driver-level secure positive/negative tests remain in CI.
The Stage-A structural guard asserts production actually imports
and uses this common delivery logic.

**Critical status distinction:** host/ESP-IDF/ESPHome builds do
not constitute a physical Heltec radio test or a Home Assistant
server result. Do NOT OTA/flash or mark live RF/HA acceptance based
only on these tests. Audit point 4 closes the software integration
coverage gap; a real deployment and physical receive/HA validation
remain deferred. Audit points 1–3 continue unchanged.

**FULL GREEN (3/3):** [GitHub Actions #42459485622](https://github.com/radugeo74/esphome-sx1262/actions/runs/42459485622),
code commit `1efb2ea455d645e193f09f33550a06f0235e1778`.
Jobs: native packet/host tests including mapped output; ESP-IDF ESP32-S3
portable link; native ESPHome Heltec and combined T1/C1 builds.
Only software/build validation, no physical flash or RF/HA approval.

## Audit point 3 — Passive AUTO independent of AES / HA (2026-11-08)

On `experimental/xmq-multidriver-auto`, the native receiver now logs
**metadata-only `NATIVE AUTO`** candidates for all 16 integrated
native XMQ families, after the existing DLL/CRC verification gate on
T1 and C1. Each diagnostic has `candidate`, `source`, `dll`,
`meter` when BCD, `tpl_security`, `tpl_mode`,
`driver_class`, and **`decode_status=not_attempted`**.
The bounded per-meter deduplication also distinguishes changes in the
observed TPL security mode. This is not a Home Assistant entity or a
validated measurement.

The new `xmq_auto_dispatch.h::identify` returns a **candidate** plus
whether a *plaintext* decoder is compiled; it never grants access to
the decoder. Existing `resolve` and configured-only dispatch remain
unchanged. `mkradio4a` and `topaseskr` are both recognized
passively but still return `NotCompiled` from plaintext AUTO and
**remain exclusively keyed `secure_meters` for encrypted RF**.
No live AES key selection, no fallback, no sensor publication on mere
identification. Nonconfigured meters continue to be diagnostic-only.

The nested-aware identification `batch2_auto_identify.h` recognizes
both `AMT/F1/06` and `AMT/F1/07` as Topas. For MK Radio4a,
the raw converter `TCH/95/37` is **insufficient**; the explicit
nested long TPL must also carry either `HYD/FE/06` (warm water) or
`WEH/FE/07` (cold water), as in upstream `mkradio4a.xmq`
public original fixtures. Upstream `detect.mvt` omits WEH;
this is a narrowly constrained passive recognizer case ONLY,
**not a generated global signature, decoder change or new
auto-decoding permission**. Conflicting and unknown nested identities
fail closed and are not confidently named MK Radio4a.

The actual receiver uses the pure `passive_auto_identify.h`
helper, independently exercised by `test_passive_auto_identification.cpp`
for all 16 families, converter-only refusal and generic MVT collisions.
Also covered by `test_batch2_auto_identification.cpp`,
`test_xmq_auto_registry.cpp`, and
`test_batch2_native_catalog_dispatch.cpp`, including negative
ambiguous-MVT and keyless plaintext-refusal cases. Code is
software-only, no physical RF or HA validation; no installed Heltec
OTA, no changes to AES decoder, Techem generated tables or security
filters. **FULL GREEN:** [GitHub Actions #42457899531](https://github.com/radugeo74/esphome-sx1262/actions/runs/42457899531),
code commit `2ebda8be74fd08e84e4c7ff7f6cfcbe9fd9b4866`;
all three jobs passed (native packet host regressions, ESP-IDF ESP32-S3
link, native ESPHome/Heltec including combined T1/C1 builds).
The shared `passive_auto_identify.h` also rejects a bare HYD/FE/06
signature and unrelated DLL gateways as a complete MK Radio4a profile;
the tests guard this and prevent generic DLL fallback from suppressing a
distinct nested TPL identity. Never interpret the 3/3 CI result as a
real on-air RF or Home Assistant test.

## Audit point 2 — OMS TPL security classification (2026-11-08)

Implemented **only on this branch**: `tpl_security_probe.h`
previously masked security mode with `& 0x0F` (only four bits)
and accepted mode 0 with nonzero N encrypted-block count.
OMS Vol. 2 §7.2.4.2, Table 19 (Issue 5.0.1) defines
M=bits 8..12 (five bits), N=bits 4..7, and requires
**M=0 and N=0** for no encryption. See
https://oms-group.org/wp-content/uploads/2024/10/OMS-Spec_Vol2_Primary_v501_01.pdf .

Now `tpl_security::inspect` reads all five bits, allows clear
only for M=N=0, reports 4/5/7/10/13 as encrypted and treats
unsupported/reserved security configurations (including
mode 0 with N!=0, and modes 16..31) as Unknown.
The existing `native_publication_gate.h` already rejects
both Encrypted and Unknown, so no decoder changes are needed.
Keep manufacturer-specific CI A0/A2 on their existing strict
driver-specific path; this is NOT a blanket approval for
encrypted payloads or AUTO decryption.

Test source:
`experimental/semtech-radio/tests/test_tpl_security_probe.cpp`
checks both short CI=7A and long CI=72 CF offsets,
M=0,N>0 fail-closed, M=16 not aliased to zero,
known encrypted modes, and valid clear configurations.
`experimental/semtech-radio/test_native_publication_gate.cpp`
asserts denial of Home Assistant publication for both failure
classes with both CI variants, preserving genuine Mode-0 N=0.
No flash, RF assertions or unrelated architecture changes.
The original WMBusmeters fixtures were not modified; only synthetic
Minomess/Hydrus test variants were corrected to M=N=0.
**FULL GREEN:** [GitHub Actions #42456075410](https://github.com/radugeo74/esphome-sx1262/actions/runs/42456075410),
code commit `c87f754e6367e79301febdeeb460cddcfc1a7d8d`,
all three jobs passed: native packet/host regressions,
ESP-IDF ESP32-S3 link, and ESPHome/Heltec builds.


## Audit point 1 — secure_meter runtime registration (2026-11-08)

**Implemented and 3/3 software CI green in #42454231643:**
the YAML `secure_meters` schema permitted five drivers but
`NativeReceiverProbe::add_secure_meter()` silently rejected two:
`mkradio4a` and `topaseskr`. This is an integration defect,
separate from their successful native AES decoder tests.

Admission is now shared by
`components/wmbus_native_sx126x/secure_meter_binding.h` and the
actual `NativeReceiverProbe::add_secure_meter()` wrapper. The
allowlist comprises **`minomess`, `kamwater`, `apator162`,
`mkradio4a`, `topaseskr`**. Only Minomess and MK Radio4a may
have a separate configured DLL sender; ID, key format and
duplicate binding checks are preserved. The C++ test
`experimental/semtech-radio/test_secure_meter_binding.cpp`
uses the actual runtime admission function and tests all five
positive registrations and negative cases. The Python manifest
regression compares the exact YAML and C++ whitelists and verifies
the receiver calls that function. The synthetic batch-2 ESPHome
CI fixture now compiles all five keyed bindings, including Topas.

**Historical audit-1 scope:** Subsequent audit points 2 and 3 are
now implemented as documented above. The separate RF/HA end-to-end
test coverage remains **NOT IMPLEMENTED**. No changes
to the Techem live decoder, encrypted payload parser, installed
Heltec firmware, actual meter IDs or AES production keys.
The most recent confirmed previous full code CI was
[run #42410374030](https://github.com/radugeo74/esphome-sx1262/actions/runs/42410374030);
confirm the new CI run independently before marking the audit
item fully green.

## Active architecture and working baseline

- Hardware target: Heltec ESP32-S3 / SX1262 / 8 MB; ESPHome 2026.10.1, ESP-IDF 5.5.6 in the experimental build context. **The official ESPHome SX126x receiver is the sole radio owner**, with native T1/C1 handling and a bounded `components/wmbus_native_sx126x` application pipeline.
- Native ingress must verify complete radio/DLL frame format and CRC, independently match the configured meter identity and allowed link mode, enforce valid plaintext/decryption/compact gates, and **only then** decode and publish finite mapped fields. Unconfigured meters can be observed diagnostically but must never auto-publish sensor readings.
- The original live Techem decoding behavior for **`fhkvdataiii` (manufacturer CI A0)** and **`mkradio4` (CI A2)** must remain unchanged. Their generated IXML tables and sensor mappings remain the field-observed baseline. The native `PORTABLE` manifest contains **`iperl`, `hydrus`, `lansenpu`, `lansendw`, `cma12w`, `ultraheat`, `sontex868`**, all limited to source-verified clear profiles. `mkradio4a` now has a separately configured **`SECURE_ONLY`** AES Mode-5 receiver; it remains unavailable for generic plaintext `meter_drivers`/`auto`.
- Source of truth: `experimental/xmq/tools/native_driver_specs.py` plus the actual configured dispatcher `components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h`. **`CONFIGURED_NATIVE = ("minomess", "kamwater", "apator162")` is software-implemented**. The three are excluded only from generic DLL-only `SUPPORTED` and support dedicated configured-only `driver: auto` plus explicit `meter_drivers` for strictly clear RF. `CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE` is the YAML driver whitelist. Their encrypted path is `secure_meters` with an explicit driver and AES key, never automatic key guessing. The legacy `BATCH2_PENDING` category was retired; it must no longer be used to describe the current software status. None is field-approved without real RF/HA validation.
- Preserve the existing original XMQ generated decoder and T1/C1 algorithms, per-meter RSSI, temperature accuracy, status/dates, no stale or partial publishing on errors, and visibility of unconfigured telegrams where already supported. Never infer a valid decode solely from a matching manufacturer, signature, data CRC or decrypted-looking bytes.

## 2026-11-07 addition: Supercal and Q400 clear-only (CI pending)

After code checkpoint `8cb03357806b50316e88e3eb9e25a1cffb7e4813`,
the branch adds original public XMQ definitions and native configured
clear-only decoding for `supercal` (SON/1B/04) and `q400`
(AXI/01,10,20/07). Both require a configured ID, native verified
DLL CRC and security-mode-zero short TPL, plus strict DIF/VIF
application validation; no AES fallback or publishing from unconfigured
telegrams. Original Sontex 739 clear application fixture and three
original Axioma Q400 plaintext fixtures are checked by new native CI
regressions. The two Q400 upstream Mode-5 NOKEY examples are
explicitly rejected. Unimplemented extended historic/date fields are
not represented as supported.

`topaseskr` is newly **software-integrated via SECURE_ONLY**
(excluded from `CONFIGURABLE` and `driver: auto`). Its original
two upstream `NOKEY` examples are Mode-5 post-decrypted applications.
The new narrowly scoped short-TPL decrypt helper supports the
`2F2F` prefix without changing the existing Apator `2F2F0F`
requirement. Strict AMT/F1/06 or 07 DLL identity, explicit AES-128 key,
complete four-block encrypted TPL, and full standard DIF/VIF
application validation are required before publishing a small numeric
subset (`total_m3`, `temperature_c`, `current_flow_m3h`,
`volume_year_period_m3`). Positive tests re-encrypt both original
applications with a synthetic key, and negative tests reject wrong
key, identity, mode, model, corruption and truncation. CBC/`2F2F`
is not a MAC. Real RF, production key and Home Assistant acceptance
remain OPEN. Do not classify any original `NOKEY` examples as on-air
ciphertext, or `topaseskr` as a plaintext AUTO driver.

After these additions the manifest contains 14 `CONFIGURABLE`
and 2 `SECURE_ONLY` driver names, 16 software routes in total,
**subject to new CI completion**.

Current changes have NOT passed all three CI jobs at the time of this
note. Last fully green code run remains `42401195133` at `8cb0335`.
No new OTA/flash or real RF/HA acceptance occurred.

## Topas NOKEY supplied-clear adjustment — 2026-11-07 (CI in progress)

The user verified the first original upstream `topaseskr` telegram
in the online wmbusmeters analyzer without a supplied AES key. This
is possible because the upstream sample stores readable EN 13757-3
DIF/VIF records, even though its short-TPL header declares
`0x0540` AES-CBC Mode 5 with four blocks. Interpret `NOKEY` as a
statement about the supplied test bytes, **not** proof of
unencrypted on-air transmission.

The separate host-only file
`experimental/semtech-radio/host_only/topaseskr_supplied_clear_fixture.h`
now parses explicitly supplied already-clear sample frames with no
key, requiring the original AMT/F1/06|07 metadata, exact 4-block
`0x0540` layout, `2F2F` fixture prefix, configured expected ID,
and complete validated EN 13757-3 records. The independent
`test_topaseskr_supplied_clear.cpp` tests both upstream original
fixtures, numerical values, negative cases and zero publication
from the normal live plaintext dispatcher.

This is deliberately a **host-only offline decoder**, not a relaxed
`native_publication_gate`. An actual live RF packet declaring
`0x0540` still goes through the existing explicit-key
`topaseskr` AES Mode-5 receiver; it is never downgraded to
`driver: auto`/keyless `meter_drivers` based on a marker or
NOKEY fixture. `SECURE_ONLY` remains unchanged. No OTA/flash or
physical RF/HA validation was performed.

**Last confirmed all-green code baseline before this addition:**
`623b03e3`, [GitHub Actions #42404050221](https://github.com/radugeo74/esphome-sx1262/actions/runs/42404050221).
The new offline-fixture CI must be independently verified.

## MK Radio4a NOKEY adjustment — 2026-11-07 (new CI pending)

Following the Topas correction, the two upstream `mkradio4a.xmq`
`NOKEY` fixtures also decode without any AES key **as supplied
already-readable data**; this behavior already existed in
`components/wmbus_native_sx126x/mkradio4a_plain.h`.
A new separate host-only adapter
`experimental/semtech-radio/host_only/mkradio4a_supplied_clear_fixture.h`
adds strict radio converter DLL and nested TPL identity verification,
model, lengths, Mode-5 `0x0530` fixture layout, and the
storage-one target/date field validation. New
`test_mkradio4a_supplied_clear.cpp` checks both original
warm/cold fixture values, wrong IDs and malformed data, and
asserts the configured live plaintext decoder still rejects the
Mode-5 samples.

This is an **offline supplied-clear reference**, not an inference
that on-air radio frames are unencrypted and not a change to
`SECURE_ONLY`, `driver: auto`, or the live AES security path.
Live MK Radio4a with Mode-5 still needs explicit key and two IDs
in `secure_meters`. No physical Heltec flash, RF or HA claim.
The new three-job workflow is `42410374030`; keep its result
separate from the previous proven green code commit `623b03e3`.

## MK Radio4a configured secure receiver — 2026-11-07 (ALL CI GREEN)

Owner authorized completing the existing post-decrypt-only `mkradio4a`
driver. Source `experimental/xmq/drivers/mkradio4a.xmq` has two
public fixtures, both **Mode-5 `NOKEY` post-decrypted examples**,
not independently checked original radio ciphertext with an AES key.
The existing native record interpreter already decodes the two
reference `target_m3` (`16.1`, `75.2`) and
`target_date` (`2021-12-31`) fields.

A new strict `secure_meters` route requires explicit
`driver: mkradio4a`, AES key, TCH/95/37 DLL converter ID
(`dll_meter_id`), and nested TPL meter ID (`meter_id`).
The upstream examples use nested HYD/FE/06 and WEH/FE/07.
The original Mode-5 configuration 0x0530 protects three 16-byte
blocks; the five additional trailer bytes are ignored as
non-application data. The existing mbedTLS long-TPL decryptor
is reused **only on a private bounded truncated copy**, after
complete radio/DLL CRC and identity checks. The decrypted application
must pass full native standard DIF/VIF volume and date checks,
with transactional `target_m3` and secure-only
`target_date` text publication. CBC lacks a MAC; 2F2F is
only a structural sanity check, not authentication.

Tests wrap independently AES-encrypted public application records
with synthetic valid format-B DLL CRC and assert both fields for
both upstream examples. Negative tests block bad key, missing
key, wrong gateway/TPL ID, model, ciphertext, source
`NOKEY` falsely presented as RF, invalid length/mode,
and CRC. Heltec combined build-only YAML and separate
keyed ESP32-S3 YAML contain synthetic binding/sensor mappings.
`SECURE_ONLY = ("mkradio4a",)` replaces its former
`POST_DECRYPT_ONLY` classification. `SUPPORTED` and
`CONFIGURABLE` plaintext lists are unchanged.

**All three CI jobs passed** on code commit `8cb03357806b50316e88e3eb9e25a1cffb7e4813`, [run `42401195133`](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133). The native test covers both synthetic encrypted versions of the public XMQ clear applications, wrong keys/IDs/security/CRC, while ESP-IDF ESP32-S3 and combined Heltec T1/C1 build are green. Earlier red runs reflect intermediate tests and schema corrections. No real MK Radio4a receiver/HA validation, no production keys and no installed firmware changes.

## Manifest cleanup after successful software tests — 2026-11-07

The obsolete `BATCH2_PENDING` classification has been removed from
the executable manifest and CI assertions. `CONFIGURED_NATIVE`
contains Minomess, Kamwater and Apator162, and `CONFIGURABLE`
includes them in the YAML whitelist. This cleanup **does not**
insert them into generic DLL-only `SUPPORTED`, change the strict
configured AUTO selector, guess AES keys, authorize unconfigured
HA publication, or imply real-meter field validation. Previous
software baseline was fully green at run `42393848471`; the
manifest-cleanup CI is a separate check and must be green before
claiming this change fully verified.

## Current native driver and CI checkpoint (2026-11-07)

**Earlier full code-integration checkpoint:** `2f518a35c5ce34d56d8c40497755380293a4e4f3`.
**GitHub Actions:** [run 42393848471](https://github.com/radugeo74/esphome-sx1262/actions/runs/42393848471), all three jobs PASS:
`native-packet-input`, `stage-a-espidf-s3-portable-link`,
`native-esphome-build`. The Heltec T1/C1 build-only YAML compiles
the configured AUTO bindings for Ultraheat and Sontex868 in addition
to the previously added models; **no OTA/flash** was performed.

- `GENERATED` (established Techem): `fhkvdataiii`, `mkradio4`.
- `PORTABLE` (native clear application subsets): `iperl`,
  `hydrus`, `lansenpu`, `lansendw`, `cma12w`,
  `ultraheat`, `sontex868`.
- `CONFIGURED_NATIVE`: `minomess`, `kamwater`, `apator162`,
  with dedicated configured clear AUTO/explicit dispatch; they are
  excluded only from the generic `SUPPORTED` selector.
- `CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE` is the YAML whitelist.
  This is software availability, not approval for physical RF operation.
- `SECURE_ONLY`: `mkradio4a` has a specifically keyed
  TCH DLL + nested HYD/WEH TPL long-header AES Mode-5 path.
  It is deliberately excluded from plaintext `CONFIGURABLE`/`auto`.
  Hardware field validation remains OPEN.

**AUTO vs explicit:** all 12 listed main drivers can be selected on
an **explicitly configured meter ID**, using `driver: auto` where
the original signature is unambiguous, or by explicit driver name.
For a Minomess converter, `dll_meter_id` and long-TPL `meter_id`
must independently match. For encrypted frames, only explicitly keyed
`secure_meters` and validated security logic may publish values.
Unconfigured RF discovery remains **metadata-only** and never creates
HA entities. Techem has actual device observations; all added
models retain a separate physical RF-to-HA acceptance gate.

**Ultraheat/Sontex868 boundary:** only genuinely clear short TPL
`0x7A` records with supported numeric fields are admitted.
Ultraheat supports `heat_kwh`, `volume_m3`, `power_kw`,
`flow_m3h`, `flow_c`, `return_c`; Sontex868 supports
`current_consumption_hca`, `consumption_at_set_date_hca`,
`current_temp_c`, `current_room_temp_c`, `max_temp_c`,
`max_temp_previous_period_c`. Date/status, wired-M-Bus and
ELL/post-decrypted cases are not claimed as supported clear RF.
Two original public samples, negative tests, a long Sontex
two-block format-B CRC envelope, and the Heltec compile passed.
The previous red CI runs were intermediate fixture/test revisions,
not unresolved failures in this fully green code revision.

**Historical checkpoint rule:** Stage A and earlier Stage B sections
below preserve evidence **as of their original dates**. Their
statements that batch-2 drivers were unavailable for YAML/AUTO or
that current firmware lacks native dispatch are superseded by this
current status. Do not rewrite historical test counts as full-frame
parity or assume CI establishes actual device acceptance.

## Stage A — completed consolidation, CI green (owner-confirmed)

The shared-source C++ application/structural algorithms are located at:

- `experimental/xmq/portable_cpp/apator162_ixml_fields.h` — bounded already-clear Apator162 subset, with host forwarding include.
- `components/wmbus_native_sx126x/kamwater_status_semantics.h` and `kamwater_numeric_semantics.h` — Kamwater states, durations and numeric fields.
- `components/wmbus_native_sx126x/kamwater_tpl_status_semantics.h` — narrow already-clear short TPL7A ERROR semantics.
- `components/wmbus_native_sx126x/minomess_wired_frame.h` — **wired M-Bus** envelope, never a wireless T1/C1 transport decoder.
- `components/wmbus_native_sx126x/kamwater_ell_ctr_iv.h` — keyless ELL AES-CTR IV derivation and big-endian 128-bit counter. Host-only OpenSSL AES-CBC/CTR/CCM decryptors and experimental secure ingress **remain isolated**; the native module is **not** an authenticated decryptor.

See `experimental/xmq/STAGE_A_CLOSEOUT.md` for acceptance details and CI limits. ESP-IDF CI builds/links the portable probe; compiling `app_main` does not mean its assertions ran on physical hardware. No production firmware was flashed for Stage A.

## Stage B — historical source-field checkpoint (2026-11-06; device gate still OPEN)

- **Last owner-confirmed green (2026-11-06): 177/177 positive functional JSON fields** across the original 39 upstream fixture cases (`minomess` 6, `kamwater` 14, `apator162` 19). The original JSON contains 181 field occurrences; the other **four** belong to `kamwater #9` with an invalid CCM tag and must remain deliberately rejected, not counted as successful decoded measurements. These are individually demonstrated host-side comparisons, **not 39 full replay passes**.
- Original `kamwater #10`: host OpenSSL secure ingress checks the configured DLL identity and a synthetic format-B CRC envelope around the original authenticated encrypted payload, and compares `total_m3`, `target_m3`, `status=TEMPORARY_ERROR`, `accoustic_noise=FFFFFFFFFFFF`. Public bad-tag #9 and stale-output regressions reject. The same mbedTLS CCM implementation compiled for ESP-IDF has **executed host Linux CI** verifying original #9/#10 and malformed input. This does not execute `app_main` on the Heltec.
- Original `apator162 #19`: AES-CBC mode-5 host decrypt plus original `total_m3=7.512` with a **synthetic** wireless format-B DLL/CRC envelope, identity mismatch and CRC rejection (green). CBC and `2F2F` are not authentication.
- Original `minomess #1/#2`: complete **wired M-Bus**, not wireless T1/C1. All 13 source JSON comparisons and M-Bus length/checksum plus independently configured eight-digit meter BCD identity gate and its negative cases are owner-confirmed green as of latest `7e89dec`. Remaining `minomess #3..#6` are original already-clear/post-decrypted RF application tests, not full secured RF replay.
- The actual native `wmbus_pure::publication::inspect` was tested against a synthetic clear Kamwater ELL/compact frame and **blocks CI=0x8D** (`UnsupportedTransport`) despite successful host-only decoding. No experimental driver is registered as live.
- **Still OPEN — no false promotion:** 0/39 approved production-equivalent complete frame-to-Home-Assistant sensor replays; 0/3 batch-2 drivers admitted. Pending: authentic complete T1/C1 frames and decryption/provenance for all needed modes, strict real identity/link-mode and replay policies, ESP-IDF/runtime execution where necessary, transactional sensor mapping and separately authorized Heltec canary/rollback. See `experimental/xmq/STAGE_B_GATE_STATUS.md` for the precise acceptance gate.
- Keep `BATCH2_PENDING = ("minomess", "kamwater", "apator162")` excluded from `SUPPORTED`, YAML driver selection and native publishing. Do **not** flash, modify installed YAML, or alter established `fhkvdataiii`/`mkradio4` behavior without explicit authorization.

## Stage B native integration — 2026-11-07 (all CI green; device validation OPEN)

**Last owner-confirmed GitHub CI green: `8136a346`, run
`42350691692` (three jobs PASS: native packet input, standalone
ESP-IDF ESP32-S3 probe, and native ESPHome complete build).**
Full build includes the opt-in `secure_meters` configuration
and conditional mbedTLS CCM/CMAC Kconfig enablement.
Earlier native source-fixture and rejection tests remain green.

**The three experimental drivers are CODE-INTEGRATED:**
- `minomess`: OMS long TPL AES-CBC Mode 5, existing native XMQ
  application decoder; optional **explicit** `dll_meter_id` for
  wireless converter DLL identity differing from actual meter TPL
  identity. TPL meter must match configured `meter_id`; optional
  DLL sender must match configured `dll_meter_id`. No AUTO key
  guesses or ambiguous multi-target sender bindings.
- `kamwater`: ELL-II AES-CTR for independently checked compact /
  clear records, plus authenticated OMS profile-D AES-CMAC/CCM
  (original public #10 positive and #9 bad CCM tag rejected).
  The validated status text is published only after CCM acceptance.
- `apator162`: OMS short TPL AES-CBC Mode 5 to native XMQ
  `total_m3`; original public encrypted #19 host regression.
All three use the actual experimental `NativeReceiverProbe` verified
T1/C1 ingress with strict, opt-in YAML `secure_meters`, individual
16-byte AES keys from `!secret`, and sensor/RSSI mapping. Existing
Techem `fhkvdataiii` and `mkradio4` paths remain unchanged.

**Validation boundary:**
- **PASS:** executable source-fixture host tests, positive/negative
  secure dispatcher regressions, all three CI jobs, complete ESP32-S3
  build/link, existing non-secure regressions.
- **NOT TESTED:** runtime on physical Heltec hardware, original
  *complete on-air* T1/C1 capture with each target driver, real
  production key provisioning, or observed Home Assistant state
  publication. The synthetic Minomess converter/CTR fixtures
  and public upstream ciphertext are not substituted for these.
- **Release gate remains OPEN:** 0/3 drivers admitted to
  `SUPPORTED` / AUTO and 0/39 complete production-equivalent
  physical RF-to-HA approvals. A separate explicitly authorized
  test build/flash with rollback and suitable actual meter captures
  is needed before production admission. No NVS work is relevant.
  **Installed Heltec firmware is unchanged.**

**Next action is hardware/runtime validation, not additional driver
framework work.** If the site has no Minomess/Kamwater/Apator162
meters or keys, report that physical end-to-end validation is
unavailable rather than inventing approval. Do not flash, enable
YAML for real meters, or alter stable Techem paths without owner's
explicit authorization. Consult
`experimental/xmq/STAGE_B_GATE_STATUS.md` for historical gates.

## Stage B manual NOKEY / clear RF path — 2026-11-07 (CI GREEN; hardware gate OPEN)

**Latest verified run `42378059859` at `f6fa1dca` is SUCCESS:
all three jobs PASS** (`native-packet-input`,
`stage-a-espidf-s3-portable-link`, `native-esphome-build`).
The keyless ESPHome YAML config and complete ESP32-S3 compile,
as well as the separate keyed build, passed. Eight preceding
intermediate workflow runs (`42377687000` through `42377967849`)
remain historically RED because the Stage A static guard still
expected the old Kamwater compact-only
`decode_clear_ell_ii_tpl79(` call instead of the now-shared
`decode_clear_ell_ii(` wrapper. Commit `f6fa1dca` updated
the guard while preserving ELL/CRC/transport checks. Do NOT re-run
or treat old red commit statuses as failures of current code.
The post-green changes to this context are docs-only.

- The `meter_drivers` YAML now accepts explicit `minomess`,
  `kamwater`, `apator162` **without any `key`**, while the
  generated `SUPPORTED` / `auto` list remains unchanged.
  Optional `dll_meter_id` for Minomess lets the configured DLL
  converter sender differ from the actual TPL meter ID. Duplicate,
  ambiguous and mixed-key/plain bindings are forbidden.
- Actual verified T1 and C1 callbacks now route explicit clear
  batch-2 readings to `xmq_batch2_plain_dispatch.h` before the
  existing generic plaintext gate. The clear path matches DLL sender,
  checks Minomess long TPL identity and **TPL security mode 0**, uses
  existing strict Apator plaintext admission, and Kamwater checks
  ELL-II mode 0 + independent payload CRC for clear full `78`,
  compact `79` or short `7A` applications. CRC, model, frame
  shape or security failures do not publish readings, RSSI or fall
  back to AUTO/encrypted dispatch. C1 retains its existing
  `c1_plaintext_publish: true` opt-in.
- Host tests cover Minomess converter and cipher-mode-5 rejection,
  Kamwater clear full/compact and bad ELL CRC/AES-CTR refusal,
  and Apator short plaintext and mode-5 refusal. A **separate
  build-only keyless ESP32-S3 YAML** confirms all 3 can compile
  with `meter_drivers`, `xmq_sensors`, no AES keys and no
  `secure_meters`: `experimental/semtech-radio/ci/native-sx126x-batch2-nokey-lab.yaml`.

**Important:** original upstream `NOKEY` samples may be
post-decrypted applications rather than actual unencrypted on-air
telegrams. In particular, original Minomess examples carry TPL
Mode 5 despite `NOKEY`; never accept that header as plaintext
without a verified keyed decryptor. Some Kamwater compact profiles
reference a previously validated full telegram and cannot be
trusted without that reference. This manual no-key path authorizes
only independently VERIFIED clear RF; it does not magically
decode encrypted radio without a key. The on-device proof and
real HA publication gate remain open; no flash was performed.

## Stage B passive automatic identification — 2026-11-07 (historical checkpoint; superseded by green CI above)

**Owner decision:** keep `minomess`, `kamwater`, `apator162`
**manually configured only for actual decoding**, but identify all
three automatically in CRC-verified T1/C1 traffic, even for unknown
`configured_meter_ids`. No new YAML setting or real-meter ID/key
is necessary for passive recognition. Existing production Techem
`auto`, explicit `meter_drivers`, `secure_meters`, sensors and
publication permissions are untouched.

Implementation:
- `components/wmbus_native_sx126x/batch2_auto_identify.h`:
  read-only MVT metadata recognition using the FULL generated
  original XMQ registry. Prefer meter identity from long TPL;
  otherwise ELL address if available, then DLL. Original
  Minomess `ZRI/00/07`, `ZRI/01/16`, `ZRI/01/06`;
  Kamwater `KAM`/`KAW`; Apator162 `APA/05/06` and
  `APA/05/07` are recognized; ambiguous/conflicting driver
  signatures never produce a definitive candidate. No payload
  decryption, metadata-based admission, or record parsing.
- `NativeReceiverProbe::log_batch2_auto_` is called only after
  the existing DLL/CRC verification on **T1, standalone C1 and
  combined-mode C1**. For one candidate it emits
  `BATCH2 AUTO candidate=<driver> source=TPL|ELL|DLL link=T1|C1-A|C1-B dll=... meter=... different_dll=... tpl_security=... (metadata only; no decode or publication)`.
  A fixed ring cache of 32 observed candidates prevents per-frame
  log spam, with no unbounded allocations or per-meter state.
- The new host regression
  `experimental/semtech-radio/test_batch2_auto_identification.cpp`
  compiles with a freshly generated registry made from actual
  `experimental/xmq/drivers/*.xmq` and tests all original
  signatures, a distinct Minomess converter DLL/TPL, ELL
  origin, collisions, ordinary Techem exclusion and malformed
  headers. A CI job also recompiles ESPHome/ESP32-S3.
- No changes to the generated `SUPPORTED` driver manifest,
  to decode `auto`, to `c1_plaintext_publish`, to active YAML,
  firmware installed on Heltec, or to HA sensor bindings.
  **An auto candidate is NOT a decoded/validated meter.** No
  actual Minomess/Kamwater/Apator162 RF packet was observed yet.

CI for this change is **not yet declared green**; initial test
command lacked the native component include directory. Fixed in
commit `b9103a7`, GitHub run `42379840181`, where
`native-packet-input` job passed. Wait for the remaining
ESP32-S3 build jobs before claiming all CI green.

## New manual clear driver — Lansen Pulse Counter (`lansenpu`), 2026-11-07

**Owner requested a simpler meter driver because no Minomess/Kamwater/
Apator162 devices are receivable locally.** Selected the Lansen
two-channel pulse counter for its three original upstream `NOKEY`
telegrams (A/B pulses and TPL status; no AES in the actual source
headers). It is a pulse counter, **not intrinsically a water meter**;
do not label its values m³. Imported original
`wmbusmeters/wmbusmeters:drivers/src/lansenpu.xmq`, upstream file
blob SHA `67d5e60247c7bce0cc288aa09d5449c912824c26` with
original GPL notice intact; original source public IDs only in tests.

`components/wmbus_native_sx126x/lansenpu_plain.h` is a strict,
independent native implementation: LAS/14/00, LAS/1B/00, LAS/0B/02,
short TPL 7A with verified security mode 0, exact
`0E FD 3A` counter A and `8E 40 FD 3A` counter B records,
six-byte/12-decimal BCD, no guessing, no repeated fields,
transactional no-stale-values, original TPL status
`OK`, `POWER_LOW`, and
`PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE` only.
`a_counter` and `b_counter` are **pulse counts**; optional
`status` is a separate text_sensor field (64-byte bounded text).

New `MANUAL_PLAIN = ("lansenpu",)` in
`experimental/xmq/tools/native_driver_specs.py` is intentionally
**excluded from `SUPPORTED` and the decoder AUTO manifest**.
ESPHome `meter_drivers` accepts `lansenpu` without AES key and
registers only its explicit numeric DLL ID. The existing CRC-verified
T1 and C1 callbacks already use `publish_plain_batch2_`, now also
recognizing the explicit Lansen binding and publishing numeric
values, TPL status and RSSI only after complete validation.
All existing Techem and the three previous batch-2 drivers stay
unchanged. `c1_plaintext_publish` remains opt-in.

`experimental/semtech-radio/test_lansenpu_native_no_key.cpp`
runs the three upstream examples expecting
A=4711, B=1234, status=POWER_LOW;
B=870, status=OK; and
B=870, status=PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE.
It independently adds and verifies synthetic DLL format-B CRC
around the public CRC-stripped source telegrams and rejects
invalid ID, AES mode, BCD, unknown TPL status/model, duplicate,
truncated, and malformed frames. A build-only no-key ESP32-S3
YAML fixture includes the two numeric and one text Lansen sensors.

**Latest CI run at code revision `3ade338c`: `42381796061`.**
The `native-packet-input` job is PASS; at time of writing
the standalone ESP-IDF/ESP32-S3 and full ESPHome build are
still IN PROGRESS. Earlier intermediate RED workflows resulted
from an incorrectly updated manifest length guard;
`3ade338c` fixed that. Do NOT call all CI green until the
two remaining jobs finish successfully. No real physical Lansen
telegram/HA reading was observed; no Heltec flash was performed.

## Strict configured AUTO for all four imported drivers — 2026-11-07 (ALL CI GREEN)

Owner authorized enabling **configured-only** `driver: auto` for
`lansenpu`, `minomess`, `kamwater`, `apator162` on Heltec.
The latter three remain excluded from generic `SUPPORTED`;
there is NO change to the existing Techem `fhkvdataiii`/
`mkradio4` AUTO implementation.

Native `xmq_batch2_plain_dispatch.h` selects a candidate using
the COMPLETE original MVT registry, verified DLL sender ID,
unambiguous nested TPL/ELL identity, and exact configured meter
ID. Minomess requires an identified long TPL meter; an optional
explicit `dll_meter_id` may differ from `meter_id` even when
`driver: auto`. Kamwater and Apator162 require an identified DLL
meter matching the configured ID. Any ambiguity/mismatch fails
closed. A chosen model is not proof of plaintext: the unchanged
strict per-driver decoder checks actual TPL/ELL security, CRC,
records and outputs before publishing. The encrypted path still
uses `secure_meters` with explicit driver and configured AES key.
No guessing keys, AES-to-plaintext fallback, phantom RSSI or
unknown-meter sensor publication. Passive read-only recognition
of all four continues for unconfigured verified T1/C1.

CI includes host tests for all four `auto` candidates and
negative tests for wrong ID, converter mismatch, MVT collision,
Minomess Mode-5 and encrypted Kamwater/Apator; compile-only Heltec
T1/C1 test YAML maps four `driver: auto` entries with synthetic
IDs, without private meters/secrets. **Physical RF-to-HA
validation unavailable** for these models; do not flash the
installed Heltec based on CI alone.

**GitHub Actions run `42388899090` at `a4894ee185ae6c9fb0608489c92283d0b351fc28`
ALL GREEN:** `native-packet-input`, `stage-a-espidf-s3-portable-link`,
`native-esphome-build`. The latter compiled actual Heltec
T1/C1 combined laboratory YAML with synthetic `auto` mappings
for all four drivers, in addition to encrypted and no-key ESP32-S3
CI builds. No actual meter, key or physical RF-to-HA validation
was available and no installed firmware was changed.
The subsequent context edits are docs-only.

## Next two native keyless profiles — 2026-11-07 (ALL CI GREEN)

Owner requested one or two additional simple drivers. Verified
upstream `wmbusmeters/wmbusmeters` original XMQ files and their
complete telegram headers; `lansenth` showed Mode-5 despite
`NOKEY`, thus was NOT admitted to the clear receiver.

Selected `lansendw` (two original clear TPL7A Mode-0 examples,
CLOSED/OPEN, event A/B counters and `error_flags`) and
`cma12w` (two original clear TPL7A Mode-0 examples,
temperature, one-hour average, battery and software version).
Independent native strict decoders live under
`components/wmbus_native_sx126x/*_plain.h`, with source XMQ
copyright notices preserved. They are included in portable
`SUPPORTED` / AUTO, independently requiring configured ID,
verified DLL CRC, exact version/manufacturer/type, transport
security Mode-0 and documented record layouts. Bad/unknown
records and security mode reject without HA publication.

Native AUTO uses the same configured no-key handler already
proven for Lansen PU; no RF stack, old legacy driver, AES key,
new external component or unknown-meter publication is added.
Status/battery/software fields are mapped only after a successful
numeric decode. Build-only Heltec YAML includes public original
fixture IDs and sensor bindings. This is GitHub-only integration,
NOT physical RF validation. No hardware changes.

**Last full CI GREEN:** GitHub Actions run `42391722726` at
`a051a46a2f5de2680ced80c5c47420d1f6518a87`, all three jobs
PASS: `native-packet-input`, `stage-a-espidf-s3-portable-link`,
`native-esphome-build` (including combined T1/C1 Heltec build).
New host test `test_lansendw_cma12w_native.cpp` uses the two
public original fixtures for each driver, checking numeric/text
values, configured AUTO, wrong identities, MVT collisions, AES
rejection, malformed input and synthetic DLL CRC verification.
Three previous RED runs were snapshots of intermediary commits:
Stage A's old static manifest guard and an extra quote in a
startup log were corrected before the green run. No real Lansen
DW/CMA12w RF receptions or HA publications are claimed.
Installed Heltec firmware was not modified. Subsequent changes
to these notes are documentation-only.

## New native drivers: Ultraheat + Sontex868 — 2026-11-07 (ALL CI GREEN)

Owner requested up to two additional easy drivers. Both use existing
bounded standard DIF/VIF parser and strict configured-only clear TPL
admission. Import original upstream XMQ definitions, with upstream
GPL-3.0-or-later copyright. No revival of legacy radio or decoder.
- Ultraheat LUG/04/04: original wireless clear short TPL fixture
  supports `heat_kwh`, `volume_m3`, `power_kw`, `flow_m3h`,
  `flow_c`, `return_c`. The original wired M-Bus example and
  ELL/post-decrypted example are NOT admitted as clear RF.
- Sontex868 SON/16/08: original clear short TPL fixture supports
  `current_consumption_hca`, `consumption_at_set_date_hca`,
  `current_temp_c`, `current_room_temp_c`, `max_temp_c`,
  `max_temp_previous_period_c`. The subset does not claim all
  upstream date/status fields.
- Both can be explicitly mapped or configured with `driver: auto`;
  no unconfigured sensor publication; no key guessing, partial
  publication, or relaxed CRC/TPL mode. Synthetic examples added to
  build-only Heltec YAML, host regressions and native CI.
- Apator08 was considered but BOTH original XMQ samples have
  non-BCD radio ID bytes; strict current eight-decimal-digit address
  admission forbids treating them as complete supported RF fixtures.
  No general ID-validation relaxation was performed.
**Latest verified code commit:** `2f518a35c5ce34d56d8c40497755380293a4e4f3`;
**GitHub Actions run `42393848471` ALL THREE PASS**:
`native-packet-input`, `stage-a-espidf-s3-portable-link`,
`native-esphome-build`. The last includes Heltec T1/C1 combined
build-only YAML with two new configured AUTO bindings. Original
Ultraheat and Sontex radio fixtures are validated against numeric
expected fields; Sontex source is a long (176-byte) format-B frame
and test now checks the correct two-block CRC envelope. Earlier
intermediate red runs reflect in-progress fixture/CI corrections,
not the final source. No physical reception or real HA value
publication has been verified for these models. No OTA/flash.

## Next developer workflow

1. Verify the latest Stage B CI, including original fixture/field-gap tests and native ESP32-S3 build. Fix failing tests before asserting increased coverage.
2. Distinguish *positive source-field comparisons*, negative security tests and complete-driver end-to-end parity. Continue the remaining security-aware test coverage without forcing `kamwater #9` to pass.
3. Only after complete per-driver integrity, identity, decryption and publication regression tests consider an **explicitly opted-in** canary build, followed by measured Heltec sensor/log parity and an available rollback.
4. Update this context and `experimental/xmq/BATCH2_STATUS.md` when a validated milestone changes; treat older sections below as archive, not current configuration.

---

## Historical context — 2026-11-01, different branch

The following text was originally written for `experimental/xmq-independent-pipeline` and is retained for traceability. It must **not** override the branch-specific state above.

# esphome-sx1262 — current project context

Updated 2026-11-01. Active research branch: `experimental/xmq-independent-pipeline`.
Known-working hardware reference: `experimental/wmbus-pure-xmq`.
Do not change the installed Heltec firmware, rewrite its private YAML, or merge to master during pipeline development.

## Hardware and observed runtime
Heltec ESP32-S3 / SX1262 / 8 MB, ESPHome 2026.10.1, ESP-IDF 5.5.6.
Owner reports approximately 126,000 seconds continuous runtime on the XMQ-only configuration.
Real T1/C1 frames are received. Seven Techem fhkvdataiii heat allocators and five
Techem mkradio4 water meters are decoded and published to Home Assistant.
The 13th physical water meter is a separate suspected radio/commissioning failure;
do not change firmware merely to address its missing readings.

## Current architecture and exact limitation
The firmware has a generated XMQ/IXML subset and `xmq_only` supports only
`fhkvdataiii` (CI A0) and `mkradio4` (CI A2).
`components/wmbus_meter/wmbus_meter.cpp::Meter::handle_frame` still calls
`meter->handleTelegram(...)` before the authoritative XMQ publication path.
Therefore `xmq_only` is NOT yet a legacy-free pipeline, even if XMQ supplies
the published fields. Legacy sources are still required to build.

## Migration acceptance criteria
1. Preserve byte-for-byte the recorded known-good outputs for all 12 meters:
   water total/target, heat current/previous HCA, temperatures, RSSI.
2. Match configured meter ID and link mode independently; reject unknown IDs
   before any sensor publication. Preserve a separate raw frame analyzer path.
3. Correctly validate frame length/format/CRC; process T1 and C1 as specified.
4. For secured frames, handle supported security modes and keys safely;
   never publish ciphertext or stale values after unsuccessful decryption.
5. Decode fhkvdataiii and mkradio4 from validated payload without calling
   legacy `handleTelegram`, and without depending on legacy `Telegram` fields.
6. Run host regression fixtures, ESPHome compile/link CI and real hardware
   soak tests before changing the installed build or enabling other drivers.
7. Preserve the existing private YAML, credentials, meter IDs, and sensor names.
   Never commit secrets or production meter identifiers to GitHub.

## Implementation order
- First, add recorded-frame regression fixtures and tests covering valid
  T1/C1, malformed CRC/length, unknown IDs, and partial/decrypt failures.
- Then split independent frame validation, meter matching, and security
  preparation into a bounded reusable pipeline; keep legacy untouched.
- Then route `xmq_only` through that pipeline behind a separate opt-in
  configuration and verify parity; maintain a rollback to known-good.
- Finally remove legacy build/link requirements for XMQ-only, extend the
  generated driver catalog, refresh documentation and evaluate release.

## Evidence boundaries
A ~35-hour uptime is encouraging evidence for current workload stability,
not proof of every radio/security error path or a fully independent decoder.
Previous `NATIVE_ESPIDF_REPORT.md` covers the earlier native-espidf migration,
not final XMQ independence.
