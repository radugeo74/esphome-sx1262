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
