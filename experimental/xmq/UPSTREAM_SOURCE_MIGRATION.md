## Calupul XMQ 3: Lansen Pulse și Sontex Supercom587 (11 noiembrie 2026)

`lansenpu` și `supercom587` sunt migrate din implementări
de decodare independente în **compilatorul XMQ comun**, prin backendul
`generate_xmq_next_profiles.py` folosit de
`generate_xmq_full_profiles.py`, apoi compilate în același
`xmq_generated_full_profiles.h`.

Interpretul `xmq_full_profile_runtime.h` reutilizează aceleași
operații comune ca la CMA12w, LansenDW, iPERL și Munia:
`Bcd12` pentru numărarea impulsurilor, `Bcd8` pentru volum
și `BcdVersion6` pentru versiunea software. Adaptatoarele
`lansenpu_plain.h` și `supercom587_plain.h` nu mai păstrează
bucle proprii de interpretare a înregistrărilor, filtre
DIF/VIF sau conversii BCD.

- **Lansen Pulse:** `LAS/14/00` și `LAS/0B/02` (dintre
  variantele sursă efectiv documentate), prefixul `2F2F`,
  contoare opționale `a_counter`/`b_counter`, modelele,
  configurațiile și cele trei stări `POWER_LOW`, `OK` și
  `PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE` provin din
  cele trei fixture-uri XMQ. `LAS/1B/00` este identificat în
  XMQ, dar rămâne fără autorizare RF până la un fixture validat.
- **Supercom587:** `SON/3C/06` și `SON/3C/07` din cele două
  fixture-uri clear, consum `total_m3`, versiune software
  `010002`, status `OK`. Sunt generate din XMQ regulile
  pentru înregistrări, configurațiile, pozițiile standard și
  marcajele auxiliare dovedite de exemple. Restul
  înregistrărilor sunt opace; **nu este revendicat suport
  semantic pentru câmpurile care nu apar în JSON-ul original**.

Noile forme ale interpretului comun păstrează cerința de CRC DLL
validat înaintea decodării, ID configurat, verificarea securității
TPL fără cheie, respingerea statusurilor și cadrelor nevalidate.
Modificările upstream se compară, se compilează și se verifică
cu fixture-urile sursă; **nu există update automat pe Heltec**.
Aceste migrări sunt complete doar pentru profilele on-air
necriptate documentate, nu pentru forme noi sau AES.

## Migrare XMQ completă pentru profilele radio documentate — iPERL + Munia (2026-11-10)

Al doilea calup de două familii este `iperl` și `munia`.
Am extins **același** compilator
`generate_xmq_full_profiles.py` și același interpret
`xmq_full_profile_runtime.h` folosite deja de
`cma12w` și `lansendw`. Compilatorul verifică
`detect.mvt`, selectorii `match.measurement_type`,
`match.vif_range`, `difvifkey`, unitățile și
`status`/lookup-urile XMQ, plus întregul JSON original.
Produce tabelele native în `xmq_generated_full_profiles.h`.

Cele două decodoare `iperl_plain.h` și `munia_plain.h`
au devenit adaptoare scurte către interpretul comun:
nicio parcurgere proprie a înregistrărilor pentru traficul RF.
Au fost extinse operațiile standard `Unsigned32` și `Bcd4`,
cu scalarea din tipurile câmpurilor; numele de măsurători
și mesajele de stare sunt din profilele generate.
`iperl::decode_clear_application` rămâne **doar**
o funcție compatibilă de testare offline a unor date deja
decriptate, nicidecum o cale de publicare RF.

**iPERL:** `SEN/68/07` cu exemplarele wireless reale
necriptate `33225544`, `total_m3=123.529`,
`max_flow_m3h=0`, stările `OK` și
`ERROR POWER_LOW`. Primul exemplu `12345699`
(`tpl-cfg=0x0010`, `N=1`) este verificat pentru
paritatea XMQ, dar NU intră în mode0 RF fără cheia
și decriptarea verificată.

**Munia:** `WEP/02/1B` și `WEP/04/1B`,
`20.1/65.7` și `21.7/61` pentru temperatură
și umiditate, status `OK`, raport brut erori zero;
întregul profil de 47 octeți necesită
exact două octeți `2F` înainte de înregistrări și
15 după acestea. Orice schimbare necunoscută de TPL,
erori, BCD, lungime, structură, ordine, ID sau mod de
securitate este respinsă fără publicare.

CI verifică cele cinci fixture-uri originale (3 iPERL,
2 Munia), teste negative, generare și mutații XMQ
necompatibile. Ca la restul proiectului, un update
upstream cere recompilare, teste și firmware nou.
Nu s-a făcut flash pe Heltec.

**Limită:** „complet” se referă la semantica
**profilelor și fixture-urilor sursă acceptate**,
nu la toate variantele posibile ale hardware-ului
și nici la decriptarea automată a telegramelor noi.

