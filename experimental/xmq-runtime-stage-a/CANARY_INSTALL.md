# Heltec SX1262 — prima instalare canary (XMQ dezactivat)

**Stare:** procedură de pregătire, **NU autorizare automată de flash**.
Scop: păstrarea radio + decodor legacy și a configurației private funcționale;
XMQ este încărcat, dar nu primește valori și nu publică entități.

## Ce este deja verificat în GitHub CI

- Workflow #62 (commit `b0f9e15c5641`) a trecut toate cele patru joburi.
- ESPHome 2026.10.1 compilează separat imaginea Heltec cu XMQ dezactivat și
  overlay-ul **7 repartitoare + 5 apometre / 50 senzori**, folosind exclusiv
  ID-uri și chei *sintetice*.
- Parserul legacy a executat `parseHeader()` și `parse()` pe o telegramă
  sintetică DV necriptată. Nu înseamnă că driverul XMQ decodează contoarele reale.

## 1. Pregătește pe calculator, NU în GitHub

Folosește o clonă locală a ramurii
`experimental/xmq-runtime-stage-a`. **Nu încărca în Git**
`secrets.yaml`, YAML-ul funcțional cu ID-uri/chei sau fișierele de
firmware private.

Fă o copie separată a YAML-ului **care funcționează acum** (referința) și
păstrează aplicația ESP32-S3 OTA `.bin` corespunzătoare drept rollback.
Verifică faptul că acel binar a fost efectiv folosit pe placa respectivă;
un binar generat de GitHub CI cu date sintetice **nu** este rollback.

Copiază working YAML într-un fișier separat pentru candidat și adaugă
numai componenta XMQ opțională în **stare dezactivată**. Nu schimba numele
nodului, `api`, OTA, Wi-Fi, pinii SPI/radio, modul T1/C1, cele 12 contoare,
AES, câmpurile sau numele senzorilor.

**Atenție:** `heltec-xmq-canary-12-meters.yaml` este o demonstrație
*build-only*, nu o configurație garantat echivalentă cu YAML-ul tău actual.
Are modul `T1` și nume de senzori de laborator. Nu-l instala direct peste
nodul funcțional și nu deduce echivalența din faptul că trece CI.

## 2. Verifică separat sursele `external_components`

Pentru prima instalare, `xmq_shadow_probe` trebuie să apară în
`external_components`, iar configurația lui trebuie să aibă literal
`enabled: false`. Rulează și verificarea clonei laborator:
`python experimental/xmq-runtime-stage-a/tools/check_shadow_lab_meter.py`.

Compară manual adresele, commiturile și componentele de pe sursele
`external_components` ale versiunii bune și ale canary-ului. Scriptul de
preflight poate detecta structura, **nu poate demonstra** că două surse
Git/GitHub livrează aceleași implementări.

## 3. Compilează candidatul privat, fără flash

Folosește versiunea ESPHome fixată în CI, `2026.10.1`, și execută local
`esphome config`, urmat de `esphome compile`, pentru YAML-ul candidat.
Nu publica logurile dacă includ date sensibile.

Păstrează aplicația OTA `.bin` rezultată, nu `bootloader.bin`,
`partitions.bin` sau o imagine combinată. Verifică manual locația și
tipul fișierului din ieșirea compilării.

## 4. Poarta locală unificată (nu face flash)

Din rădăcina clonei:

```bash
python experimental/xmq-runtime-stage-a/tools/canary_release_gate.py \
  --reference /cale/privata/heltec-working.yaml \
  --candidate /cale/privata/heltec-xmq-canary.yaml \
  --fallback-bin /cale/privata/heltec-working-firmware.bin \
  --canary-bin /cale/privata/heltec-xmq-canary-firmware.bin \
  --manifest /cale/privata/canary-release-new.json
```

Acest utilitar apelează în ordine `preflight_canary.py` și
`verify_rollback.py`: respinge diferențe ale configurației operaționale,
refuză imagini non-ESP32-S3/identice/supradimensionate, apoi scrie un
manifest local cu hash-uri SHA-256 și permisiuni restrictive. Nu accesează
dispozitivul, nu modifică firmware-ul și nu arată valorile private.

Dacă apare `CANARY_RELEASE_BLOCKED`, **nu instala**. Nu ocoli controlul
modificând YAML-ul funcțional doar ca să treacă testul.

## 5. Decizia de flash și testul de teren

Prima instalare este permisă **numai după aprobare manuală**:
- Ai un dispozitiv canary și acces USB pentru recuperare.
- Ai verificat fizic placa Heltec, partițiile și imaginea de rollback.
- Cele 12 contoare și toți senzorii sunt identici cu referința.
- Radio, T1/C1 și componentele externe sunt revizuite.
- Nu există niciun consumator XMQ activ: `enabled: false`.

După o eventuală instalare separată, compară recepția, numărul de
telegrame, RSSI, heap și fiecare senzor cu referința. Nu activa XMQ ca
decodor principal și nu elimina driverul legacy doar fiindcă această
variantă pornește.

**De finalizat pentru migrarea XMQ activ:** test integrat
`handleTelegram()` cu parserul real și observator, comparație read-only
`fhkvdataiii` + `mkradio4`, apoi verificare RF T1/C1 pe dispozitiv.
