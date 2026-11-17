# Etapa A — motor XMQ comun (în dezvoltare)

**Branch:** `experimental/xmq-runtime-stage-a`  
**Bază:** `experimental/dual-radio-sx126x-sx127x-sync54`.

## Obiectiv acceptat

La actualizarea unui driver, copiem fișierul `.xmq` **original** din
`wmbusmeters/drivers/src`, fără generarea manuală a unui nou `driver_*.cc`
sau transcriere individuală a regulilor. Interpretul comun va procesa
aceleași descrieri; doar extensiile *limbajului* upstream pot necesita
modificări în motor. Ținta ulterioară: stocare în flash și hot-reload fără OTA.

## Ce este implementat în acest prim increment

- `xmq_reader.h`: parser **structural** C++17, independent de ESPHome
  și radio. Citește blocuri imbricate, chei/valori, șiruri multilinii,
  comentarii `//` și `/* ... */`, inclusiv anteturi `template(...)`.
  Păstrează reguli necunoscute pentru extensibilitate; nu are reguli
  specifice fiecărui contor.
- Limite configurabile pentru dimensiunea sursei, numărul de noduri și
  adâncimea arborelui; erori explicite pentru surse incomplete/invalide.
- `upstream/src/*.xmq`: copii **nemodificate** pentru `iperl`,
  `mkradio4a`, `mkradio4`, `fhkvdataiii`.
- `upstream/library.xmq`: copie nemodificată din wmbusmeters.
- `test_xmq_reader.cpp`: verifică structură, identificarea driverului,
  câmpuri, IXML și formule păstrate drept text, plus limite și erori.

**NU este încă un interpret funcțional XMQ.** Nu execută `match`,
`calculate`, `ixml`, `library { use ... }`, nu interpretează DV records,
nu face dispatch/auto-detect, AES sau hot-reload. Nu este conectat la
`wmbus_radio`/`wmbus_meter`. Decodoarele C++ vechi rămân active.
Nu există rezultate RF sau paritate de valori atribuite acestui parser.

## Test local pe PC (nu lansează GitHub Actions)

Din rădăcina repo-ului:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -Iexperimental/xmq-runtime-stage-a \
  experimental/xmq-runtime-stage-a/test_xmq_reader.cpp \
  -o /tmp/test_xmq_reader
/tmp/test_xmq_reader
```

Testul este **pregătit, nu executat prin GitHub Actions** în acest increment.
Atenție: `STRUCTURAL_OK` confirmă numai citirea structurii, NU paritatea
telegramelor wmbusmeters.

## Etapele următoare, tot în Etapa A

1. Adăugare AST semantic pentru `detect`, `match`, cantități, unități
   și câmpuri; evaluare `iperl`/ `mkradio4a` pe fixture-uri upstream.
2. Motor comun pentru `calculate`, expresii și `library.xmq`.
3. Interpret IXML necesar pentru `mkradio4`/`fhkvdataiii`.
4. Teste de paritate cu rezultatele upstream, urmate de adaptorul ESPHome
   selectabil, fără a elimina încă decodoarele existente.
5. Separat: fișiere din flash, activare atomică și rollback / hot-reload.

## Proveniență și licență

Fișierele din `upstream/` provin din
https://github.com/wmbusmeters/wmbusmeters/tree/master/drivers ;
sunt păstrate fără schimbări și cu antetele originale. Upstream declară
GPL-3.0-or-later; tratați toate copiile și utilizarea acestora conform
licenței upstream. Înaintea fiecărui import ulterior vom urmări commitul
upstream și compatibilitatea regulilor.

**Nu modificați SX126x/SX127x în acest branch pentru a testa parserul.**

## Increment 2 — metadate și potrivire semantică (16.11.2026)

- `xmq_semantic.h` introduce un model comun `Driver`, `Mvt`, `FieldRule` și `DvRecord`, independent de placa radio și de numele driverului.
- Încărcarea `driver { ... }` extrage `name`, `meter_type`, `manufacturer`, lista `detect.mvt`, câmpuri `match`, `calculate` și `ixml` (ultimele două doar stocate, **neexecutate**).
- Funcția `detects(...)` verifică exact tripletul producător/versiune/tip (valorile numerice din `mvt` sunt hexazecimale).
- `matches(...)` acceptă exclusiv selectorii `difvifkey`, `measurement_type`, `vif_range`, `storage_nr`, `tariff_nr` și `subunit_nr`; un selector necunoscut face regula **nesuportată** și potrivirea e respinsă. Nici `match_entire_payload`, nici `calculate` nu pot fi evaluate de acest modul.
- Pentru `storage_nr`/`tariff_nr`/`subunit_nr` nespecificate se potrivește numai valoarea zero, păstrând comportamentul conservator din `FieldMatcher` upstream.
- `test_xmq_semantic.cpp`: cazuri de potrivire și respingere pentru fișiere originale `iperl`, `mkradio4a`, `mkradio4` și `fhkvdataiii`, plus o clauză necunoscută.

**Limitare critică:** structura `DvRecord` primește metadatele DV deja decodate din exterior. Acest increment nu parsează încă telegrame, nu extrage valori și nu verifică paritatea rezultatelor cu wmbusmeters. Nici testele C++ nu au fost executate încă într-un build local/CI; existența lor în GitHub nu înseamnă PASS.

Pentru rulare locală din root repo:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iexperimental/xmq-runtime-stage-a \
  experimental/xmq-runtime-stage-a/test_xmq_semantic.cpp -o /tmp/test_xmq_semantic
/tmp/test_xmq_semantic
```