## Calup de migrare XMQ completă pentru profilurile disponibile: CMA12w + Lansen DW (2026-11-10)

Primele două familii convertite de la decodoare C++ independente la
**interpretul C++ comun al profilelor compilate la build** sunt
`cma12w` și `lansendw`.

`experimental/xmq/tools/generate_xmq_full_profiles.py` parsează
direct XMQ-ul sursă, pentru modelele MVT, `fields.match` cu
`measurement_type`, `vif_range`, `storage_nr`, `subunit_nr`,
`lookup` cu `mask_bits`, `map_type` și `map`, precum și
`library.use=software_version` și toate JSON-urile exemplelor.
Compilarea emite `xmq_generated_full_profiles.h`.
`xmq_full_profile_runtime.h` interpretează același format
generat pentru ambele familii: BCD12, signed16 scalat, raw16,
versiune software și tabele de stare. Wrapper-ele
`cma12w_plain.h` și `lansendw_plain.h` nu mai au bucle
specifice de recunoaștere a înregistrărilor sau conversie
individuală, doar păstrează API-ul structurilor `Reading`.
Numele câmpurilor numerice și text sunt preluate din
descriptorii generați.

**CMA12w** (ELV/20/1B și ELV/1F/1B): temperatura curentă,
media orară, bateria brută + `BATTERY_HEX`, versiunea
software cu componente inversate și `status=OK`.

**Lansen DW** (LAS/07/1D): `a_counter`, `b_counter`,
`OPEN/CLOSED` din IndexToString, `ERROR_FLAGS_1` din
BitToString și combinațiile dovedite `PERMANENT_ERROR
TPL_STS_64/96`. Se păstrează separat câmpurile brute,
bitii TPL neinterpretați și cele șase indicatoare standard.

Generatorul acceptă numai operații, unități și structuri
pe care interpretul comun le implementează demonstrabil.
Schimbările incompatibile în XMQ opresc build-ul; sunt
testate mutații ale selectorilor, tabelelor de lookup,
lungimilor și câmpurilor. Declarațiile XMQ actualizate
**nu sunt executate dinamic** de Heltec: sunt necesare
regenerare, validare de fixture, CI și flash controlat.
Nu au fost relaxate validarea DLL CRC, contor configurat,
TPL fără criptare sau refuzul recordurilor necunoscute.

Această migrare privește **semantica integrală din cele
patru fixture-uri keyless existente** ale celor două familii,
nu autorizează profiluri RF neprobate ori algoritmi de
criptare. Alte 50 de familii rămân de evaluat individual
pentru atingerea aceleiași arhitecturi finale.

## Migrare XMQ nativă: Lansen Pulse după iPERL și Munia (2026-11-10)

Am adăugat `generate_pulse_fields.py` ca al treilea compilator de
semantici native după `generate_ixml_cpp.py` și
`generate_standard_selectors.py`. Pentru `lansenpu`,
`difvifkey`, numele `a_counter` / `b_counter`,
tipul BCD12, bitul specific `SABOTAGE_ENCLOSURE` și
textele statusurilor admise sunt extrase efectiv din
`experimental/xmq/drivers/lansenpu.xmq` și fixture-urile sale,
apoi transpuse în `xmq_generated_pulse_fields.h`. C++ consumă
aceste câmpuri, în loc să păstreze manual cheile DIF/VIF și
etichetele de stare.

Generatorul respinge valori BCD invalide, schimbări necunoscute
ale semnificației bitului de sabotaj, modificări ale structurii
de câmp, JSON-uri incompatibile și statusuri care nu pot fi
justificate de definițiile XMQ existente. Verificările de
criptare, de ID configurat, DLL CRC și de publicare rămân
independente de aceste date generate.

Aceasta este **migrare parțială** a unui al treilea driver, nu
declarație de paritate universală pentru toate cele 52.
Obiectivul este să înlocuim succesiv toate decodoarele manuale
cu selectori, transformări, formule, lookup-uri și structuri
generate la compilare din definițiile XMQ. Nu există XMQ
descărcat/executat dinamic pe Heltec; după update upstream
este obligatoriu build, teste și flash controlat.

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

# Contract: sursele wmbusmeters XMQ trebuie sa devina autoritatea driverelor native

**Stare la 10 noiembrie 2026:** migratia NU este inca finalizata.
Scopul convenit este acelasi model ca la `mkradio4` si `fhkvdataiii`:
definitii XMQ originale -> compilator de subset XMQ/IXML ->
tabele/C++ native ESPHome -> teste pe fixture-uri -> firmware Heltec.

## Situatia reala

