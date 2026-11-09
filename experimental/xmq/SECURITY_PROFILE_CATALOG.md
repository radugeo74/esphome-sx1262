# Catalogul de securitate wM-Bus / OMS — 50 drivere native

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
| [`hcae2`](drivers/hcae2.xmq) | `PORTABLE` | `EFE/31/08` | 5 (5 `NOKEY`) | HCA curent/target, T1 Mode 0 | — |
| [`bfw240radio`](drivers/bfw240radio.xmq) | `PORTABLE` | `BFW/02/08` | 6 (6 `NOKEY`) | HCA curent/precedent Mode 0 | — |
| [`qwaterv2`](drivers/qwaterv2.xmq) | `PORTABLE` | `QDS/33/37`, `QDS/35/06`, `QDS/35/07`, `QDS/35/37`, `QDS/16/06`, `QDS/16/07`, `QDS/17/06`, `QDS/17/07`, `QDS/18/06`, `QDS/18/07`, `QDS/19/06`, `QDS/19/07`, `QDS/1A/06`, `QDS/1A/07`, `QDS/1D/06`, `QDS/1D/07`, `QDS/36/06`, `QDS/36/07`, `LSE/19/07`, `LSE/1A/06`, `LSE/1A/07`, `LSE/1D/06`, `LSE/1D/07`, `LSE/33/37`, `LSE/35/37`, `LSE/36/06`, `LSE/36/07` | 23 (21 `NOKEY`) | Doar CI 7A Mode 0 | — |
| [`hydrodigit`](drivers/hydrodigit.xmq) | `PORTABLE` | `BMT/13/06`, `BMT/17/06`, `BMT/13/07`, `BMT/15/07`, `BMT/17/07` | 6 (6 `NOKEY`) | Doar CI 7A Mode 0 | — |
| [`itron`](drivers/itron.xmq) | `PORTABLE` | `ITW/00/07`, `ITW/03/07`, `ITW/33/07`, `ITW/00/16` | 4 (4 `NOKEY`) | Consum total și target, Mode 0 | — |
| [`qualcosonic`](drivers/qualcosonic.xmq) | `PORTABLE` | `AXI/0B/0D`, `AXI/0C/0D` | 3 (3 `NOKEY`) | Volum/temperaturi/putere, Mode 0 | — |
| [`lse_08`](drivers/lse_08.xmq) | `PORTABLE` | `LSE/01/08` | 1 (1 `NOKEY`) | HCA la data de referință, Mode 0 | — |
| [`waterstarm`](drivers/waterstarm.xmq) | `PORTABLE` | `DWZ/00/06`, `DWZ/02/06`, `DWZ/02/07`, `EFE/03/07`, `EFE/70/07`, `DWZ/00/07` | 8 (5 `NOKEY`) | Volum total și la data de referință, Mode 0 | — |
| [`lse_07_17`](drivers/lse_07_17.xmq) | `PORTABLE` | `LSE/18/06`, `LSE/18/07`, `LSE/16/07`, `LSE/17/07`, `LSE/D8/07` | 4 (4 `NOKEY`) | Volum / volum billing, Mode 0; vD8 numai explicit | — |
| [`munia`](drivers/munia.xmq) | `PORTABLE` | `WEP/02/1B`, `WEP/04/1B` | 2 (2 `NOKEY`) | Temperatură / umiditate, Mode 0 | — |
| [`qheat_55_us`](drivers/qheat_55_us.xmq) | `PORTABLE` | `LUG/07/04`, `LUG/0A/04` | 4 (4 `NOKEY`) | Energie și consum la billing (kWh), Mode 0; volum opțional | — |
| [`maddalena`](drivers/maddalena.xmq) | `PORTABLE` | `MAD/01/07`, `MAD/01/06` | 1 (1 `NOKEY`) | Volum total și billing, doar MAD/01/07 Mode 0 | — |
| [`itronheat`](drivers/itronheat.xmq) | `PORTABLE` | `ITW/00/04` | 2 (2 `NOKEY`) | Energie, volum și energie an anterior, doar Mode 0 | — |
| [`ehzp`](drivers/ehzp.xmq) | `PORTABLE` | `EMH/02/02`, `EMH/00/02` | 2 (2 `NOKEY`) | Energie consumată, produsă și putere, doar EMH/00/02 Mode 0 | — |
| [`qheatv2`](drivers/qheatv2.xmq) | `PORTABLE` | `QDS/23/04`, `QDS/23/37`, `QDS/3C/04`, `QDS/3C/37`, `QDS/3E/04`, `QDS/3E/37`, `QDS/46/04`, `QDS/46/37`, `QDS/47/04`, `QDS/47/37`, `QDS/48/04`, `QDS/48/37`, `QDS/3F/37`, `LSE/23/04`, `LSE/23/37`, `LSE/3E/04`, `LSE/3E/37`, `LSE/46/04`, `LSE/46/37`, `LSE/47/04`, `LSE/47/37`, `LSE/48/04`, `LSE/48/37` | 10 (8 `NOKEY`) | Energie totală/an referință, numai LSE/23/04 Mode 0 | — |
| [`c5isf`](drivers/c5isf.xmq) | `PORTABLE` | `ZRI/88/0D`, `ZRI/88/07`, `ZRI/88/04` | 3 (3 `NOKEY`) | Energie/volum și eroare reală, ZRI/88/0D Mode 0 | — |
| [`microclima`](drivers/microclima.xmq) | `PORTABLE` | `MAD/00/04` | 2 (2 `NOKEY`) | Energie, volum, temperaturi opționale, Mode 0 | — |
| [`abbb23`](drivers/abbb23.xmq) | `PORTABLE` | `ABB/20/02` | 1 (1 `NOKEY`) | Energie importată totală, Mode 0 | — |
| [`ei6500`](drivers/ei6500.xmq) | `PORTABLE` | `EIE/0C/1A` | 6 (6 `NOKEY`) | Doar contoare diagnostic istorice (NU alarmă incendiu), Mode 0 | — |
| [`apatoreitn40`](drivers/apatoreitn40.xmq) | `PORTABLE` | `APA/09/08`, `APA/0A/08`, `APA/0D/08`, `APA/0E/08`, `APT/09/08`, `APT/0A/08`, `APT/0D/08`, `APT/0E/08` | 3 (3 `NOKEY`) | Valori HCA + temperaturi, doar APA/09/08 și APA/0D/08 CI A0 în clar | — |
| [`elf`](drivers/elf.xmq) | `PORTABLE` | `APA/40/04` | 1 (1 `NOKEY`) | Energie, volum, temperaturi CI72 Mode0, cu DLL/TPL distincte | — |
| [`engelmann_faw`](drivers/engelmann_faw.xmq) | `PORTABLE` | `EFE/00/07` | 2 (1 `NOKEY`) | Consum la raportare și două valori istorice CI72 Mode0, DLL/TPL distincte; AES respins | — |
| [`apatoreitn`](drivers/apatoreitn.xmq) | `PORTABLE` | `APA/04/08`, `APT/04/08` | 3 (3 `NOKEY`) | HCA/temperaturi și stare ESB, numai A0 și două forme B6 validate | — |
| [`gransystems`](drivers/gransystems.xmq) | `PORTABLE` | `GSS/01/02` | 2 (2 `NOKEY`) | Consum total electric și tarife 1/2, CI78 exact, fără TPL | — |

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
- **`hcae2`:** Doua exemple T1 Mode 0 originale confirma HCA 0/0; C1 include CRC separat. ActiveLong cu 242/373 este antet AES Mode 5 deja lizibil in XMQ, respins pe RF. Test numeric 242/373 numai cu antet Mode 0 sintetic, etichetat non-RF.
- **`bfw240radio`:** Sase exemple BFW 240 radio Mode 0, structura producator fixa 55 octeti, curent si precedent HCA. Istoricul proprietar 18 luni si alte formate raman excluse.
- **`qwaterv2`:** Subset standard CI 7A Mode 0 pentru total_m3, target_year_m3 si target_m3, din 8 exemple publice. CI 78 WalkBy, CI 72 si doua exemple cu cheie explicita nu intra in ruta radio plaintext.
- **`hydrodigit`:** Patru exemple publice CI 7A Mode 0 pentru volum total. Un exemplu NOKEY cu Mode 5 AES si unul cu CI 8C ELL sunt excluse; campurile proprietare raman nedecodate.
- **`itron`:** Doua exemple Itron ITW Mode 0 permit total_m3 si target_m3. Celelalte doua fixture NOKEY sunt ambele AES Mode 5: unul CI 72 TPL lung si unul CI 7A TPL scurt; respinse la publicare.
- **`qualcosonic`:** Trei exemple originale Qualcosonic Mode 0: total_m3, flow_temperature_c, return_temperature_c si power_kw. Energiile de incalzire/racire si set-date au modificatori VIF nesuportati in acest subset.
- **`lse_08`:** Un exemplu public TPL Mode 0 LSE/01/08: consumption_at_set_date_hca=321 si status TPL_STS_128 (nu OK). Fara dovezi pentru consumul curent sau alte modele; variantele necunoscute nu se publica.
- **`waterstarm`:** Doar doua exemple radio DWZ/00/06 si DWZ/00/07 cu TPL Mode 0 si 151 octeti: total_m3 si consumption_at_set_date_m3. Patru exemple TPL Mode 5, dintre care doua NOKEY deja decriptate, si doua ELL sunt excluse; istoricul lunar nu este publicat.
- **`lse_07_17`:** Patru fixture TPL Mode 0 publice LSE: doua profile storage-8 (consumption_at_set_date_8_m3), doua total/due (total_m3 si due_m3). Versiunea DLL D8/07 este inclusa explicit in detect.mvt upstream si poate fi recunoscuta prin AUTO configurat; nu se publica date/alarme neverificate.
- **`munia`:** Doua fixture TPL Mode 0 publice WEP/02-04/1B cu 47 octeti, temperatura si umiditate din BCD standard: current_temperature_c si current_relative_humidity_rh; status OK doar pentru doua antete/fanioni zero verificati. Fara variante decriptate sau presupuneri despre alte modele.
- **`qheat_55_us`:** Patru exemple originale LUG/07-0A/04 CI 7A Mode 0 confirma total_energy_consumption_kwh si key_datetime_kwh; total_m3 este optional doar cand exista in telegrama. Un exemplu contine alarma F0 reala: nu publicam stare OK si nu interpretam semnalele de eroare ori datele istorice nevalidate.
- **`maddalena`:** Un singur exemplu original MAD/01/07 CI 7A Mode 0 (79 octeti) confirma total_m3=0.167 si target_m3=0. Fabrication 0E78 contine sentinel FF si nu poate fi decodat ca BCD numeric; verificam structura dar decodam separat doar cele doua volume. MAD/01/06 exista doar in detect.mvt, fara dovada de profil acceptat.
- **`itronheat`:** Un fixture Itron/Allmess ITW/00/04 original CI 7A Mode 0 (65 octeti) confirma total_kwh=19529, total_m3=8401.224 si last_year_kwh=13978. Alt fixture NOKEY are antet AES Mode 5 si aplicatie deja decriptata, exclus RF. Statutul OK din XMQ nu este extrapolat la alte variante. Exemplul original clar are M-field 0xA697 (ITW cu bit15 activ), fata de codul 0x2697 din detect.mvt; AUTO permite numai aceasta varianta documentata, cu profil si ID validate.
- **`ehzp`:** Un fixture EMH/00/02 original CI 7A Mode 0 (175 octeti) confirma total_energy_consumption_kwh=95.7722, total_energy_production_kwh=0.0021 si current_power_consumption_kw=0.09. Celalalt fixture NOKEY este ELL CI 8C cu aplicatie furnizata deja in clar, exclus din aceasta ruta. Variantele cu istoric/tarife nu sunt publicate.
- **`qheatv2`:** Din cele zece exemple originale QHeat V2, doar ultimul LSE/23/04 transmite CI 7A Mode 0 cu 47 octeti: total_kwh=12560.3 si target_year_kwh=11132. M-field este 0xB265 (LSE cu bit15 activ) si AUTO accepta strict acea varianta; CI 72, CI 78, WalkBy si exemple cu cheie raman excluse. Status OK este publicat numai pentru acest profil cu TPL status 0.
- **`c5isf`:** Un exemplu original ZRI/88/0D de 230 octeti Mode 0 confirma total_energy_consumption_kwh=0, total_volume_m3=0 si total_energy_consumption_last_month_kwh=0. Contorul are status DRY ERROR RETURN_SENSOR_INTERRUPTED, nu OK. Celelalte doua exemple au antete AES TPL Mode 5, desi sunt NOKEY in XMQ; nu sunt publicate fara decriptare. Marcatorii istorici invalidi sunt exclusi.
- **`microclima`:** Doua exemple originale MAD/00/04 CI 7A Mode 0 cu 74/165 octeti confirma energia totala (1805/2119 kWh) si volumul total (297.994/241.757 m3). Primul sustine si temperaturile tur/retur 21/21 C; al doilea transmite istoric separat, nepublicat. Structura, MVT, ID si transportul sunt verificate strict; nu inventam alarme si date istorice.
- **`abbb23`:** Un exemplu ABB/20/02 CI 7A Mode 0 de 133 octeti confirma total_energy_consumption_kwh=5249.17 din BCD6 DIF0E/VIF8400. Restul tarifelor, fluxurilor, contoarelor, textelor si alarmelor nu sunt publicate fara dovezi suplimentare. Un singur fixture nu este dovada paritatii pentru toate ABB B23.
- **`ei6500`:** Sase fixture originale Ei Electronics EIE/0C/1A (89 octeti CI 7A Mode 0) demonstreaza doar contoare istorice de alarme/teste/demontare si nivelul de praf: alarm_counter, removed_counter, test_button_counter, dust_level_counter. Nu interpreteaza siguranta actuala, fum/foc, nu publica status OK si nu trebuie utilizat pentru protectie la incendiu.
- **`apatoreitn40`:** Trei telegrame originale APA/09/08 si APA/0D/08 (65 octeti, CI A0, 27 cuvinte little-endian) valideaza current_hca, previous_hca, previous_2y_hca, previous_3y_hca, current_month_hca, previous_month_hca si temperaturi camera/radiator. Versiunile APA/0A/08 si APA/0E/08 sunt criptate AES si nu sunt acceptate de parserul clar. APT detect.mvt nu inseamna profil publicat. Doar variantele si structura validate sunt admise.
- **`elf`:** Un singur exemplu original APA/40/04, 82 octeti, CI72 Mode0, DLL 00050901 si TPL 01885619 diferite: energie totala 3112.49977 kWh, volum 201.364 m3, referinta 3047.8 kWh, tur 69 C, retur 58 C. Antetul XMQ are ERROR_FLAGS_2000000; nu publicam status OK. STRICT doua ID-uri configurate, fara relaxarea gate-ului normal.
- **`engelmann_faw`:** Un singur exemplu original EFE/00/07, 144 octeti, CI72 Mode0, DLL 43000255 si TPL 20254060 diferite: consumption_at_reporting_date_m3=59.682, consumption_1_months_ago_m3=78.908 si consumption_2_months_ago_m3=77.230. Nu este disponibil total_m3 in aceasta telegrama; istoricul complet este validat structural, dar nepublicat. Un alt exemplu CI73 AES este exclus; strict doua ID-uri configurate.
- **`apatoreitn`:** Trei exemple originale Apator E-ITN APA/04/08 (CI A0/26 octeti) si APT/04/08 (CI B6/38 si 42 octeti, prefix A0 strict incadrat). Decodare HCA curent/an anterior si temperaturile medii camera 21.703125/19.890625, 25.78125/22.390625, respectiv 21.703125/19.890625 C. Status SEAL_BROKEN sau OK numai dupa verificarea stricta a datei ESB. CI B6 nu este acceptat generic si nu implica TPL Mode0 sau AES; sunt suportate numai aceste doua variante originale.
- **`gransystems`:** Doua exemple reale GSS/01/02 CI 78 (121/159 octeti, fara antet TPL) confirma energie totala 0.916/0 kWh si tarife 1=0.873/0, 2=0.043/0 kWh. Parserul dedicat admite numai cele doua structuri originale, cu ID DLL, MVT si DIF/VIF validate. Nu exista exceptie generala CI78 in gate-ul OMS; nu se publica status OK, deoarece exemplul trifazat raporteaza RTC_ERROR.
  Doua aplicatii XMQ postdecriptate Mode 5; smoke_detected este telemetrie, NU detector de incendiu certificat in Home Assistant.

