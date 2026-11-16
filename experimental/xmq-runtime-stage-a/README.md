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