Manifest: `experimental/xmq/tools/native_driver_specs.py`.
Total 52 familii native: 2 generate din IXML, 43 decodoare C++ portabile,
3 pipeline-uri configurate separat si 4 pipeline-uri AES separate.
Unele dintre cele 43 folosesc deja tabele de modele/DIF-VIF generate
(`generate_clear_profiles.py`), dar decodarea valorilor, selectia
campurilor, statusul, calendarul si formatul criptografic inca pot
fi codificate manual. Acestea **nu** trebuie raportate ca fiind complet
generate sau automat sincronizate cu wmbusmeters.

La revizia publica upstream `f6ab1c28dab6f953198a4bfcbcfc353378951847`,
compararea Git blob SHA pentru cele 52 XMQ locale a identificat 51
identice si un singur drift: **`fhkvdataiii`**. Upstream include doua
fixture-uri noi, dintre care un OMS 0x08 cu
`current_consumption_hca`, `consumption_at_set_date_hca` si `set_date`,
separat de `current_hca`/`previous_hca` ale telegramei proprietare
0x80. Schimbarea NU trebuie copiata peste runtime fara a asigura ca
valorile primite de la formatul OMS nu suprascriu valorile proprietare.
Un CI verde pentru snapshotul vechi nu valideaza noile telegrame.

## Mecanism de audit si preluare controlata

Un checkout local al repository-ului upstream este necesar.
Exemplu reproductibil:

```bash
git clone --depth 1 https://github.com/wmbusmeters/wmbusmeters.git /tmp/wmbusmeters-upstream
git -C /tmp/wmbusmeters-upstream rev-parse HEAD
python3 experimental/xmq/tools/audit_upstream_drivers.py \
  --upstream-dir /tmp/wmbusmeters-upstream/drivers/src \
  --upstream-revision "$(git -C /tmp/wmbusmeters-upstream rev-parse HEAD)" \
  --json /tmp/xmq-upstream-report.json \
  --markdown /tmp/xmq-upstream-report.md \
  --stage-dir /tmp/xmq-upstream-review
```

Scriptul identifica TOATE cele 52 de surse, citeste XMQ schimbate,
verifica identitatea declarata a driverului, compara SHA de Git si
numarul de fixture-uri. **Nu modifica** `experimental/xmq/drivers`,
decodoarele C++ sau firmware-ul. Copiile sunt puse doar intr-un
director separat pentru review. `--fail-on-drift` poate semnaliza
drift intr-un job de monitorizare SEPARAT; nu trebuie confundat cu
un regres al compilarii firmware.

Pentru a promova o sursa noua trebuie:

1. Validat diferențial XMQ fata de upstream (selector, campuri, IXML,
   formule, status, statusuri necunoscute, unitati, tip TPL/ELL, AES).
2. Compilata in cod/tabele native. Daca compilatorul de subset nu
   suporta o constructie, extensia se face explicit, fara ignorare.
3. Comparate campurile originale JSON pentru toate fixture-urile
   upstream si mutatiile negative (criptare, CRC, ID, statusuri).
4. Compilat Heltec ESPHome si ESP-IDF in CI; apoi test hardware
   separat daca decizia este de a instala un firmware nou.
5. Niciun update de sursa/firmware nu este auto-instalat pe Heltec.

## Migrare functionala ramasa

- **Etapa 1 (instrumentata):** audit pentru cele 52 de surse XMQ,
  plus semnalarea precisa a modificarilor. Nu rezolva decodoarele manuale.
- **Etapa 2:** compilator comun al selectorilor `match`
  (DIF/VIF, storage, tariff, VIFE, quantity, scaling) in descriptorii
  C++ nativi. Conversia sa inlocuiasca efectiv selectiile
  codificate manual, nu sa genereze doar metadata.
- **Etapa 3:** compilarea functiilor `calculate`, `lookup`,
  `library.use`, codurilor de stare, campurilor text si datelor.
  Constructiile neimplementate trebuie sa opreasca build-ul
  pe o noua sursa, nu sa fie ignorate silentios.
- **Etapa 4:** adaptoare native pentru IXML proprietar,
  transport ELL/AFL si criptare, cu chei configurate explicit,
  negarea automata a datelor deja-decriptate si teste separate.
- **Etapa 5:** migrarea etapizata a celor 43 de decodoare portabile,
  apoi reducerea codului duplicat si suportul surselor actualizate
  numai dupa paritate completa. Incepem cu formele simple
  (`munia`, `cma12w`, `lansenpu`, `lansendw`, `iperl`),
  apoi `ultraheat` si restul. In paralel, `fhkvdataiii`
  are prioritate functionala pentru noul profil OMS 0x08.

`driver: auto` alege un driver deja compilat pentru un contor
configurat. Nu descarca, nu interpreteaza si nu compileaza XMQ la
runtime. Un update upstream va fi utilizabil pe Heltec **dupa
regenerare, teste, compilare si instalarea firmware-ului**.