**Următorul increment Etapa A:** suport complet pentru selectorii upstream, evaluarea cantităților și conversiilor; apoi formula engine și IXML, cu teste de paritate reale.

---


## Increment 3 — expresii numerice comune (16.11.2026)

- `xmq_expression.h`: evaluator de expresii numerice cu variabile rezolvate dintr-un tabel de valori. Suportă `+`, `-`, `*`, `/`, `%`, `>>`, `<<`, comparații, `&&`, `||`, negație, paranteze și minus unar.
- Unități tipizate în acest subset: `counter` (dimensiune numerică) și `m3` (volum); adunări între dimensiuni incompatibile sunt respinse, la fel operațiile invalide, variabilele necunoscute, diviziunea la zero, expresiile prea lungi și schimbările de biți nevalide.
- `test_xmq_expression.cpp`: alege formulele direct din `mkradio4.xmq` nemodificat și testează lanțul numeric `prev_raw_counter` → `prev_m3` → `target_m3` și `total_m3`; verifică expresii autentice `fhkvdataiii.xmq` pentru anul/luna/ziua anterioară. Fără reguli C++ specifice driverului în motor.
- Formula cu data de calendar din `fhkvdataiii` (`'2000-01-01' + ... month + ... h`) rămâne explicit **nesuportată**; similar expresiile cu alte unități și operații specifice upstream neintroduse încă. IXML nu este interpretat.
- Test local suplimentar: `g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iexperimental/xmq-runtime-stage-a experimental/xmq-runtime-stage-a/test_xmq_expression.cpp -o /tmp/test_xmq_expression && /tmp/test_xmq_expression`.

**Stare validare:** codul și testele sunt publicate, însă compilarea și rularea C++ nu au fost executate încă pe acest increment. Nu există paritate demonstrată pe telegrame. În pasul următor trebuie verificat local `-Werror`, apoi completate unitățile/calendarul, extractorul DV și IXML pe fixture-uri originale.

---



## Increment 4 — extragere DV și proiectare numerică din XMQ

Fișiere noi:
- `xmq_dv_subset.h` — parser cu limite pentru un subset de înregistrări DV clare, **după** verificarea PHY/CRC și **după** decriptare. Acceptă tipuri DIF întregi/BCD standard și VIF Volume/VolumeFlow; nu interpretează încă datele calendaristice, IXML, VIFE extinse ori structuri proprietare.
- `test_xmq_dv_subset.cpp` — exemple de payload clare din fixture-urile originale `iperl` și `mkradio4a`, plus cazuri trunchiate/invalide.
- `xmq_projection.h` — punte generică: aplică regulile `match` extrase din fișierul XMQ asupra înregistrărilor DV deja decodate. Produce numai câmpuri numerice Volume/Flow compatibile; detectează potriviri ambigue. Nu conține decodoare per model.
- `test_xmq_projection.cpp` — verifică prin fișierele XMQ originale `iperl` și `mkradio4a` câmpurile numerice `total_m3`, `max_flow_m3h`, `target_m3`; respinge dublurile. Nu pretinde interpretarea câmpurilor `target_date` / `status`.

