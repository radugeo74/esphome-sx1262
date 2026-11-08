# Catalogul de securitate wM-Bus / OMS — 26 drivere native

**Revizie:** 2026-11-08. **Branch:** `experimental/xmq-multidriver-auto`. **Stadiu:** dovada software, nu certificare de echipament sau validare hardware.

Acest catalog descrie **ce poate decoda proiectul nostru**, nu daca toate contoarele cu acelasi nume emit necriptat. In XMQ, `NOKEY` indica doar lipsa cheii furnizate testului; unele exemple sunt deja decriptate. Identificarea AUTO nu ofera o cheie AES si nu autorizeaza singura publicarea.

Surse: [WMBusmeters — drivere XMQ](https://github.com/wmbusmeters/wmbusmeters/tree/master/drivers/src), [WMBusmeters — explicatia telegramelor logate](https://github.com/wmbusmeters/wmbusmeters), [OMS Group — produse certificate](https://oms-group.org/en/certified-products/).

## Capabilitati si dovezi publice

| Driver | Grup implementare | Semnaturi MVT XMQ | Fixture XMQ (din care NOKEY) | Fara cheie: implementarea noastra | Cu cheie: profiluri implementate |
|---|---|---|---:|---|---|
| [`fhkvdataiii`](drivers/fhkvdataiii.xmq) | `GENERATED` | `TCH/69/80`, `TCH/94/80` | 3 (3 `NOKEY`) | Proprietar CI A0/A2 | — |
| [`mkradio4`](drivers/mkradio4.xmq) | `GENERATED` | `TCH/95/62`, `TCH/70/62`, `TCH/95/72`, `TCH/70/72` | 1 (1 `NOKEY`) | Proprietar CI A0/A2 | — |
| [`iperl`](drivers/iperl.xmq) | `PORTABLE` | `SEN/68/06`, `SEN/68/07`, `SEN/7C/07` | 3 (3 `NOKEY`) | Profil clar suportat | — |
| [`hydrus`](drivers/hydrus.xmq) | `PORTABLE` | `DME/70/07`, `DME/76/07`, `HYD/24/07`, `HYD/8B/07`, `HYD/8B/06`, `DME/70/06`, `DME/70/16` | 7 (7 `NOKEY`) | Profil clar suportat | — |
| [`lansenpu`](drivers/lansenpu.xmq) | `PORTABLE` | `LAS/14/00`, `LAS/1B/00`, `LAS/0B/02` | 3 (3 `NOKEY`) | Profil clar suportat | — |
| [`lansendw`](drivers/lansendw.xmq) | `PORTABLE` | `LAS/07/1D` | 2 (2 `NOKEY`) | Profil clar suportat | — |
| [`cma12w`](drivers/cma12w.xmq) | `PORTABLE` | `ELV/20/1B`, `ELV/1F/1B` | 2 (2 `NOKEY`) | Profil clar suportat | — |
| [`ultraheat`](drivers/ultraheat.xmq) | `PORTABLE` | `LUG/04/04` | 3 (3 `NOKEY`) | Profil clar suportat | — |
| [`sontex868`](drivers/sontex868.xmq) | `PORTABLE` | `SON/16/08` | 1 (1 `NOKEY`) | Profil clar suportat | — |
| [`supercal`](drivers/supercal.xmq) | `PORTABLE` | `SON/1B/04` | 1 (1 `NOKEY`) | Profil clar suportat | — |
| [`q400`](drivers/q400.xmq) | `PORTABLE` | `AXI/01/07`, `AXI/10/07`, `AXI/20/07` | 5 (5 `NOKEY`) | Profil clar suportat | — |
| [`minomess`](drivers/minomess.xmq) | `CONFIGURED_NATIVE` | `ZRI/00/07`, `ZRI/01/16`, `ZRI/01/06` | 6 (6 `NOKEY`) | Profil clar configurat | OMS A / TPL Mode 5 |
| [`kamwater`](drivers/kamwater.xmq) | `CONFIGURED_NATIVE` | `KAM/1B/06`, `KAM/1B/16`, `KAW/3A/16`, `KAW/3C/16`, `KAW/3C/06`, `KAM/1D/16`, `KAW/42/06`, `KAW/42/16` | 14 (12 `NOKEY`) | Profil clar configurat | ELL AES-CTR; OMS D / TPL Mode 10 |
| [`apator162`](drivers/apator162.xmq) | `CONFIGURED_NATIVE` | `APA/05/06`, `APA/05/07` | 19 (18 `NOKEY`) | Profil clar configurat | OMS A / TPL Mode 5 |
| [`mkradio4a`](drivers/mkradio4a.xmq) | `SECURE_ONLY` | `HYD/FE/06`, `TCH/95/37` | 2 (2 `NOKEY`) | Nu | OMS A / TPL Mode 5 |
| [`topaseskr`](drivers/topaseskr.xmq) | `SECURE_ONLY` | `AMT/F1/06`, `AMT/F1/07` | 2 (2 `NOKEY`) | Nu | OMS A / TPL Mode 5 |
| [`lansenth`](drivers/lansenth.xmq) | `SECURE_ONLY` | `LAS/07/1B`, `LAS/09/1B` | 2 (2 `NOKEY`) | Nu | OMS A / TPL Mode 5 |
| [`lansensm`](drivers/lansensm.xmq) | `SECURE_ONLY` | `LAS/03/1A` | 2 (2 `NOKEY`) | Nu | OMS A / TPL Mode 5 |
| [`elvsense`](drivers/elvsense.xmq) | `PORTABLE` | `ELV/50/1B`, `ELV/51/1B`, `ELV/52/1B`, `ELV/53/1B`, `ELV/54/1B` | 3 (3 `NOKEY`) | Profil clar limitat | — |
| [`lansenrp`](drivers/lansenrp.xmq) | `PORTABLE` | `LAS/0B/32` | 2 (2 `NOKEY`) | Profil clar R4 | — |
| [`qsmoke`](drivers/qsmoke.xmq) | `PORTABLE` | `QDS/21/1A`, `QDS/23/1A`, `LSE/21/1A`, `LSE/23/1A` | 2 (2 `NOKEY`) | Doar CI 7A Mode 0 | — |
| [`qcaloric`](drivers/qcaloric.xmq) | `PORTABLE` | `LSE/34/08`, `LSE/35/08`, `QDS/35/08`, `QDS/34/08`, `QDS/36/08`, `QDS/3A/08`, `LSE/18/08`, `ZRI/FD/08` | 10 (10 `NOKEY`) | Doar CI 7A Mode 0 | — |
| [`rfmamb`](drivers/rfmamb.xmq) | `PORTABLE` | `BMT/10/1B` | 2 (1 `NOKEY`) | Temperatură + umiditate Mode 0 | — |
| [`sensostar`](drivers/sensostar.xmq) | `PORTABLE` | `EFE/00/04` | 4 (4 `NOKEY`) | Energie + volum Mode 0 | — |
| [`evo868`](drivers/evo868.xmq) | `PORTABLE` | `MAD/50/06`, `MAD/50/07`, `MAD/50/16` | 2 (2 `NOKEY`) | Total și set-date Mode 0 | — |
| [`supercom587`](drivers/supercom587.xmq) | `PORTABLE` | `SON/3C/06`, `SON/3C/07` | 2 (2 `NOKEY`) | Total BCD Mode 0 | — |

**Legenda:** un profil clar este admis numai cu verificarile de integritate, identitate si securitate aferente; nu inseamna «orice dispozitiv din familie este necriptat». Pentru profilele AES, cheia este configurata explicit in `secure_meters`, iar un cadru respins nu revine la decoderul plaintext. Cele doua drivere `SECURE_ONLY` pot fi doar recunoscute diagnostic fara cheie.

## Particularitati si limite pe driver

- **`fhkvdataiii`:** Exemple CI A0/A2; suport proprietar Techem, fara ruta OMS AES generica.
- **`mkradio4`:** Exemplu CI A2. NOKEY in XMQ nu inseamna certificare fara cheie pentru toate variantele.
- **`iperl`:** Un fixture upstream spune explicit ca telegrama este deja decriptata; altele au CF Mode 0.
- **`hydrus`:** Teste mixte TPL/ELL, unele cu antet Mode 5; decodorul PORTABLE accepta doar profiluri clare.
- **`lansenpu`:** Exemple NOKEY; nu se deduce comportamentul tuturor versiunilor LAS.
- **`lansendw`:** Exemple NOKEY; ruta locala decodifica numai profilul clar compatibil.
- **`cma12w`:** Exemple NOKEY cu separator in datele testului; statutul RF brut nu este certificat.
- **`ultraheat`:** Teste mixte M-Bus cablat si wireless; cablat nu demonstreaza RF fara criptare.
- **`sontex868`:** Exemplu NOKEY; fara ruta AES implementata pentru aceasta familie.
- **`supercal`:** Exemplu NOKEY; fara ruta AES implementata pentru aceasta familie.
- **`q400`:** XMQ include atat CF Mode 0 cat si antete Mode 5 cu continut furnizat lizibil.
- **`minomess`:** Teste cablate si TPL lung Mode 5 deja lizibil; AES testat software cu cheie.
- **`kamwater`:** Teste publice ELL CTR si CCM autentificat; rute AES distincte, fara fallback.
- **`apator162`:** Include exemplu public criptat AES Mode 5 si alte exemple NOKEY; profilele difera.
- **`mkradio4a`:** XMQ deja lizibil cu antet Mode 5; test AES recriptat sintetic. Convertor TCH/95/37 plus HYD/FE/06 ori WEH/FE/07 in TPL.
- **`topaseskr`:** XMQ NOKEY deja lizibil sub antet Mode 5; AES validat cu date recriptate sintetic.
- **`lansenth`:** Mode 5 cu cheie explicita, 2 fixture-uri post-decriptare; temperaturi si umiditate.
  Doua aplicatii XMQ lizibile cu antet TPL Mode 5; CBC testat prin recriptare cu cheie sintetica, nu RF real.
- **`lansensm`:** Mode 5 cu cheie explicita, 2 fixture-uri post-decriptare; stare fum numai ca telemetrie, NU alarma de incendiu certificata.
- **`elvsense`:** Doar exemplul ES1 este TPL Mode 0, cu temperatura si baterie. Celelalte doua NOKEY au antete Mode 5; nu se publica fara decriptare verificata.
- **`lansenrp`:** Doua exemple publice Mode 0, schema R4 cu stare, ascultare, contoare si baterie. Variantele neconfirmate sunt refuzate.
- **`qsmoke`:** Numai exemplul QSmokep CI 7A / Mode 0 este admis. Doar contoare istorice; NU este alarma de incendiu si nici verificarea unui detector certificat.
- **`qcaloric`:** Numai subsetul CI 7A, Mode 0, DIF/VIF standard HCA; exclude WalkBy CI 78, Mode 5 post-decriptat si variantele proprietare.
- **`rfmamb`:** Suport nativ numai pentru exemplul RFM-AMB Mode 0: 3 temperaturi si 3 umiditati; exemplul AES Mode 5 cu cheie nu are ruta nativa de decriptare.
- **`sensostar`:** Suport nativ limitat la doua exemple radio Mode 0: energie kWh, volum m3 si target kWh optional; M-Bus cablat si exemplul AES Mode 5 NOKEY sunt excluse.
- **`evo868`:** Numai doua exemple publice TPL Mode 0 cu total si consum la data de referinta. Istoricul extins si variantele necunoscute nu sunt decodate.
- **`supercom587`:** Numai profilul clar Sontex 587 demonstrat de doua exemple publice apa rece/calda. Total BCD m3 si stare OK numai dupa verificarea formatului.
  Doua aplicatii XMQ postdecriptate Mode 5; smoke_detected este telemetrie, NU detector de incendiu certificat in Home Assistant.

## Audit 6: dovezile `NOKEY` nu sunt capturi RF clare (2026-11-08)

Am inspectat automat toate cele **105** de fixture XMQ din cele 16
drivere: **101** ruleaza fara cheie in test, dar **40** dintre cele
`NOKEY` pastreaza in antet un **TPL Mode 5 / AES-CBC**. Inca un
fixture Apator162 are Mode 5 cu cheie explicita (deci **42** antete
Mode 5). Nu le etichetam ca RF fara cheie: unele contin date deja
decriptate dupa `_`, altele sunt cadre criptate din teste.

Un exemplu `iperl` declara **M=0, N=1** in CF, combinatie invalida
pentru Mode 0 conform OMS, desi fixture-ul foloseste `NOKEY`.
Alt exemplu iPERL cu **M=0, N=0** este acceptat de ruta nativa
plaintext. Nu interzicem profilurile realmente necriptate.

**Corectie de securitate descoperita in aceasta etapa:** decodorul
direct `iperl::decode` verifica anterior numai 4 biti ai lui M si
nu verifica N; putea citi offline exemplul M=0,N=1 ca si cum ar fi
un cadru necriptat. Publicarea prin receptor era deja blocata.
Am unit verificarea iPERL cu `tpl_security::inspect` (M pe 5 biti,
M=0,N=0 obligatorii), am adaugat un al doilea control in
`decode_configured_plain_frame` si am separat
`iperl::decode_clear_application` numai pentru teste offline
cu date deja lizibile. Nu au fost modificate telegramele XMQ.

**Garantiile din CI:** `test_nokey_fixture_provenance.py` analizeaza
fiecare sursa XMQ, fara sa deduca securitatea din `NOKEY` sau
`2F2F`. `test_nokey_rf_fail_closed.cpp` foloseste chiar
`tpl_security::inspect`, `publication::inspect` si
`batch2_plain::decode_configured` pentru fixture-urile Q400,
Topas, MK Radio4a, Minomess si iPERL. Orice antet Mode 5
este respins pe ruta clara, chiar daca dupa separator se afla
date deja lizibile. Aceasta verificare se adauga testelor AES
cu cheie explicita de la punctul 4.

**Limita:** sintaxa XMQ nu confirma niciodata singura ce a transmis
fizic contorul. CI necunoscut, ELL, antet TPL incomplet si moduri
proprietare raman `unknown` pana la probe reale, nu sunt clasificate
automat ca `fara cheie`.

## OMS Group: certificare versus implementare

OMS Group publica certificarea pe **produs comercial, versiune/generatie si profil**. MVT (`producator/versiune/tip`) nu identifica sigur acel SKU; de aceea **niciunul dintre cele 26 drivere nu are asociat un certificat OMS confirmat** (`oms_certified_sku: null`). Nu am dedus certificari din numele unui driver.

Profilurile OMS pe care le-am implementat pentru cazuri specifice sunt **A / AES-CBC Mode 5** si **D / AES-CCM Mode 10**. Implementarea ELL AES-CTR este un traseu separat, care nu trebuie reclasificat automat ca profil A/B/D. **Mode 7 (OMS B) si Mode 13** sunt detectate de filtrul de securitate, dar nu au aici un decodor general. Aceasta este o harta de compatibilitate, nu o lista de contoare care sigur nu necesita cheie.

## Ce ar confirma realmente «fara cheie»

1. Telegrama RF originala, cu CRC corect si antetele complete, capturata fara pre-decriptare.
2. Interpretarea securitatii din TPL/ELL/CF pe cadrul respectiv, nu doar din CI sau din `NOKEY`.
3. Decodare completa a valorilor reale fara cheie si potrivirea producatorului, versiunii si tipului.
4. Daca exista model comercial exact, identificare OMS independenta cu certificat si generatie; fara a incarca date private in GitHub.

**Sursa de adevar:** [`security_profile_catalog.json`](security_profile_catalog.json). CI executa [`test_security_profile_catalog.py`](tests/test_security_profile_catalog.py), care verifica exact cele 26 intrari, manifestul nativ, semnaturile/fixture XMQ si lista capabilitatilor AES. Nu am schimbat runtime-ul, cheile, decodoarele sau firmware-ul.
