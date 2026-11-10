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

## Vario451: XMQ hex-IXML compilat în firmware (2026-11-10)

- `generate_vario451_ixml.py` parcurge gramatica IXML originală
  `vario451.xmq` (`hex` reprezintă câte un nibble), derivă pozițiile
  `DV_PREV` și `DV_CURR`, verifică mapările `027E/027F` și
  formulele XMQ pentru conversia din mGJ în kWh.
- Generează `xmq_generated_vario451.h` pentru decodorul C++ nativ
  `vario451_plain.h`. Limitele de securitate, modelul admis
  `TCH/27/C3`, antetul CI=A2, ID-ul configurat, verificarea CRC
  externă și layoutul radio rămân stricte. Modelul `TCH/27/04`
  nu devine publicabil fără fixture radio verificat.
- Fixture-ul original verifică toate cele trei valori calculate;
  build-only CI include și teste negative. Nu se fac flashuri.

# Native XMQ wM-Bus on official ESPHome SX126x

**Current branch:** `experimental/xmq-multidriver-auto` (native-only,
November 2026). This branch no longer packages the legacy
`wmbus_common`, `wmbus_meter`, `wmbus_radio` or
`socket_transmitter` components.

## Active sources

- `components/sx126x/`: patched official ESPHome SX126x T1/C1 streaming.
- `components/wmbus_native_sx126x/`: bounded native frame receiver,
  CRC/integrity gating, configured meter binding, strict XMQ AUTO and
  explicit decoding, application security and Home Assistant sensor output.
- `experimental/xmq/drivers/`: 52 upstream-anchored XMQ definitions.
- `experimental/xmq/tools/native_driver_specs.py`: explicit native
  decoder manifest. `generate_ixml_cpp.py` and
  `build_native_generated.py` produce the compile-time descriptors.
- `experimental/xmq/tools/generate_clear_profiles.py`: fixture-checked
  compile-time model and DIF/VIF record-key tables for munia, cma12w,
  lansendw and lansenpu. Their native security, numeric and status
  validation is retained; this is a bounded compiler, not full XMQ.
- `experimental/xmq/portable_cpp/`: portable XMQ records/math/date
  implementation and **offline** comparison catalog/parity helpers.
- `experimental/xmq/legacy_reference/`: 15 isolated, header-only
  historical `wmbus_pure` reference implementations for host tests.
  **Not part of the deployed SX126x native firmware.**
- `experimental/semtech-radio/`: native RF/host fixture tests,
  build-only Heltec YAML and CI guards.
- `experimental/xmq/espidf_cpp_probe/`: native ESP-IDF link/portability
  validation. It does not register the old wM-Bus radio component.

## Build/test contract

Use `.github/workflows/native-sx126x-adapter.yml` as the authoritative
continuous-integration workflow. The Heltec lab YAMLs import only
`sx126x` and `wmbus_native_sx126x`, never the old radio. CI also
validates fixture parity, test-only portable legacy references, the
security catalog and ESP-IDF/ESPHome builds.

Native receiver publication requires a configured meter identity,
validated packet/frame integrity and exact supported driver/security
profile. `driver: auto` is constrained by the same identity and
security rules as explicit drivers. `NOKEY` in an upstream fixture does
**not** prove an RF telegram is unencrypted.

These checks do not establish live RF parity, so do not flash experimental
lab firmware without separate radio/board verification. Real meter IDs,
cryptographic keys and HA API credentials must remain in private YAML
secrets rather than committed public configuration.

## Historical notes

Older XMQ shadow, `handleTelegram` and custom transceiver documentation
refer to retired branches, not to this native-only branch. Git history and
`docs/history/` preserve migration context. See top-level `README.md`
and `PROJECT_CONTEXT.md` for the driver batch chronology.