**Important:** până la completarea calendarului, IXML și formulelor, **nu există paritate integrală** cu niciunul dintre aceste drivere. `mkradio4` și `fhkvdataiii` încă necesită IXML ca să creeze înregistrările DV proprietare înainte de calcule. Nu s-a făcut modificare ori flash pe Heltec/LILYGO.

**Validare GitHub:** primul workflow declanșat în această etapă s-a oprit la avertismentul GCC `-Werror=dangling-reference` din `test_xmq_reader.cpp`, raportat de utilizator. S-au corectat ulterior semnătura helperului și concatenarea textului în excepție, dar corecțiile **nu au fost încă revalidate printr-o nouă execuție**. Workflow-ul cu declanșare explicită prin fișierul `run-once-20261116.txt` a fost extins să includă `projection` la o execuție viitoare; **nu a fost pornit al doilea run**.

Pentru validare locală (fără GitHub Actions), din rădăcina repository-ului:

```sh
dir=experimental/xmq-runtime-stage-a
for suite in reader semantic expression dv_subset projection; do
  g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pedantic -I"$dir" \
    "$dir/test_xmq_${suite}.cpp" -o "/tmp/test_xmq_${suite}" || exit 1
  "/tmp/test_xmq_${suite}" || exit 1
done
```

**Pasul următor:** compatibilitate cu semantica exactă a parserului upstream pentru înregistrările DV standard, date/status/BCD/semne, apoi IXML (`mkradio4`, `fhkvdataiii`) și verificarea parității complete față de rezultatele fixture-urilor originale.


## Increment 5 — interpret IXML din fișier, plus lanț numeric comun

- `xmq_ixml_subset.h` citește expresiile `ixml` **direct din fișierele upstream**, interpretând un subset generic: `decode` și alternative, reguli numite, literali hex, `byte`, `word`, `byte*`, capturi `@DV_...` și declarații `>dvk`. Nu conține ramuri condiționate de numele driverului.
- `test_xmq_ixml_subset.cpp` verifică extragerea DV sintetică pentru payload-uri originale `mkradio4` și `fhkvdataiii` (format `0F`) și respingerea prefixelor necunoscute. Capturile sunt doar octeți și DIF/VIF; deocamdată **nu înseamnă valori publicate în HA**.
- `xmq_numeric_runtime.h` leagă generic IXML → înregistrări DV standard → `match` → expresii `calculate`. Subsetul de valori acceptate include `Volume` și `Dimensionless`, inclusiv valoarea de volum cu VIF `0x15`.
- `test_xmq_numeric_runtime.cpp` citește **`mkradio4.xmq` nemodificat** și telegrama clară din fixture-ul upstream, apoi verifică numeric `target_m3 = 0.1` și `total_m3 = 0.4`. Nicio formulă per-driver nu este hardcodificată în interpret.
- **Corecție de compatibilitate upstream:** potrivirea exactă prin `difvifkey` prevalează față de stocarea implicită zero, ca în `FieldMatcher` din wmbusmeters. Test de regresie introdus pentru `mkradio4` / `4215` (storage=1).

**Limite deliberate:** IXML complet are construcții încă neimplementate; fiecare construcție necunoscută produce eroare. Pentru `fhkvdataiii` sunt capturate înregistrările sintetice, dar extragerea valorilor HCA, temperaturilor și datelor complete urmează. Nu există încă selectare automată conectată la pipeline, AES, flash runtime sau hot-reload. Niciun decodor C++ existent nu a fost eliminat.

**Testare:** suitele C++ noi sunt incluse în lista **pentru următoarea verificare explicită** a workflow-ului `xmq-stage-a-host.yml`; fișierul de declanșare unică NU a fost modificat și nu a fost lansată a doua execuție GitHub Actions. Nu declarăm PASS fără compilare și rulare.

Pentru rulare locală fără Actions:

```sh
dir=experimental/xmq-runtime-stage-a
for suite in reader semantic expression dv_subset projection ixml_subset numeric_runtime; do
  g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pedantic -I"$dir" \
    "$dir/test_xmq_${suite}.cpp" -o "/tmp/test_xmq_${suite}" || exit 1
  "/tmp/test_xmq_${suite}" || exit 1
done
```

