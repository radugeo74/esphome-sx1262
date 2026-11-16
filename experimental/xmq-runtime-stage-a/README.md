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
