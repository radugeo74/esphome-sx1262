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


## Increment 14 — adaptor read-only pentru un VFS deja montat (17.11.2026)

- `xmq_mounted_vfs.h`: adaptor separat, independent de ESPHome și de recepția radio, care citește `/cale/montata/<driver>.xmq` prin `fopen/fread` standard C, compatibile conceptual cu VFS-ul ESP-IDF **după montarea sistemului de fișiere de către aplicație**. Nu formatează, nu montează, nu scrie și nu actualizează partiții flash.
- Se folosesc aceleași reguli XMQ originale și registrul `xmq_source_registry.h`. Numele driverelor sunt date explicit și validate; nu există traversare de directoare la citirea numelor. Mount point-ul trebuie să fie de încredere; acest adaptor nu izolează symlink-uri ostile dintr-un filesystem host.
- Limite implicite pentru experiment: **maxim 8 drivere, 16 KiB per fișier, 32 KiB text total**. Sunt limite de intrare, **nu** un buget garantat de heap (parserul și structurile C++ consumă RAM suplimentară). Valoarea se validează mai târziu pe ESP32.
- `test_xmq_mounted_vfs.cpp`: simulează VFS montat pe host, cu cele patru surse upstream copiate într-un director temporar, verifică autodetectarea, rezultate `mkradio4a`, căi invalide, fișiere absente/goale/depășite, date XMQ invalide și depășiri ale limitelor.
- **Nu reprezintă încă build ESP-IDF/ESPHome sau test hardware.** Bibliotecile C++ folosesc excepții și trebuie evaluată compatibilitatea cu configurația firmware-ului. Nu există încă provisionare de imagini LittleFS/SPIFFS, montare VFS, securitate/integritate a driverelor actualizate, încărcare OTA ori publicare de senzori.

Workflow-ul pentru increment compilează și testează acest adaptor pe host în cadrul unei singure execuții GitHub Actions. Nicio componentă radio nu este modificată.


## Increment 15 — compilare reală ESP-IDF ESP32-S3, izolată (17.11.2026)

- Proiectul minimal `espidf-smoke/` compilează adaptorul de citire `xmq_mounted_vfs.h` și traseul de interpretare XMQ pentru **ESP32-S3**, nu doar pentru arhitectura x86 Linux. Nu se include `xmq_driver_loader.h` (dependent de `std::filesystem`, rezervat hostului).
- `espidf-smoke/sdkconfig.defaults` activează explicit `CONFIG_COMPILER_CXX_EXCEPTIONS=y`. Acesta este doar un workaround necesar motorului experimental actual; nu demonstrează compatibilitate cu ESPHome, unde excepțiile pot fi dezactivate. Eliminarea excepțiilor din motor sau configurarea explicită a acestora rămâne un pas înaintea integrării.
- Jobul dependent `esp32s3-xmq-build` din **același workflow** `xmq-stage-a-host.yml` utilizează `espressif/esp-idf-ci-action@v1`, imagine ESP-IDF `v5.5`, `target=esp32s3`; rulează `idf.py build && idf.py size`. Jobul pornește numai după ce toate testele host au trecut.
- Fișierul `main.cpp` verifică strict compilarea, linkarea și tratarea controlată a unei surse nemontate. **Nu montează efectiv LittleFS/SPIFFS, nu citește partiția flash reală, nu publică senzori și nu este compilare a firmware-ului final Heltec/LILYGO.** Raportul `idf.py size` se referă la imaginea minimală, nu la heap maxim ori fragmentare în runtime.
- După o validare verde a acestui calup, următorii pași necesari sunt montarea reală a unui sistem de fișiere cu integritate verificată, testarea pe dispozitiv, integrarea în `wmbus_common` și strategia fără excepții pentru compatibilitatea cu ESPHome.

**Regulă CI:** un singur workflow declanșat pentru acest increment; în interiorul său două joburi ordonate (host, apoi ESP32-S3 build-only). Componentele radio funcționale nu sunt schimbate.


### Rezultat validat al Incrementului 15 — GitHub Actions #11

