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