**Următorul pas Etapa A:** verificarea integrată prin build, apoi extinderea DV către HCA, temperaturi/date și expresii calendaristice, pentru paritatea `fhkvdataiii` pe fixture-urile originale.

## Increment 6 — valori HCA, temperatură și componente de dată (16.11.2026)

- `xmq_dv_subset.h`: suport generic, suplimentar față de Volume/VolumeFlow, pentru VIF `6E` (HeatCostAllocation), `64..67` (ExternalTemperature), `5C..5F` (ReturnTemperature) și `FD3A` (counter/dimensionless). Sunt interpretate numai înregistrările DV numerice cu tip de date suportat; restul nu devin automat câmpuri valide.
- `xmq_expression.h`: distinge și păstrează dimensiunile `HCA` și `TemperatureC`, pe lângă `Counter` și `VolumeM3`; operațiile între dimensiuni incompatibile sunt refuzate.
- `xmq_numeric_runtime.h`: câmpurile XMQ `HCA`, `Temperature` și `Dimensionless` pot fi extrase prin regulile `match` originale, cu tipul de unitate verificat. Formulele numerice `previous_day/month/year` și `current_day/month/year` sunt evaluate din XMQ, nu reconstruite în C++ specific driverului.
- `test_xmq_hca_numeric.cpp`: verificare de regresie pregătită pentru payload-ul upstream `fhkvdataiii`, varianta `0F`: current=4 HCA, previous=45 HCA, temp_room=22.37 °C, temp_radiator=23.60 °C, plus componentele de dată 2021-02-05 și 2020-12-31.
- Lista de teste a workflow-ului `xmq-stage-a-host.yml` a fost extinsă **fără a modifica trigger-ul de unică folosință** și fără a lansa un al doilea run.

**De verificat înainte de flash / integrare:** compilare și execuție efectivă a testelor C++ (încă neconfirmate), corectitudinea pentru celelalte versiuni proprietare `01`/`11`, calculul datelor ISO complete din formulele calendaristice XMQ, comportamentul cu câmpuri lipsă și câmpuri duplicate, și paritate integrală cu fixture-urile upstream. Nu este permisă publicarea rezultatelor unui decodor experimental în Home Assistant până la paritate confirmată. Acest cod nu este încă conectat la ESPHome.

**Regulă de menținere:** pentru driverele upstream se copiază numai fișierele `.xmq` nemodificate; se dezvoltă o singură implementare generică pentru limbaj, fără implementări pe nume de contor.


## Increment 7 — calendar XMQ original, fără reguli per-contor (16.11.2026)

- `xmq_calendar.h`: evaluator calendaristic independent de ESPHome, folosind aritmetică gregoriană exactă și expresii de forma dată ISO + luni întregi + ore întregi. Citește variabile din rezultatele numerice ale motorului comun; nu presupune luni de 30 zile și nu folosește fusul orar al sistemului.
- `xmq_calendar_runtime.h`: evaluează câmpurile `PointInTime` cu `calculate` din `.xmq` și publică rezultatele interne `current_date` / `previous_date` ca șiruri ISO (nu entități ESPHome).
- `test_xmq_calendar.cpp`: cazuri de test pregătite folosind **regulile și payload-urile originale** `fhkvdataiii` pentru marcajele IXML `01`, `11`, `0F` și datele de referință 2026-06-04/2025-12-31, 2020-02-08/2019-12-31, 2021-02-05/2020-12-31. Include an bisect, schimbare de an, unități nesuportate și variabile lipsă.
- Am adăugat testul `calendar` în suita host **pentru o verificare viitoare**, fără modificarea fișierului de declanșare `run-once-20261116.txt` și fără alt workflow.

**Limitări importante:** subsetul nu implementează toate operațiile calendaristice acceptate de wmbusmeters; refuză expresiile necunoscute. Nu există încă paritate confirmată prin execuția testului `calendar`; upstream nu a fost comparat complet pentru câmpuri lipsă, OMS 0x08, status sau AES. Motorul nu este încă activ în dispozitivele Heltec/LILYGO.

Rulare locală, din rădăcina repo-ului:

```sh
dir=experimental/xmq-runtime-stage-a
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -pedantic -I"$dir" \
  "$dir/test_xmq_calendar.cpp" -o /tmp/test_xmq_calendar
/tmp/test_xmq_calendar
```