## Audit 6: dovezile `NOKEY` nu sunt capturi RF clare (2026-11-08)

Am inspectat automat toate cele **209** de fixture XMQ din cele 50
drivere: **197** ruleaza fara cheie in test, dar **49** dintre cele
`NOKEY` pastreaza in antet un **TPL Mode 5 / AES-CBC**. Inca un
fixture Apator162 are Mode 5 cu cheie explicita (deci **53** antete
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

OMS Group publica certificarea pe **produs comercial, versiune/generatie si profil**. MVT (`producator/versiune/tip`) nu identifica sigur acel SKU; de aceea **niciunul dintre cele 50 drivere nu are asociat un certificat OMS confirmat** (`oms_certified_sku: null`). Nu am dedus certificari din numele unui driver.

Profilurile OMS pe care le-am implementat pentru cazuri specifice sunt **A / AES-CBC Mode 5** si **D / AES-CCM Mode 10**. Implementarea ELL AES-CTR este un traseu separat, care nu trebuie reclasificat automat ca profil A/B/D. **Mode 7 (OMS B) si Mode 13** sunt detectate de filtrul de securitate, dar nu au aici un decodor general. Aceasta este o harta de compatibilitate, nu o lista de contoare care sigur nu necesita cheie.

## Ce ar confirma realmente «fara cheie»

1. Telegrama RF originala, cu CRC corect si antetele complete, capturata fara pre-decriptare.
2. Interpretarea securitatii din TPL/ELL/CF pe cadrul respectiv, nu doar din CI sau din `NOKEY`.
3. Decodare completa a valorilor reale fara cheie si potrivirea producatorului, versiunii si tipului.
4. Daca exista model comercial exact, identificare OMS independenta cu certificat si generatie; fara a incarca date private in GitHub.

**Sursa de adevar:** [`security_profile_catalog.json`](security_profile_catalog.json). CI executa [`test_security_profile_catalog.py`](tests/test_security_profile_catalog.py), care verifica exact cele 50 intrari, manifestul nativ, semnaturile/fixture XMQ si lista capabilitatilor AES. Nu am schimbat runtime-ul, cheile, decodoarele sau firmware-ul.