[Workflow #11 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44102921242), commit `faf32169260bc22babd1cba2bff225c3042b3916`. Ambele joburi au trecut: suita XMQ host și compilarea/linkarea proiectului minimal pe **ESP-IDF v5.5, target esp32s3**, urmată de `idf.py size`.

Valorile obținute din logul **imaginii minimale**:
- `xmq_stage_a_smoke.bin`: `0x3a5d0` octeți (239056 octeți), într-o partiție de aplicație de 1 MiB.
- `idf.py size`: imaginea totală estimată 238933 octeți; DIRAM static 56007 octeți (16,39% din 341760), memorie DIRAM rămasă 285753 octeți, flash code 108246 octeți și flash data 60616 octeți.
- Datele **nu sunt** măsurători de heap în funcționare, nu includ configurarea radio/ESPHome, fișierele XMQ stocate și nici consumul integral pentru decodare. În această imagine, apelul către un fișier nemontat este doar pentru a asigura compilarea simbolurilor.

**Nu folosiți această imagine pentru flash pe Heltec/LILYGO**: este un smoke test fără partiție de drivere, fără montare filesystem și fără integrare radio.


## Increment 16 — pod izolat de la payload validat la interpretul XMQ

- După [workflow-ul #11 verde](https://github.com/radugeo74/esphome-sx1262/actions/runs/44102921242), am verificat calea actuală `components/wmbus_meter/wmbus_meter.cpp`: handlerul său apelează în continuare `Meter::handleTelegram` din motorul existent. **Nu s-a schimbat acest traseu**.
- `xmq_frame_bridge.h` introduce un punct de intrare comun pentru aplicația deja validată/decriptată, identitatea MVT interioară și formatul explicit `StandardDv` sau `OriginalIxml`. Selectează driverul din registrul comun, aplică regulile originale XMQ pentru câmpuri numerice și date; refuză identități/formate greșite și payloaduri goale.
- `test_xmq_frame_bridge.cpp`: compară rezultate definite de fixture-urile originale `fhkvdataiii`, `mkradio4`, `mkradio4a`, `iperl`: HCA 4/45, temperaturi 22.37/23.60, date ISO, HCA OMS 152/1092 și set_date, apă 0.1/0.4, 16.1 și 123.529 m³.
- Proiectul `espidf-smoke/main/main.cpp` compilează și leagă același pod de aplicație în build-ul minimal ESP32-S3, pe lângă adaptorul VFS.
- **Limită de securitate:** podul nu primește telegrame brute, nu verifică CRC, nu decriptează AES, nu determină CI/link-mode automat și nu marchează un cadru ca handled. Nu înlocuiește driverul existent și nu emite senzori; trebuie să primească payloadul clar și identitatea verificate de pipeline-ul actual.
- Nu instalați încă imaginea ESP-IDF minimală pe Heltec; înainte sunt obligatorii integrarea controlată în ESPHome, comparație paralelă cu decodorul vechi și măsurarea consumului de heap.

Validare: un singur workflow GitHub Actions cu teste host și compilare ESP32-S3 build-only pentru acest calup.


### Rezultat confirmat — Incrementul 16

[Workflow #12 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44105521193), commit `42ce903030cef37b1bd3426da5d8086bc9f9129c`: ambele joburi au trecut (host cu `frame_bridge` și ESP-IDF v5.5, `esp32s3`), inclusiv verificarea că toate componentele radio active au rămas nemodificate.

Logul `idf.py size` pentru imaginea minimală care **referențiază și leagă acum efectiv bridge-ul XMQ**:
- binar `xmq_stage_a_smoke.bin`: `0x85340` = 545600 octeți (aproximativ 532,8 KiB) dintr-o partiție app de 1 MiB;
- imagine raportată de `idf.py size`: 545477 octeți;
- DIRAM static utilizat: **60839 octeți** (17,8% din 341760), rămas raportat **280921 octeți**;
- Flash Code: 340002 octeți; Flash Data: 135060 octeți.

Față de smoke-ul precedent, includerea reală a motorului în linkare crește semnificativ imaginea flash (de la ~239 KiB), dar numai cu 4832 octeți DIRAM static raportat. **Nu reprezintă RAM de vârf, heap folosit în procesare, stocarea driverelor ori firmware-ul complet ESPHome**. Compatibilitatea excepțiilor C++ cu configurația finală și limitele RAM la decode rămân probleme deschise. Firmware-ul de test nu este instalabil ca receptor funcțional.


## Increment 17 — comparator de paritate offline, fără impact asupra firmware-ului

- `xmq_shadow_compare.h`: comparator generic, read-only, pentru rezultatele numerice și datele decodate de `xmq_frame_bridge.h` în raport cu rezultate așteptate documentate. Raportează diferențe, valori/date lipsă și valori neașteptate, cu toleranță numerică explicită. Nu modifică registrul, cadrele radio sau starea vreunui contor.
- `test_xmq_shadow_compare.cpp`: testează sursele XMQ originale `fhkvdataiii` (0x94 proprietar + OMS 0x08), `mkradio4`, `mkradio4a`, `iperl`. Valorile sunt copiate ca așteptări din fixture-urile `json` upstream, doar pentru subsetul suportat. Verifică și că abaterile simulate sunt detectate.
- Aceste teste sunt **paritate față de valori de referință din fixture-urile upstream**, nu execută în paralel instanțe ale driverului vechi C++ și XMQ pe aceeași telegramă recepționată live. Diferențele de status/TPL, criptare, decodare completă și câmpurile neimplementate rămân în afara contractului.
- Nu s-a modificat `components/wmbus_meter` sau `components/wmbus_radio` și nu s-a conectat comparatorul la calea live de recepție. Nu marcați aceste teste drept dovadă că XMQ poate înlocui încă decodorul vechi pe Heltec.
- Un singur workflow pentru Incrementul 17, care include toate suitele host și compilarea izolată ESP32-S3, va valida acest calup.

**Validare Increment 17:** [workflow #13 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44106958561). Au trecut atât toate testele host (inclusiv `shadow_compare`), cât și build-ul separat ESP-IDF ESP32-S3. Nicio schimbare în componentele active. Aceasta nu certifică comparația live cu decodorul C++ vechi și nu autorizează instalarea imaginii minimale pe Heltec.


## Increment 18 — modul „shadow” numai pentru diagnostic, fără impact asupra producției (17.11.2026)

- `xmq_shadow_diagnostic.h`: sesiune read-only de comparație a rezultatului XMQ cu valori deja produse de decodorul existent. Este **dezactivată implicit**; se activează explicit prin `set_enabled(true)`. Are număr maxim de observații configurabil, toleranță pentru diferențe numerice, contoare de potriviri/diferențe/erori și suport pentru eșantioane ale memoriei libere furnizate de apelant.
- Orice eroare în interpretul XMQ/comparator este reținută drept diagnostic `UnsupportedOrError` și **nu propagă excepții** către apelantul care execută decodarea veche. Un apel de diagnostic nu schimbă valorile vechi, nu marchează telegrama ca procesată și nu publică senzori.
- Nu se rețin payloaduri, adrese, identificatori, chei sau valorile fiecărei telegrame în contorul de diagnostic. Se rețin numai statistici agregate și ultima dimensiune a comparației; aplicația este responsabilă de a furniza un payload **deja verificat CRC și decriptat**, cu identitatea MVT interioară corectă.
- `test_xmq_shadow_diagnostic.cpp`: testează modul dezactivat, concordanțe, abateri, format/MVT invalid, limita de observații, contabilizarea probelor de heap și izolarea erorilor.
- Build-ul minimal ESP32-S3 include acum instanțierea și folosirea efectivă a sesiunii `ShadowDiagnostic`, fără intercepția unui cadru radio.
- **Nu este încă o opțiune YAML ESPHome și nu s-a implementat callback-ul live din `wmbus_meter`**. Următorul pas, după CI verde, este adaptarea la un punct de captură care poate extrage *în siguranță* plaintext-ul deja decriptat din structura `Telegram`, evitând orice a doua decriptare, orice decodare pe firmware-ul de producție și orice schimbare a fluxului de senzori.
- Verificarea constă dintr-un singur workflow GitHub Actions pentru calup, cu teste host și un build ESP32-S3 izolat. Toate componentele `components/sx126x`, `components/sx127x`, `components/wmbus_radio`, `components/wmbus_common` și `components/wmbus_meter` rămân nemodificate.


### Validare Increment 18 — SUCCESS

[Workflow XMQ Stage A #14](https://github.com/radugeo74/esphome-sx1262/actions/runs/44108627499) — ambele joburi au trecut: suitele host, inclusiv `shadow_diagnostic`, și compilarea/linkarea ESP-IDF v5.5 pentru `esp32s3`. Baza radio a rămas nemodificată.

Logul `idf.py size` pentru imaginea **minimală** cu diagnostic: `xmq_stage_a_smoke.bin` = `0x86000` = 548864 octeți; dimensiune estimată 548741 octeți; DIRAM static 60839 octeți (17,8% din totalul raportat), rămas 280921 octeți. Nici acestea nu sunt măsurători de heap live și nici dimensiunea firmware-ului Heltec complet.

**Următoarea limitare de rezolvat:** capturarea sigură a datelor deja decriptate din pipeline-ul existent și eliminarea/izolarea excepțiilor C++ înainte de activarea în ESPHome. Nu instalați încă imaginea minimală.


## Increment 19 — gardă pentru capturarea sigură a payloadului din Telegram (17.11.2026)

Am inspectat sursele active `components/wmbus_common/meters.cc`, `wmbus.cc`, `wmbus.h`, `address.h` și `components/wmbus_meter/wmbus_meter.cpp`:

- `MeterCommonImplementation::handleTelegram()` setează `id_match=true` **înainte de** `Telegram::parse()`; un ID potrivit nu garantează decriptarea sau procesarea. Într-un eventual hook shadow trebuie păstrat și verificat explicit **bool-ul returnat de `handleTelegram`**, plus `!decryption_failed` și `!discard`.
- După procesarea reușită, `Telegram::frame` conține cadrul potențial decriptat, iar `Telegram::extractPayload()` folosește intervalul `frame[header_size:frame.size()-suffix_size]`. Motorul XMQ are nevoie strict de acest interval.
- `Telegram::addresses.back()` este identitatea interioară de referință (TPL, când există), nu neapărat cea de la DLL a convertorului radio.

Noul `xmq_telegram_capture.h` realizează o copie temporară numai după confirmarea explicită a succesului decodorului existent, refuză date incomplete/criptate, limite sau adrese invalide, folosește adresa interioară și filtrează strict CI-urile suportate în subsetul actual (proprietar 0xA0/0xA2, OMS 0x72/0x7A). Nu tratează CI-uri suplimentare prin presupuneri și nu face decriptare secundară. Există o limită de 1024 octeți pentru aplicația clară.

`test_xmq_telegram_capture.cpp` verifică limitele, CI, conversia producătorului, diferența dintre `id_match` și succesul real al decodorului, absența cheilor și respingerea datelor deja invalidate. Proiectul ESP32-S3 minimal compilează și instanțiază aceeași gardă într-un mock compatibil cu structura `Telegram`.

**Limitări:** captura este încă izolată, nu este apelată din `wmbus_meter`. Testul folosind o structură `Telegram` simulată nu garantează că legarea la structura reală și compatibilitatea cu toate modurile TPL sunt validate. Pentru a nu schimba traseul activ și a nu cere excepții C++ în firmware-ul stabil, callback-ul live și YAML-ul opt-in se adaugă doar după un build complet ESPHome izolat și auditarea accesului la plaintext.

Regula păstrată: **un singur workflow per calup**, fără modificări la componentele radio funcționale.


### Rezultat Increment 19 — GitHub Actions #15

[Workflow XMQ Stage A #15 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44113266367) — ambele joburi au trecut, inclusiv testul `telegram_capture` și compilarea ESP-IDF v5.5 pentru `esp32s3`. Nu au fost modificate componentele active ale receptorului.

Dimensiuni din `idf.py size` pentru smoke-ul minimal cu gardă: `xmq_stage_a_smoke.bin` = `0x866b0` = 550576 octeți; imagine totală raportată **550461 octeți**; DIRAM static **60839 octeți** (17,8% din 341760), rămas raportat **280921 octeți**. Aceste valori nu includ firmware-ul complet ESPHome și nici utilizarea de heap în timpul interpretării.

**Nu este încă disponibil un firmware nou instalabil pe Heltec.** Acest increment asigură doar capturarea corectă dintr-o structură cu forma `Telegram` în build-ul izolat. Următorul pas trebuie să fie testul cu API-ul `Telegram` real din `wmbus_common` și introducerea unui callback `shadow` opt-in într-un build ESPHome izolat, fără a modifica traseul activ și fără a considera `id_match` echivalent cu succesul decodării.


## Increment 20 — contract real Telegram + compilare Heltec completă, fără flash (17.11.2026)

- `test_xmq_real_telegram_contract.cpp` include chiar `components/wmbus_common/wmbus.h` din proiect, instanțiază `::Telegram` și `::Address` și exercită `capture_verified_payload` după un apel reușit, cu adresă interioară TCH și convertor exterior SEN. Verifică și că rezultatul decodorului vechi, câmpul `decryption_failed` și limitele header/suffix rămân respectate.
- Compilarea izolată ESP-IDF v5.5 pentru ESP32-S3 include acum același `::Telegram` real, nu doar un mock. `espidf-smoke/main/esphome/core/log.h` este **doar un shim de build** pentru declarația reală; nu se distribuie în ESPHome sau în componentele radio. Nu verifică simbolurile întregului decodor existent din firmware.
- Workflow-ul unic adaugă `heltec-esphome-build`: instalează ESPHome 2026.10.1 și compilează exclusiv configurația deja existentă `tests/sx126x-wmbus-bridge.yaml` pentru Heltec/SX1262. Această configurație nu conține un hook live XMQ și nu a fost modificată. Este o verificare a regresiilor radio/ESPHome, nu dovadă că XMQ este integrat în binarul complet.
- Păstrăm distincte trei verificări: motor host, captura cu declarația reală `Telegram` pe ESP32-S3, respectiv firmware-ul existent Heltec. **Niciunul nu are voie să fie considerat test hardware sau demonstrație de decodare AES live**.
- `components/wmbus_meter`, `components/wmbus_common`, `components/wmbus_radio`, `components/sx126x` și `components/sx127x` sunt protejate de comparația cu branch-ul de bază și nu se modifică în acest calup.

Următoarea problemă tehnică după verificarea CI: găsirea unei metode opt-in de a conecta captura după `handleTelegram` fără excepții C++ în firmware-ul ESPHome și fără a schimba observațiile din Home Assistant; nu activăm callback-ul live până la demonstrarea acestei compatibilități.

### Increment 20 — prima validare

[Workflow #16](https://github.com/radugeo74/esphome-sx1262/actions/runs/44117356831) a raportat FAILURE numai în testul `real_telegram_contract`: avertisment GCC `-Wunused-parameter` din constructorul privat existent în `components/wmbus_common/wmbus.h` a fost promovat la eroare de `-Werror`. Toate celelalte 19 suite host au trecut; joburile ESP-IDF și Heltec au fost sărite din cauza dependenței. Avertismentul a fost izolat prin `#pragma GCC diagnostic` doar în testul experimental, fără modificarea codului upstream. O singură revalidare separată va verifica corecția.

### Increment 20 — a doua validare

[Workflow #17](https://github.com/radugeo74/esphome-sx1262/actions/runs/44117972169) a trecut toate cele 19 teste existente, iar testul `real_telegram_contract` a compilat, dar nu a putut fi legat: `wmbus.h` conține obiectul static `NoDifVifKey("")`, care invocă `extractDV()` din `dvparser.cc`. Am adăugat **numai în executabilele izolate de test**, host și ESP-IDF, o implementare minimală care acceptă exclusiv cheia goală și refuză orice intrare DIF/VIF reală. Nu intră în firmware-ul ESPHome și nu înlocuiește funcția adevărată. Următoarea revalidare va putea continua către cele două compilări build-only.


### Increment 20 — validare completă a contractului real Telegram

[Workflow #18 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44118661664), commit `fd9241b4eeb43c91b848c6024c540fb6699daaef`: toate cele trei joburi au trecut.

- Host: 20 de suite trecute, inclusiv `real_telegram_contract`, cu declarațiile reale `::Telegram` și `::Address`, fără modificări la `wmbus_common`.
- ESP-IDF minimal 5.5/ESP32-S3: compilare și linkare cu `::Telegram` real și captura XMQ, dar cu shim `esphome/core/log.h` și stub `extractDV` pentru `NoDifVifKey("")` **numai în proiectul izolat**. `xmq_stage_a_smoke.bin` = `0x86d20` (552224 B), `idf.py size` 552105 B; DIRAM static 61151 B (17,89%).
- ESPHome 2026.10.1 Heltec/SX1262: `esphome config` și `esphome compile tests/sx126x-wmbus-bridge.yaml` **SUCCESS**. Imaginea de bază `sx126x-wmbus-bridge.bin` = `0x8ce10` (577040 B), `idf.py size` 569023 B, DIRAM static 57419 B (16,8%). Acesta este **firmware-ul existent de laborator, fără XMQ integrat**.

Cele două dimensiuni aparțin unor firmware-uri **diferite**: nu se însumează și nu reprezintă consumul unei integrări finale. Nu a existat flash pe dispozitive, măsurare heap live, interceptare decriptare AES sau validare RF.

**Pas rămas înainte de o versiune instalabilă:** integrarea opt-in a capturii și comparației în binarul ESPHome complet, într-o configurație izolată, plus rezolvarea excepțiilor C++ din motorul XMQ și verificarea comportamentului radio/heap. Până atunci nu instalați imaginea smoke pe Heltec.


## Increment 21 — primul firmware ESPHome Heltec cu XMQ inclus, numai build (17.11.2026)

- `components/xmq_shadow_probe/`: componentă ESPHome **experimentală, izolată**, ce include și leagă real `xmq_shadow_diagnostic.h` și motorul comun XMQ în **același firmware** în care există `wmbus_common`, `wmbus_meter`, `wmbus_radio` și SX1262. Nu se conectează la handlerul radio și nu modifică datele Home Assistant. Activarea opțională execută doar un **self-test cu date sintetice** la pornire, nu decodare live. Dezactivată implicit.
- `heltec-xmq-shadow-build.yaml`: copie de laborator a fixture-ului Heltec, cu ID de contor sintetic și `xmq_shadow_probe.enabled: false`; nu are Wi-Fi/API/AES key și nu trebuie instalată ca firmware de producție.
- Această etapă activează explicit **`CONFIG_COMPILER_CXX_EXCEPTIONS=y`** prin API-ul ESPHome pentru configurarea ESP-IDF, deoarece interpretul XMQ existent încă folosește excepții. Nu presupunem că acestea pot fi activate în firmware-ul stabil fără costuri sau incompatibilități.
- Workflow-ul existent conține un job suplimentar `heltec-xmq-shadow-build` pentru `esphome config` + `esphome compile` cu ESPHome 2026.10.1, în același CI cu suita de 20 teste host, compilarea XMQ ESP-IDF izolată și fixture-ul Heltec vechi. Verifică static că proba este dezactivată și că implementarea ei nu apelează handleri radio.
- **Limitări:** chiar un succes al acestei compilări validează doar coexistența în același binar ESPHome. Nu este un hook de captură după AES, nu există comparații live, nu se măsoară heap-ul de procesare și nu se autorizează flash pe Heltec.
- Următorul pas după un build verde: proiectarea unui callback opt-in după decodarea cu succes a `Telegram` (folosind valoarea returnată de `handleTelegram`, nu doar `id_match`) într-un branch distinct, cu comparații reale și limite de CPU/heap.


### Increment 21 — prima execuție și corecția de împachetare

[Workflow #19](https://github.com/radugeo74/esphome-sx1262/actions/runs/44122827101) a trecut cele 20 de suite host, ESP-IDF XMQ izolat și compilarea firmware-ului Heltec vechi. Numai **build-ul combinat Heltec+XMQ** a eșuat: configurația YAML s-a validat și compilarea a ajuns la `xmq_shadow_probe.cpp`, dar ESPHome native a raportat `fatal error: xmq_shadow_diagnostic.h: No such file or directory`. Opțiunea `-I` trimisă din componentă nu a adăugat sursele originale în directorul generat de compilare.

Corecție **strict de împachetare, fără modificarea motorului**: `tools/stage_shadow_headers.py` copiază toate fișierele `xmq_*.h` **nemodificate** lângă componenta experimentală înainte de compilarea YAML. Scriptul afișează SHA-256 prescurtat pentru fiecare sursă, iar `.gitignore` exclude copiile generate. Se rulează manual cu `python experimental/xmq-runtime-stage-a/tools/stage_shadow_headers.py` înainte de `esphome compile experimental/xmq-runtime-stage-a/heltec-xmq-shadow-build.yaml`. Workflow-ul face automat acest pas. Copiile nu sunt în Git și nu modifică sursele XMQ principale.

Atenție: fișierele sunt incluse în imaginea **de test**, iar activarea excepțiilor C++ rămâne experimentală; nu folosiți acest flux pentru actualizarea Heltec de producție. O singură revalidare corectivă va testa compilarea combinată.


### Increment 21 — a doua validare și coliziunea macro `error`

[Workflow #20](https://github.com/radugeo74/esphome-sx1262/actions/runs/44123982013) a trecut toate testele host, smoke-ul ESP32-S3 și build-ul Heltec vechi. În primul build **combinat**, împachetarea headerelor s-a rezolvat, dar compilarea `main.cpp` generat de ESPHome a eșuat: `components/wmbus_common/util.h` definește macro-ul C/C++ `error(...)`, care interfera cu metoda `Parser::error(...)` din `xmq_reader.h`.

Corecție de compatibilitate **strict la granița de includere**: `components/xmq_shadow_probe/xmq_compat_include.h` salvează macro-ul `error` (dacă există), îl dezactivează pe durata includerii headerelor originale XMQ și îl restaurează. Nici `wmbus_common`, nici parserul XMQ nu sunt modificate. Noul test `test_xmq_compat_macros.cpp` reproduce coliziunea și verifică restaurarea macro-ului, înainte de o singură revalidare a build-ului combinat.

Acesta rămâne un test de **coexistență la compilare**, nu o implementare a capturii live sau un firmware recomandat pentru instalare pe Heltec.


### Increment 21 — corecția includerii automate ESPHome

[Workflow #21](https://github.com/radugeo74/esphome-sx1262/actions/runs/44125233897): cele 21 de suite host, smoke-ul ESP32-S3 și build-ul Heltec de bază au trecut. Build-ul combinat a eșuat din nou deoarece **ESPHome include automat fiecare header .h al componentei în `esphome.h`**, iar includerea directă a lui `xmq_reader.h` avea loc înainte de wrapperul componentei. Macro-ul `error(...)` din `wmbus_common/util.h` afecta metoda `Parser::error(...)`.

Soluția fără modificarea parserului original: scriptul de pregătire adaugă **numai copiei temporare pentru build** a lui `xmq_reader.h` un `#pragma push_macro("error")` / `#undef error` înainte de codul original și `#pragma pop_macro("error")` după el. Restul fișierului rămâne byte-for-byte identic cu originalul. `test_xmq_staged_reader.cpp` reproduce includerea directă ESPHome cu macro-ul deja definit; rulează după pregătirea headerelor. Nicio componentă radio nu este modificată.

O singură revalidare corectivă, cu toate joburile în workflow-ul existent, urmează să confirme legarea binarului complet. Până la un rezultat verde, această etapă **nu** este pregătită pentru instalare.


### Increment 21 — VALIDAT: primul firmware ESPHome Heltec cu XMQ inclus

[Workflow #22 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44126705769), commit `bbc8f59c815ebac494246c09d329a50148da349a`: **toate cele patru joburi au trecut**, inclusiv `heltec-xmq-shadow-build`, primul build real ESPHome 2026.10.1/ESP32-S3/SX1262 care leagă în același binar motorul vechi wM-Bus și codul XMQ, cu `xmq_shadow_probe.enabled: false`. A trecut și noul test direct pentru includerea `xmq_reader.h` sub macro-ul `error`.

Dimensiuni `idf.py size` raportate de CI (același toolchain și aceeași configurație de bază, două binare distincte):

| Indicator | Heltec de bază | Heltec + XMQ (shadow dezactivat) | Diferență |
|---|---:|---:|---:|
| Imagine totală estimată | 569023 B | 665203 B | **+96180 B** |
| DIRAM static folosit | 57419 B | 57627 B | **+208 B** |
| DIRAM static rămas raportat | 284341 B | 284133 B | **−208 B** |

Acestea **nu sunt măsurători live de heap** și nu includ costul de memorie al încărcării și execuției driverelor XMQ la runtime. Folosirea excepțiilor C++ este activată explicit în configurarea build-ului de laborator și poate avea cost de flash și comportament diferit față de firmware-ul funcțional.

**Stare exactă:** Am obținut coexistența de build ESPHome + SX1262 + XMQ. Nu există încă captură live după decriptare, compare shadow în funcționare sau configurare de producție. Configurația experimentală nu are Wi-Fi/API și nu trebuie instalată ca înlocuitor pentru firmware-ul real. Recepția radio, `wmbus_common`, `wmbus_meter` și senzorii originali au rămas nemodificați.

**Următorul calup** este un hook opțional de observație după succesul complet al `handleTelegram()`, în continuare fără a permite modulului XMQ să modifice rezultatul sau să marcheze un cadru radio drept procesat. Pentru activarea efectivă pe Heltec este necesară validarea compilării și a consumului heap, apoi comparație pe telegrame reale.


## Increment 22 — hook post-decodare în copia izolată a `wmbus_meter`

Am adăugat o copie de laborator a componentei `components/wmbus_meter` în `experimental/xmq-runtime-stage-a/components/wmbus_meter/`, inclusiv platformele sensor și text_sensor. **Nu se modifică nimic în `components/` activ.** Copia este identică cu originalul, cu excepția `wmbus_meter.cpp`: după `handleTelegram(...)`, păstrează bool-ul `decoded` și invocă `xmq_shadow_probe::observe_legacy_telegram(*telegram, decoded)` înaintea ramurii existente `if (id_match)`. Nu se modifică `frame->mark_as_handled()`, actualizarea senzorilor sau gestionarea adreselor și cheilor.

`xmq_live_capture_hook.h` are numai declarația funcției, fără să expună parserul XMQ prin headerele contorului. `xmq_shadow_probe.cpp` definește un callback global **inert implicit**: numai dacă `enabled: true`, captează copia payloadului validat de parserul vechi (`decoded`, `!decryption_failed`, `!discard`, CI validat, limite de header/sufix), fără să păstreze identități, chei AES ori cadre, fără să altereze `Telegram`. Limitează observațiile la 128 pe boot și raportează doar numărul acceptărilor/refuzurilor și minimul heap liber observat. **Nu rulează încă driverele XMQ pe telegramele live și nu compară valori numerice**; acesta este un test al capturii și izolării, nu al parității de decodare.

`heltec-xmq-shadow-build.yaml` folosește copia de laborator `wmbus_meter` și `xmq_shadow_probe.enabled: false`. `tools/check_shadow_lab_meter.py` verifică exact că fiecare fișier din copia laboratorului este neschimbat, cu excepția hook-ului permis, și că opțiunea YAML rămâne dezactivată. Testul se execută în workflow înainte de comparația radio și de compilările ESP-IDF/Heltec. Păstrăm un singur workflow per calup.

Pentru un canary real urmează: build Heltec verde cu hook, configurație completă cu Wi-Fi/API/secrete locale fără publicarea lor, probă pe un dispozitiv de test cu backup de firmware și posibilitate de rollback, apoi verificarea traficului RF, a senzorilor existenți și a memoriei. **Nu flash-uiți configurația build-only fără Wi-Fi/API.**


### Increment 22 — SUCCESS: callback-ul post-decodare poate fi compilat în firmware-ul Heltec

[Workflow XMQ Stage A #23 — SUCCESS](https://github.com/radugeo74/esphome-sx1262/actions/runs/44131164851), commit `774ca02a8914c9a823cc566acbb851043edfe7da`: toate cele patru joburi au trecut, inclusiv verificarea strictă a copiei de laborator `wmbus_meter` și compilarea firmware-ului Heltec cu noul hook prezent, dar `xmq_shadow_probe.enabled: false`.

| Indicator (ESPHome 2026.10.1, ESP32-S3) | Heltec bază | Heltec + XMQ / hook dezactivat | Diferență |
|---|---:|---:|---:|
| `idf.py size` imagine | 569023 B | 666691 B | +97668 B |
| DIRAM static utilizat | 57419 B | 57683 B | +264 B |
| DIRAM static raportat rămas | 284341 B | 284077 B | -264 B |

**Concluzie strictă:** s-a demonstrat coexistența codului hook și a motoarelor în același firmware ESPHome. Nu s-a demonstrat încă decodarea efectivă XMQ pe telegrame radio live sau consumul de heap la captură. Hook-ul nu modifică rama/codul de decodare existent, `frame->mark_as_handled` și senzorii rămân în controlul bibliotecii vechi.

**Înainte de primul canary pe SX1262:** pregătiți un YAML complet cu Wi-Fi/API/OTA și contoarele reale, dar păstrați probe-ul dezactivat la prima instalare; validați funcționarea și rollback-ul, apoi activați `enabled: true` pe un dispozitiv de test pentru contorizarea capturilor. CI/compilarea build-only actuală nu autorizează încă flash. Ulterior încărcați sursele driverelor XMQ și comparați pe același payload clar, fără a modifica sursa senzorilor.


## Increment 23 — canary SX1262 cu conectivitate, pregătit numai pentru compilare (17.11.2026)

`heltec-xmq-canary.yaml` moștenește configurația Heltec/XMQ de laborator și adaugă **Wi-Fi, API Home Assistant criptat și OTA**, cu `wifi_ssid`, `wifi_password`, `heltec_api_key`, `heltec_ota_password` exclusiv din `!secret`. `experimental/xmq-runtime-stage-a/.gitignore` exclude `secrets.yaml`; **nu se publică date reale**. Pentru compilare manuală pe mașina locală trebuie create cheile reale în `experimental/xmq-runtime-stage-a/secrets.yaml` (fișier local, niciodată comis).

**ATENȚIE: nu instalați încă acest YAML.** Pachetul de laborator conține doar ID-ul de contor *sintetic* `0x12345678` și doi senzori de test; lipsește configurația privată reală cu toate repartitoarele și apometrele. Nu are strategia finală de rețea, numele actual al dispozitivului și nici backup/rollback confirmat pe hardware. `xmq_shadow_probe.enabled: false` la prima pornire; decodorul vechi rămâne responsabil de senzori.

Workflow-ul **existent** compilează mai întâi imaginea combinată precedentă și apoi `heltec-xmq-canary.yaml` într-un job de validare, injectând temporar numai parole și o cheie API complet fictive generate în runner; le șterge imediat după test. Niciun secret de producție nu intră în CI. CI confirmă exclusiv generarea imaginii și configurația validă, nu conectivitatea reală, recepția radio, memoria heap sau disponibilitatea OTA în rețeaua utilizatorului.

Pași rămași până la prima instalare canary: copierea locală a parametrilor legitimi din YAML-ul Heltec funcțional (nume, Wi-Fi, API/OTA, 12 contoare, chei AES, senzori), compilare pe țintă, salvarea firmware-ului vechi și validarea unei proceduri de revenire. Abia după verificare hardware se poate activa diagnosticul pentru maximum 128 de capturi, fără a publica date XMQ.