**Continuare:** este necesară o verificare de compilare și execuție a întregii suite C++ (fără a lansa repetat Actions). Apoi completarea semnificației valorilor DV/OMS și a câmpurilor lipsă, fără a crea cod special pentru `fhkvdataiii`.


## Increment 8 — telegrame OMS parțiale / protecție împotriva valorilor inventate (17.11.2026)

- În `xmq_numeric_runtime.h`, formulele `calculate` cu variabile lipsă nu mai întrerup procesarea întregii telegrame. Câmpul derivat respectiv lipsește din rezultat; erorile reale ale expresiei (unități greșite, împărțire la zero, sintaxă nesuportată) rămân erori.
- În `xmq_calendar_runtime.h`, câmpurile de dată nu se mai calculează dacă telegrama nu conține variabilele necesare. Nu se reutilizează valori dintr-o telegramă anterioară.
- `test_xmq_oms_partial.cpp`: folosește payload-ul exact al fixture-ului original `fhkvdataiii` cu telegramă OMS de tip 0x08, verificând `current_consumption_hca=152` și `consumption_at_set_date_hca=1092` fără a fabrica `current_hca`, `previous_hca`, datele proprietare ori componentele lor. Testează și izolarea între o telegramă completă și una parțială.
- Testul este adăugat la suita de host pentru **următoarea execuție autorizată**, fără un al doilea trigger GitHub Actions.

**Stare de verificare:** modificările sunt publicate, însă compilarea locală nu a putut fi efectuată aici deoarece mediul de execuție nu are acces direct la sursele GitHub; testele noi nu sunt declarate PASS. Singura execuție GitHub Actions anterioară a eșuat la compilator și nu a fost relansată.

**Limitări:** data OMS standard `426C` nu este încă interpretată de subsetul numeric, iar statusurile, AES, IXML complet, întreaga bibliotecă XMQ și integrarea ESPHome rămân nefinalizate. Nu înlocuiți decodoarele actuale pe hardware.


## Increment 9 — date standard OMS VIF 0x6C (17.11.2026)

- `xmq_dv_subset.h`: suport generic pentru data OMS de doi octeți, VIF `6C`. Decodarea calculează ziua, luna și anul din structura EN 13757, validează intervalele și anii bisecți și atașează un șir ISO `YYYY-MM-DD` la `DecodedRecord::iso_date`. Valorile numerice HCA se păstrează.
- `xmq_oms_date.h`: proiecție de câmpuri Date exclusiv după regulile `match` citite din `.xmq`, fără condiții dependente de numele contorului; ambiguitățile sunt respinse.
- `test_xmq_oms_partial.cpp`: fixture upstream OMS `0x08` pentru `fhkvdataiii` verifică `set_date=2025-12-31`, `current_consumption_hca=152`, `consumption_at_set_date_hca=1092`. `current_date`/`previous_date` din formule proprietare **rămân absente** în acest frame. Teste de respingere pentru lună invalidă și zi zero.
- În același fișier de test se verifică separarea între o telegramă completă proprietară și una OMS parțială.

**Validare:** nu s-a putut accesa GitHub din mediul local de compilare (DNS `github.com` indisponibil). Fișierele au fost actualizate prin conexiunea GitHub, dar compilarea/rularea efectivă a testelor noi **nu este confirmată**. Nu s-a pornit un alt workflow GitHub. Actualizarea păstrează componentele radio și decoder-ele de producție nemodificate.

**Următorul pas prioritar:** verificarea de compilare și execuție a întregului set de teste host, corectarea eșecurilor observate și verificarea parității OMS/IXML înainte de extinderea motorului.


## Increment 10 — cale comună pentru payload-uri OMS clare (17.11.2026)

Validarea precedentă GitHub Actions #4 a fost **verde**, inclusiv verificarea că radio-ul și componentele wmbus active nu au fost modificate:
https://github.com/radugeo74/esphome-sx1262/actions/runs/44093741937

