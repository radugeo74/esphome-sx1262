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

# Native wM-Bus / XMQ on ESPHome SX126x

**Experimental Heltec ESP32-S3 + SX1262 receiver** for wireless M-Bus
T1 and C1, with a native C++ parsing/decoding pipeline and configurable
Home Assistant sensors.

**Active branch:** `experimental/xmq-multidriver-auto`  
**Last fully green code CI — secure-delivery audit point 4:** [GitHub Actions #42459485622](https://github.com/radugeo74/esphome-sx1262/actions/runs/42459485622)
(code commit `1efb2ea455d645e193f09f33550a06f0235e1778`, all three jobs passed).

> This branch is a distinct native implementation. It **does not use
> `wmbus_radio` / `wmbus_meter` or the old
> `Meter::handleTelegram()` pipeline as its live receiver/decoder**.
> Earlier experiments in this repository used different components; do
> not copy their YAML into this branch. The current architecture and
> safety rules are recorded in [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).

## Catalog de securitate (audit punctul 5)

[**Catalogul wM-Bus / OMS**](experimental/xmq/SECURITY_PROFILE_CATALOG.md)
descrie pentru toate cele 16 drivere MVT-urile, fixture-urile XMQ
si rutele clare/securizate implementate, fara sa presupuna ca
`NOKEY` inseamna RF necriptat. Datele structurate sunt in
[`security_profile_catalog.json`](experimental/xmq/security_profile_catalog.json).
CI verifica automat catalogul fata de manifest si sursele XMQ.
Nu atribuim certificate OMS pana nu este identificat modelul comercial exact.
Documentatie si CI numai; nu afecteaza decoderul sau firmware-ul.

### Audit punctul 6: siguranta `NOKEY` versus RF (2026-11-08)

Dintre 74 de fixture XMQ, 71 sunt `NOKEY`, insa **31 dintre
acestea au antet TPL AES Mode 5**. Aparitia `NOKEY` sau a
markerului `2F2F` nu permite decodarea fara cheie.
[Testul de provenienta](experimental/xmq/tests/test_nokey_fixture_provenance.py)
si [testul de securitate RF](experimental/semtech-radio/test_nokey_rf_fail_closed.cpp)
ruleaza in CI: exemplele deja lizibile din Q400/Minomess/Topas/
MK Radio4a nu pot ocoli `publication::inspect` sau
`batch2_plain::decode_configured`. Un iPERL Mode 0 valid ramane
acceptat; un Mode 0 cu N nenul este respins.

**Descoperire prin regresie:** vechiul `iperl::decode()` verifica
numai 4 biti ai modului, fara N. Desi publicarea normala era
blocata deja de filtrul receptorului, apelul direct putea decoda
un fixture `NOKEY` cu M=0,N=1. Acum si decodorul iPERL
aplica filtrul OMS complet, iar `xmq_decode_catalog.h` repeta
admiterea de publicare. Datele deja lizibile ale fixture-ului
sunt testate separat prin `decode_clear_application()`, fara
sa fie prezentate ca un cadru RF necriptat.
Vezi [catalogul de dovezi](experimental/xmq/SECURITY_PROFILE_CATALOG.md).
Nu au fost schimbate decodoarele, cheile ori firmware-ul instalat.

### Audit 7 — RSSI SX1262 (2026-11-08)

**CI source-fidelity fix (2026-11-08):** the Heltec T1 lab YAML
used to clone the old `experimental/semtech-sx126x-sx127x` branch,
which did **not** exercise changes in the active RSSI code branch.
T1, combined T1/C1, and diagnostic lab YAMLs now compile the **local
checkout** `../../components`, identical to the GitHub Actions
commit under test. A fast CI guard
[`test_heltec_lab_source_ref.py`](experimental/semtech-radio/ci/test_heltec_lab_source_ref.py)
rejects future remote-branch regressions. These build-only YAMLs must
be run from inside this repository, not copied without their
`../../components` dependency.

At commit `57232901`, ESPHome compilation remained blocked by
GitHub release assets returning HTTP 504 during ESP-IDF setup, not by
C++ compilation. Tracked in [issue #1](https://github.com/radugeo74/esphome-sx1262/issues/1).
The RSSI change is not considered fully green until this new
exact-source CI build completes. No firmware was flashed.

**CI download hardening (2026-11-08):** pinned ESPHome 2026.10.1
supports `ESPHOME_IDF_FRAMEWORK_MIRRORS`. The build now tries its
original `esphome-libs` ESP-IDF 5.5.6 tar.xz first, then the
**official Espressif-hosted** 5.5.6 release ZIP as a distinct CDN
fallback. `IDF_GITHUB_ASSETS=dl.espressif.com/github_assets` routes
ESP-IDF compiler/ROM tool archives through Espressif's mirror.
The pinned local tool cache `~/.cache/esphome/idf` is restored and
saved by `actions/cache@v4` after a successful build. A host CI
contract test asserts these URLs and both versions to guard future
changes. No framework version or sensor/decoder behavior was changed.
**The fallback requires a new successful full ESPHome build before
this audit point can be called green.**


For **GFSK GetPacketStatus (0x14)** the three bytes are
`RxStatus`, **`RssiSync`** and **`RssiAvg`**. Their scale
is `-raw/2.0 dBm`. Our progressive T1 and combined T1/C1 receive
path collects a full FIFO frame **before** a reliable `RxDone`;
therefore the HA RSSI source is **`RssiSync` (byte [1])**, latched
at synchronization. The packet average (byte [2]) is reliable only
after packet completion and is diagnostic-only here. The normal
upstream FSK `RxDone` path continues using the packet average.

The single source of truth is
[`gfsk_rssi.h`](components/sx126x/gfsk_rssi.h), used **by the actual
radio firmware** and [the host test](experimental/semtech-radio/test_sx126x_gfsk_rssi.cpp).
[The integration guard](experimental/semtech-radio/ci/test_gfsk_rssi_source_contract.py)
ensures that the same sync value is forwarded to the native receiver
and mapped HA sensors. When `t1_raw_diagnostics: true`, the local
log prints bounded `sync_raw`, `avg_raw`, both converted dBm and
the `packet_received` status; no meter IDs or AES keys are logged
by this specific diagnostic. **No artificial RSSI offset, smoothing,
or guessing from the old RTL receiver is introduced.**

**Status:** software / build validation only. The historically
reported approximately -114 dBm may come from packet average or from
the firmware previously installed on the Heltec. A valid sync RSSI
still can differ from the older RTL2832U receiver due to hardware
and RF frontend. We cannot claim the physical discrepancy is resolved
until actual RF on the same device is observed. Never flash
without preserving the existing stable firmware.

## Architecture

- **Radio:** ESPHome's SX126x component, locally patched for T1/C1
  progressive FIFO reception; target board is Heltec WiFi LoRa 32 V3
  (ESP32-S3 / SX1262).
- **wM-Bus:** `components/wmbus_native_sx126x` handles radio-to-DLL
  framing, CRC verification, T1/C1 routing, meter identity, security
  checks, driver selection and mapped sensor publication.
- **Decoding:** native C++ implementations and selected generated XMQ
  rules, checked against public original `wmbusmeters` XMQ fixtures.
  The old `wmbusmeters` radio/decoder stack is **not an active
  dependency** of this receiver.
- **Home Assistant:** only explicitly configured meter IDs and mapped
  `xmq_sensors`, `xmq_text_sensors` or `rssi_sensors` can publish
  values. Unknown meters may be recognized in diagnostics, but do not
  cause entities or readings to be created.

### Audit point 4: verified T1/C1 to mapped sensor delivery (2026-11-08)

Native encrypted publication now uses the shared, host-testable
`components/wmbus_native_sx126x/secure_meter_delivery.h` from the
**actual** `NativeReceiverProbe::publish_secure_batch2_()`. It
performs the same configured sender binding, native AES-driver decode,
result validation, and mapping callback invocation as the firmware;
the receiver connects these callbacks to explicit `xmq_sensors`,
`xmq_text_sensors`, and `rssi_sensors` entities. When a configured
secure sender is recognized but decoding fails, its frame remains
**consumed and rejected**, with **zero** numeric/date/status/RSSI
updates and no fallback to plaintext/AUTO. Unconfigured senders
produce no HA updates. Recognition alone never reaches this
delivery gate.

The new `test_secure_rx_to_mapped_sensors.cpp` feeds packets through
the production `verified::process` path, covering **T1 3-of-6 plus
format-A DLL CRC, C1-A CRC and C1-B CRC**, configured meter binding,
the real mbedTLS native decoder and the shared mapped-output callbacks
with simulated HA sensor objects. Positive cases include an original
public encrypted `apator162` frame (`total_m3=7.512`), an
independently re-encrypted public `topaseskr` application
(`total_m3=481.899`), and an independently encrypted public
`mkradio4a` converter/TPL frame (`target_m3=16.1`,
`target_date=2021-12-31`). Test keys are public/synthetic.
Negative cases include bad DLL CRC, wrong AES key, wrong meter identity,
wrong model/security mode, no configured binding, and upstream
already-decrypted `NOKEY` material presented as on-air ciphertext.
The structural Stage-A regression now checks the shared header is
**actually wired into the receiver**, not just a host-only duplicate.

**Limitations:** this is host integration and ESP-IDF/ESPHome
*build-only* validation, not RF/IRQ/FIFO operation on physical
Heltec hardware and not an actual Home Assistant API assertion.
The existing clear-profile T1/C1 fixture tests remain separate.
No real meter IDs, keys or private RF captures were added.
No change to `meter_drivers`, key selection, original AES algorithms
or installed Heltec firmware. Genuine hardware/HA field acceptance
remains a separate deployment decision.

**CI 3/3 PASS:** [#42459485622](https://github.com/radugeo74/esphome-sx1262/actions/runs/42459485622) — native packet/CRC and mapped output host tests, ESP-IDF S3 link, ESPHome Heltec builds including combined T1/C1.

### OMS transport-security audit fix (2026-11-08)

Read-only audit item 2 identified two independent failures in
`components/wmbus_native_sx126x/tpl_security_probe.h`: it extracted
only the low **four** security-mode bits (instead of OMS **five**,
CF bits 8..12), and it accepted Mode 0 even when CF bits 4..7
declared nonzero encrypted blocks. Per [OMS Vol. 2 §7.2.4.2
and Table 19](https://oms-group.org/wp-content/uploads/2024/10/OMS-Spec_Vol2_Primary_v501_01.pdf),
**clear transport requires M=0 and N=0**. A nonzero N with M=0
is inconsistent and must stay `Unknown`/fail-closed; a fifth
mode bit must never alias reserved Mode 16 to clear Mode 0.

The native probe now reads M with `(CF >> 8) & 0x1F` and N with
`(CF >> 4) & 0x0F`. Known encrypted modes 4 (legacy compatibility),
5, 7, 10 and 13 are flagged as encrypted, while unsupported modes
remain `Unknown`. Existing valid Mode-0 frames with N=0 and
allowed link-control flags still pass. The publication gate continues
to block anything other than `NotEncrypted`.

Positive/negative regressions now cover both `CI=7A` short TPL and
`CI=72` long TPL in `test_tpl_security_probe.cpp` and
`test_native_publication_gate.cpp`.
The Minomess and Hydrus host tests now construct valid synthetic Mode-0
frames by clearing **both** M and N; upstream encrypted XMQ fixtures
remain unchanged. CI #42456075410 passed all three jobs (host tests,
ESP-IDF ESP32-S3 link, and ESPHome Heltec builds). This change does **not**
introduce new AES decoding, bypass existing secure bindings,
alter the manufacturer's clear-specific A0/A2 decoding, or
change any installed Heltec firmware.

### Secure-meter admission audit fix (2026-11-08)

The `secure_meters` YAML schema already accepted all five keyed native
routes, but the old C++ `NativeReceiverProbe::add_secure_meter()` rejected
`mkradio4a` and `topaseskr` at runtime. This made successfully
compiled configurations silently drop those two bindings.

The receiver now delegates registration to the host-tested
`secure_meter_binding.h`, whose explicit allowlist covers
`minomess`, `kamwater`, `apator162`, `mkradio4a` and
`topaseskr`. The DLL/TPL rule remains unchanged: only
`minomess` and `mkradio4a` may bind a distinct `dll_meter_id`.
Registration still rejects invalid decimal IDs, invalid AES hex keys
and duplicate bindings; keys remain private and AUTO/cleartext
fallback remains forbidden for encrypted packets.

`test_secure_meter_binding.cpp` executes the **same** admission
function that the native receiver calls. The native manifest test
also compares the Python YAML and C++ secure-driver allowlists to
prevent this mismatch recurring. The ESPHome CI fixture now includes
synthetic bindings for all five AES routes. This is software-only,
not a claim of successful live RF/HA delivery.

### Audit point 3: AUTO recognition is not AES decryption (2026-11-08)

**Passive AUTO recognition is available for all 16 native driver families**
on independently DLL-CRC-verified T1/C1 packets. The log label is
`NATIVE AUTO` and includes `candidate`, `source=DLL/TPL/ELL`,
`tpl_security`, `tpl_mode`, `driver_class`, and
`decode_status=not_attempted`. The last field means exactly that:
recognition alone did not call a decoder, try an AES key or publish any
Home Assistant values. A verified unknown meter remains diagnostic-only.

**Configured decoding is a separate capability:** 14 drivers remain
eligible for `meter_drivers: driver: auto` (or an explicit driver)
**only for supported clear RF profiles**. The remaining two,
`mkradio4a` and `topaseskr`, remain `SECURE_ONLY`:
encrypted Mode-5 RF requires an explicit `secure_meters` entry, its
individual AES key and a successful application decoder. Neither is
admitted as a plaintext AUTO decoder. A matching MVT is not a key and
the upstream `NOKEY` fixtures do not permit ciphertext fallback.

**Nested MK Radio4a identity:** the original XMQ warm-water profile
contains `TCH/95/37` converter DLL and `HYD/FE/06` meter TPL;
the cold-water profile contains `TCH/95/37` and `WEH/FE/07`.
The latter is validated by the original upstream fixture but missing
from `mkradio4a.xmq`'s `detect.mvt`: the passive recognizer accepts
only that **exact converter + nested TPL pair**. It does **not**
alter the generated global MVT registry, YAML schema, decoder or
security/admission requirements. A TCH converter on its own, or a
conflicting/unknown nested TPL model, is not identified as a confirmed
MK Radio4a meter. `topaseskr` is identified passively for both
documented `AMT/F1/06` and `AMT/F1/07` signatures.

The actual receiver uses the host-tested `passive_auto_identify.h`
helper, covering all 16 families and the converter-only rejection.
Regression tests `test_passive_auto_identification.cpp`,
`test_batch2_auto_identification.cpp`, `test_xmq_auto_registry.cpp`
and `test_batch2_native_catalog_dispatch.cpp` separate
metadata recognition, decoder readiness, ambiguity and plaintext
refusal. No changes were made to the AES implementation, actual meter
keys, active Heltec firmware or configured HA devices.

**CI full green:** [#42457899531](https://github.com/radugeo74/esphome-sx1262/actions/runs/42457899531)
(native packet tests, ESP-IDF ESP32-S3 link, ESPHome Heltec T1/C1
builds). The final passive helper also rejects an isolated HYD MVT
without the required TCH converter and forbids generic DLL fallback
when a different nested TPL address is present.

### Supported driver selection

| Driver | `driver: auto` for a configured meter | Explicit `meter_drivers` |
| --- | --- | --- |
| `fhkvdataiii` (Techem heat) | Yes | Yes |
| `mkradio4` (Techem water) | Yes | Yes |
| `iperl` | Yes (supported clear payloads) | Yes |
| `hydrus` | Yes (supported clear payloads) | Yes |
| `lansenpu` (pulse channels A/B) | Yes | Yes |
| `lansendw` (door/window + pulse events) | Yes | Yes |
| `cma12w` (room temperature, battery) | Yes | Yes |
| `ultraheat` (heat energy, flow, temperature) | Yes, verified clear short TPL | Yes |
| `sontex868` (heat allocation and temperature) | Yes, verified clear short TPL | Yes |
| `minomess` | Yes, clear transport only | Yes |
| `kamwater` | Yes, clear transport only | Yes |
| `supercal` (Sontex Supercal 739) | Yes, clear CI=7A subset | Yes |
| `q400` (Axioma Q400) | Yes, clear CI=7A subset only | Yes |
| `apator162` | Yes, clear transport only | Yes |

The new `ultraheat` and `sontex868` decoders currently cover the
publicly verified **unencrypted short-TPL numeric subset** only.
Ultraheat provides `heat_kwh`, `volume_m3`, `power_kw`,
`flow_m3h`, `flow_c`, `return_c`; Sontex868 provides
`current_consumption_hca`, `consumption_at_set_date_hca`,
`current_temp_c`, `current_room_temp_c`, `max_temp_c`,
`max_temp_previous_period_c`. Do not infer that other upstream
fields, wired M-Bus frames or post-decrypted ELL frames are supported.
Original source fields and strict negative-frame tests passed
[run #42393848471](https://github.com/radugeo74/esphome-sx1262/actions/runs/42393848471).

### New clear-only profiles (2026-11-07; previous code CI green)

`supercal` (SON/1B/04) has one original public clear short-TPL
fixture and publishes `total_kwh`, `total_m3`, `power_kw`,
`volume_flow_m3h`, `flow_temperature_c`, and
`return_temperature_c`. Target/storage-20 dates and values are
not yet included in this native subset.

`q400` (AXI/01/07, AXI/10/07, AXI/20/07) publishes strict
clear short-TPL `total_m3` and optional `volume_flow_m3h`,
`flow_temperature_c`, `battery_pct`. Of five original
public XMQ cases, three have unencrypted TPL and two are
Mode-5 `NOKEY` examples with post-decrypted application records.
The latter are **not** authorized for the no-key decoder.
Other Q400 historical, error, forward/backward fields are outside
this first native subset.

`topaseskr` (Integra AMT/F1/06 or 07) is additionally wired as
**explicit-key `SECURE_ONLY`**. Both public `NOKEY` examples have
already-decrypted Mode-5 applications, not original ciphertext.
The native receiver requires a matched DLL meter ID, AES-128 key,
Mode-5 short TPL with a complete 4-block CBC payload, the decrypted
`2F2F` structural prefix, and independently validated DIF/VIF
records. It publishes a deliberately narrow numeric subset:
`total_m3`, `temperature_c`, `current_flow_m3h`, and optional
`volume_year_period_m3`. Other upstream Topas history/date/battery
fields are not yet published. CBC/`2F2F` does **not** authenticate a
telegram. The source applications are re-encrypted with synthetic
keys for positive/negative CI; real on-air ciphertext and Home
Assistant operation are still unverified.

```yaml
wmbus_native_sx126x:
  # ...sx126x_id and radio options from your complete YAML...
  secure_meters:
    - meter_id: "12345678"  # synthetic configured DLL meter ID
      driver: topaseskr
      key: !secret example_topaseskr_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example Topas total"
        unit_of_measurement: "m³"
```

The `topaseskr` **decoding** route is **never** part of `meter_drivers`,
plaintext decoding or `driver: auto`; metadata-only recognition is separate.

### Topas NOKEY fixtures: keyless *offline* decode

The two original `topaseskr.xmq` examples supplied by upstream are
**already-readable EN 13757-3 DIF/VIF application bytes**. They can be
decoded **without supplying any AES key**, despite their embedded
`7A ... 40 05 2F 2F` short-TPL header (which declares Mode 5,
four AES-CBC blocks). The source's `NOKEY` label does not show
whether their source on-air packets were clear or decrypted before
publication.

A host-only reference adapter
`experimental/semtech-radio/host_only/topaseskr_supplied_clear_fixture.h`
accepts these **explicitly supplied already-clear offline samples**.
`experimental/semtech-radio/test_topaseskr_supplied_clear.cpp`
checks the original numeric outputs and malformed/header/identity
rejections with **no AES key**. It is not imported into the Heltec
receiver or exposed as a Home Assistant publication path.

For **live RF**, the existing `secure_meters` Mode-5 AES implementation
and explicit key remain mandatory when a packet advertises
`0x0540`. A `2F2F` marker alone does not prove decryption or
authorize bypassing the TPL security gate. Keyless live
`topaseskr` AUTO/`meter_drivers` support is **not** claimed without
evidence of a genuine unencrypted radio profile.



These new additions are software-only, not tested with live RF or
Home Assistant. Keep the last *fully green* CI marker above until
the new jobs complete.

### MK Radio4a NOKEY examples also decode without AES (offline)

Just as for `topaseskr`, both original `mkradio4a.xmq`
examples (`FOO` warm water and `BAR` cold water) contain
**already-readable storage-one DIF/VIF values** despite their
Mode-5 `0x0530` three-block CBC TPL and radio-converter
headers. The existing `mkradio4a_plain.h` parser has decoded
them without a key; the new **host-only** supplied-clear adapter
`experimental/semtech-radio/host_only/mkradio4a_supplied_clear_fixture.h`
additionally validates **both** DLL radio-converter and nested
TPL meter IDs, model/version, the `2F2F` fixture marker,
and complete target volume/date fields.

The regression `test_mkradio4a_supplied_clear.cpp` decodes both
original public examples **without a key**, checks their expected
`target_m3` (16.1/75.2) and `target_date` (2021-12-31),
and rejects wrong DLL/TPL IDs, malformed data, and unsupported
envelopes. This adapter is **not included in the live Heltec**
receiver, not a keyless `driver: auto` authorization.

The original examples may have been decrypted before being saved:
`NOKEY` does **not** establish unencrypted radio transmission.
For live `0x0530` Mode-5 RF, the configured `secure_meters`
route still requires the corresponding AES key and both IDs.
No OTA/flash or physical RF/HA test was performed.

### Encrypted-only driver: `mkradio4a`

`mkradio4a` is now additionally software-integrated through
**explicit `secure_meters` only**. The public original XMQ
examples use Techem `TCH/95/37` radio converters and nested
`HYD/FE/06` (warm) or `WEH/FE/07` (cold) meter headers. The
receiver must match **both configured IDs**, validate DLL CRC,
decrypt exactly three AES-CBC Mode-5 blocks with the explicit AES-128
key, then validate the original application before publishing
`target_m3` and `target_date` (ISO `YYYY-MM-DD`).

```yaml
wmbus_native_sx126x:
  # ...sx126x_id and radio options from your complete YAML...
  secure_meters:
    - meter_id: "12345678"      # synthetic nested TPL ID
      dll_meter_id: "87654321"  # synthetic radio converter DLL ID
      driver: mkradio4a
      key: !secret example_mkradio4a_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: target_m3
      sensor:
        name: "Example MK Radio 4a target"
        unit_of_measurement: "m³"
  xmq_text_sensors:
    - meter_id: "12345678"
      field: target_date
      sensor:
        name: "Example MK Radio 4a target date"
```

Use **actual corresponding** meter/convertor IDs and the correct
private AES key in your own YAML; these values are illustrative.
The upstream `NOKEY` samples contain post-decrypted application
records, not verified on-air ciphertext. Regression tests use
synthetic AES encryption of both original sample applications; there
is **no real MK Radio 4a RF/HA validation**. All three new-driver CI jobs passed [run #42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133). AES-CBC and the
`2F 2F` marker are **not cryptographic authentication**.
`mkradio4a` is deliberately **not allowed in plaintext
`meter_drivers` or `driver: auto`**.

### Software support and acceptance status

**The table lists currently configured software driver candidates; the newly added clear-only Q400/Supercal jobs must pass CI before being counted as fully verified**, but only
the Techem baseline has been observed with real nearby meters.
The active driver manifest defines:

- `SUPPORTED`: generic native compiled/DLL AUTO drivers.
- `CONFIGURED_NATIVE = ("minomess", "kamwater", "apator162")`:
  software-tested native drivers with strict **configured-only** AUTO.
- `CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE`:
  the 14 names allowed in YAML `meter_drivers`.
- `SECURE_ONLY = ("mkradio4a", "topaseskr")`: 2 additional
  explicit-key drivers (16 software-integrated drivers in total). MK Radio4a
  requires separate DLL/TPL identities; Topas binds its DLL ID directly.

The old `BATCH2_PENDING` category was retired after software
integration. The three models are **not pending software implementation**;
they are omitted from generic `SUPPORTED` only because their
DLL/TPL/ELL identity/security admission requires a separate native
dispatcher. This restriction does not block `driver: auto` or an
explicit driver on a configured meter.

A matched manufacturer/model never proves the payload is clear,
valid or authenticated. **Physical RF-to-Home Assistant validation
is still open** for the new driver families.

## YAML: automatic or explicit driver selection

For a **configured meter**, both forms are supported. Use an
eight-digit, decimal-string meter ID (including leading zeroes). The
examples use **synthetic IDs only**.

**Option A — automatically identify the driver:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: auto
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example meter total"
        unit_of_measurement: "m³"
```

The firmware selects a driver from the verified telegram metadata,
then the chosen decoder independently checks transport, meter identity,
security and field validity. No matching/valid frame means **no
sensor update**.

**Option B — specify the driver yourself:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: kamwater
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example Kamwater total"
        unit_of_measurement: "m³"
```

In both examples, `sx126x_id` must reference a correctly configured
`sx126x:` radio in the **same full YAML**. These snippets demonstrate
driver selection; they are not standalone firmware configurations.
Map only fields supported by the selected meter and driver.

**Minomess behind a radio converter:** when the radio's DLL sender ID
differs from the actual meter ID in the long TPL address, configure
both explicitly. This works with `driver: auto` or `driver: minomess`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  meter_drivers:
    - meter_id: "12345678"      # meter identity in TPL
      dll_meter_id: "87654321"  # sender identity in DLL
      driver: auto              # or: minomess
```

The two identities must match their respective headers; a wrong or
ambiguous match blocks decoding. A distinct `dll_meter_id` is not a
general gateway auto-discovery feature.

### Encryption and discovery boundaries

**`driver: auto` does not guess AES keys or decrypt encrypted
telegrams.** For encrypted meters, use an explicit driver and the
appropriate AES-128 key in `secure_meters`, usually via `!secret`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  secure_meters:
    - meter_id: "12345678"
      driver: minomess
      key: !secret example_minomess_aes_key
```

`secure_meters` supports the separately reviewed keyed paths for
`minomess`, `kamwater`, `apator162` and `mkradio4a`; it **requires an explicit
driver**, not `auto`. A telegram that claims encryption but lacks a
configured valid key must not be published as plaintext. Failed
integrity, identity, payload or cryptographic checks do not publish
partial, stale or synthetic values. C1 plaintext sensor publication
additionally requires the opt-in `c1_plaintext_publish: true`.

CRC-verified T1/C1 frames from **unconfigured** meters can be
recognized passively, including Minomess, Kamwater, Apator162 and
Lansen, using original XMQ manufacturer/version/type signatures.
Diagnostic `BATCH2 AUTO candidate=...` messages do **not** decode,
create entities, or publish meter values.

### New cleartext drivers: Lansen DW and Elvaco CMA12w

The native pipeline also supports `lansendw` (Lansen
door/window, `a_counter` / `b_counter`, text `status` OPEN/CLOSED,
`error_flags`) and `cma12w` (Elvaco temperature,
`current_temperature_c`, `average_temperature_1h_c`,
text `battery`, `software_version`, `status`).
Both accept `driver: auto` or the explicit driver name on
a *configured* eight-digit meter ID. They use strict mode-zero
TPL frames and four original upstream `NOKEY` examples. The
new decoders reject wrong meter/model, encrypted modes and invalid
records rather than publishing partial sensor updates.
`lansenth` is **not** enabled: its upstream NOKEY examples
carry encrypted TPL Mode 5 and must not be misclassified as clear RF.

## Building and testing

The branch includes a **build-only** Heltec T1/C1 laboratory YAML at
[experimental/semtech-radio/heltec-sx126x-combined-lab.yaml](experimental/semtech-radio/heltec-sx126x-combined-lab.yaml).
It uses synthetic or public upstream example IDs and exercises
`driver: auto` for the eight newer drivers (including Ultraheat and Sontex868). Check the board revision and real radio pin/TCXO mapping
before adapting it to your own hardware.

GitHub Actions run [#42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133)
passed `native-packet-input`,
`stage-a-espidf-s3-portable-link` and `native-esphome-build`.
This establishes host regression coverage and ESP32-S3/Heltec
**compilation**, not reception or correct HA publication from real
Minomess/Kamwater/Apator162/Lansen/Elvaco/Ultraheat/Sontex devices. The known-working Techem
behavior has been observed separately on a Heltec, but does not
validate every newly added driver.

**Do not flash the existing production Heltec solely on the strength
of CI.** Preserve a working firmware backup and test any new build
with real reception before declaring field readiness. Do not commit
private meter IDs, encryption keys, Wi-Fi secrets or production
telegrams.

For implementation notes, start with
[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md),
[experimental/xmq/README.md](experimental/xmq/README.md),
[experimental/xmq/STAGE_B_GATE_STATUS.md](experimental/xmq/STAGE_B_GATE_STATUS.md),
[experimental/xmq/BATCH2_STATUS.md](experimental/xmq/BATCH2_STATUS.md)
and the [CI workflow](.github/workflows/native-sx126x-adapter.yml).

## Project origin, credits and licensing

This **current native SX126x + XMQ architecture is not a continuation
of the old `SzczepanLeon/esphome-components` runtime**. That project
is part of the **historical development lineage** of earlier wM-Bus
experiments in this repository; it is not the active radio/decoding
component described above.

The active project uses the ESPHome SX126x component with local
modifications, plus native implementation work in this repository.
Original XMQ driver specifications and public fixture expectations
come from [wmbusmeters](https://github.com/wmbusmeters/wmbusmeters)
and retain their upstream copyright/licence notices. Historical
SzczepanLeon contributions and any other imported source still
retain the attribution and licensing applicable to those files:
architecture changes do **not** erase the origin of reused code.
Consult the individual file headers before reuse or redistribution.

For earlier development and the former relationship with
[SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components),
see the [archived historical README](docs/history/xmq-independent-pipeline-README.md).