- `xmq_clear_runtime.h`: o cale generică independentă de ESPHome care validează tripletul MVT al contorului, decodează înregistrări DV standard, proiectează valorile Volume/Flow și datele OMS conform regulilor `match` din **fișierele XMQ originale**.
- `test_xmq_clear_runtime.cpp`: folosește fișierele upstream `iperl.xmq` și `mkradio4a.xmq` nemodificate; verifică numeric 7.704/123.529 m³, 16.1/75.2 m³, precum și `target_date=2021-12-31` (cheia internă `target` în proiecția OMS). Mai testează MVT incompatibil, dată invalidă și lipsa scurgerilor de stare între telegrame.
- Nu adaugă funcții per-driver; proiectarea rezultatelor este comună.

**Limitări:** datele trebuie să fie deja verificate la nivel radio/CRC și decriptate; nu există interpretare completă a statusului, a tuturor VIF/VIFE ori paritate integrală. Motorul încă nu este conectat la firmware.

Testul nou este inclus în **următoarea singură execuție GitHub Actions**, care trebuie verificată înaintea incrementului următor.

## Increment 11 — registru comun auto/explicit (17.11.2026)

- `xmq_driver_registry.h` selectează automat driverul din `detect.mvt` al fișierelor XMQ originale; acceptă și nume explicit, dacă există în registru și identificarea MVT se potrivește. Refuză coliziunile de identificare și numele duplicate, fără a alege arbitrar.
- `test_xmq_driver_registry.cpp` verifică selecția `iperl`, `mkradio4a`, `mkradio4`, `fhkvdataiii` și refuzurile pentru necunoscute și ambiguități; include decodare standard de test după selectare.
- Limite: selectarea funcționează doar pe identitatea MVT primită deja de la un apelant, nu extrage identitatea din telegramă; nu este încă adaptor ESPHome, nu gestionează prioritatea upstream, nu interpretează cadrele radio și nu decriptează AES. Alegerea explicită este momentan strictă la MVT.

Testele vor fi validate împreună într-un singur workflow. Nu se modifică decodoarele de producție.

## Increment 12 — registru încărcat din directorul upstream XMQ (17.11.2026)

- Baza validată: [XMQ Stage A #6 — verde](https://github.com/radugeo74/esphome-sx1262/actions/runs/44096080728).
- `xmq_driver_loader.h`: citește toate fișierele `.xmq` nemodificate dintr-un director local, validează limitele (maximum 128 drivere / 256 KiB per driver), interzice symlink-urile și numele ambigue, verifică potrivirea numelui din `driver.name` cu fișierul și construiește registrul comun. Nu introduce cod separat pentru fiecare driver.
- `test_xmq_driver_loader.cpp`: testează încărcarea setului upstream `iperl`, `mkradio4a`, `mkradio4`, `fhkvdataiii`, selecția automată și decodarea unui exemplu clar, limitele de resurse, refuzul unui alias, al unui XMQ incomplet și al unui fișier symlink.
- Modificarea este **strict experimentală**, independentă de ESPHome. Folosește sistemul de fișiere al hostului, nu încărcare din flash pe ESP32. Înlocuirea driverelor runtime fără recompilare și încărcarea unei biblioteci XMQ complete sunt obiective viitoare; nu sunt încă implementate.
- Testul `driver_loader` este adăugat în suita host și se validează într-un singur workflow pentru acest calup.


## Increment 13 — încărcare XMQ independentă de sistemul de fișiere

- `xmq_source_registry.h` primește lista numelor driverelor și un **callback de citire** a textului XMQ. Motorul nu folosește `std::filesystem`: adaptorul poate proveni ulterior din flash sau alt sistem de stocare, fără să schimbe parserul ori decodoarele comune.
- Sunt impuse limite explicite (maxim 128 drivere, 256 KiB/fișier, 1 MiB surse text total), nume simple fără căi, fără duplicate și identificare `driver.name` egală cu numele sursei. Callback-ul este furnizat de apelant și trebuie să returneze surse de încredere.
- `test_xmq_source_registry.cpp` folosește cele 4 fișiere originale upstream ca date memorate, selectează driverele după MVT, verifică decodarea standard și refuză sursele goale, duplicate sau peste limite.
- Acesta este **doar un pas portabil, testat pe host**. NU citește efectiv LittleFS/SPIFFS de pe ESP32, nu face actualizare fără recompilare și nu este conectat la radio sau la senzorii Home Assistant.
- Baza precedentă: [workflow #7 verde](https://github.com/radugeo74/esphome-sx1262/actions/runs/44098363372). Verificarea noului calup se efectuează separat, o singură dată.
