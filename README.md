# Native wM-Bus / XMQ on ESPHome SX126x

**Experimental Heltec ESP32-S3 + SX1262 receiver** for wireless M-Bus
T1 and C1, with a native C++ parsing/decoding pipeline and configurable
Home Assistant sensors.

**Active branch:** `experimental/xmq-multidriver-auto`  
**Last fully green code CI — secure-delivery audit point 4:** [GitHub Actions #42459485622](https://github.com/radugeo74/esphome-sx1262/actions/runs/42459485622)
(code commit `1efb2ea455d645e193f09f33550a06f0235e1778`, all three jobs passed).

> This branch is a distinct native implementation. It **does not use
> `wmbus_radio` / `wmbus_meter` or the old
> `Meter::handleTelegram()` pipeline as its live receiver/decoder**.
> Earlier experiments in this repository used different components; do
> not copy their YAML into this branch. The current architecture and
> safety rules are recorded in [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).

## Catalog de securitate (audit punctul 5)

[**Catalogul wM-Bus / OMS**](experimental/xmq/SECURITY_PROFILE_CATALOG.md)
descrie pentru toate cele 16 drivere MVT-urile, fixture-urile XMQ
si rutele clare/securizate implementate, fara sa presupuna ca
`NOKEY` inseamna RF necriptat. Datele structurate sunt in
[`security_profile_catalog.json`](experimental/xmq/security_profile_catalog.json).
CI verifica automat catalogul fata de manifest si sursele XMQ.
Nu atribuim certificate OMS pana nu este identificat modelul comercial exact.
Documentatie si CI numai; nu afecteaza decoderul sau firmware-ul.

### Audit punctul 6: siguranta `NOKEY` versus RF (2026-11-08)

Dintre 74 de fixture XMQ, 71 sunt `NOKEY`, insa **31 dintre
acestea au antet TPL AES Mode 5**. Aparitia `NOKEY` sau a
markerului `2F2F` nu permite decodarea fara cheie.
[Testul de provenienta](experimental/xmq/tests/test_nokey_fixture_provenance.py)
si [testul de securitate RF](experimental/semtech-radio/test_nokey_rf_fail_closed.cpp)
ruleaza in CI: exemplele deja lizibile din Q400/Minomess/Topas/
MK Radio4a nu pot ocoli `publication::inspect` sau
`batch2_plain::decode_configured`. Un iPERL Mode 0 valid ramane
acceptat; un Mode 0 cu N nenul este respins.

**Descoperire prin regresie:** vechiul `iperl::decode()` verifica
numai 4 biti ai modului, fara N. Desi publicarea normala era
blocata deja de filtrul receptorului, apelul direct putea decoda
un fixture `NOKEY` cu M=0,N=1. Acum si decodorul iPERL
aplica filtrul OMS complet, iar `xmq_decode_catalog.h` repeta
admiterea de publicare. Datele deja lizibile ale fixture-ului
sunt testate separat prin `decode_clear_application()`, fara
sa fie prezentate ca un cadru RF necriptat.
Vezi [catalogul de dovezi](experimental/xmq/SECURITY_PROFILE_CATALOG.md).
Nu au fost schimbate decodoarele, cheile ori firmware-ul instalat.

### Audit 7 — RSSI SX1262 (2026-11-08)

**CI source-fidelity fix (2026-11-08):** the Heltec T1 lab YAML
used to clone the old `experimental/semtech-sx126x-sx127x` branch,
which did **not** exercise changes in the active RSSI code branch.
T1, combined T1/C1, and diagnostic lab YAMLs now compile the **local
checkout** `../../components`, identical to the GitHub Actions
commit under test. A fast CI guard
[`test_heltec_lab_source_ref.py`](experimental/semtech-radio/ci/test_heltec_lab_source_ref.py)
rejects future remote-branch regressions. These build-only YAMLs must
be run from inside this repository, not copied without their
`../../components` dependency.

At commit `57232901`, ESPHome compilation remained blocked by
GitHub release assets returning HTTP 504 during ESP-IDF setup, not by
C++ compilation. Tracked in [issue #1](https://github.com/radugeo74/esphome-sx1262/issues/1).
The RSSI change is not considered fully green until this new
exact-source CI build completes. No firmware was flashed.

**CI download hardening (2026-11-08):** pinned ESPHome 2026.10.1
supports `ESPHOME_IDF_FRAMEWORK_MIRRORS`. The build now tries its
original `esphome-libs` ESP-IDF 5.5.6 tar.xz first, then the
**official Espressif-hosted** 5.5.6 release ZIP as a distinct CDN
fallback. `IDF_GITHUB_ASSETS=dl.espressif.com/github_assets` routes
ESP-IDF compiler/ROM tool archives through Espressif's mirror.
The pinned local tool cache `~/.cache/esphome/idf` is restored and
saved by `actions/cache@v4` after a successful build. A host CI
contract test asserts these URLs and both versions to guard future
changes. No framework version or sensor/decoder behavior was changed.
**The fallback requires a new successful full ESPHome build before
this audit point can be called green.**


For **GFSK GetPacketStatus (0x14)** the three bytes are
`RxStatus`, **`RssiSync`** and **`RssiAvg`**. Their scale
is `-raw/2.0 dBm`. Our progressive T1 and combined T1/C1 receive
path collects a full FIFO frame **before** a reliable `RxDone`;
therefore the HA RSSI source is **`RssiSync` (byte [1])**, latched
at synchronization. The packet average (byte [2]) is reliable only
after packet completion and is diagnostic-only here. The normal
upstream FSK `RxDone` path continues using the packet average.

The single source of truth is
[`gfsk_rssi.h`](components/sx126x/gfsk_rssi.h), used **by the actual
radio firmware** and [the host test](experimental/semtech-radio/test_sx126x_gfsk_rssi.cpp).
[The integration guard](experimental/semtech-radio/ci/test_gfsk_rssi_source_contract.py)
ensures that the same sync value is forwarded to the native receiver
and mapped HA sensors. When `t1_raw_diagnostics: true`, the local
log prints bounded `sync_raw`, `avg_raw`, both converted dBm and
the `packet_received` status; no meter IDs or AES keys are logged
by this specific diagnostic. **No artificial RSSI offset, smoothing,
or guessing from the old RTL receiver is introduced.**

**Status:** software / build validation only. The historically
reported approximately -114 dBm may come from packet average or from
the firmware previously installed on the Heltec. A valid sync RSSI
still can differ from the older RTL2832U receiver due to hardware
and RF frontend. We cannot claim the physical discrepancy is resolved
until actual RF on the same device is observed. Never flash
without preserving the existing stable firmware.

## Architecture

- **Radio:** ESPHome's SX126x component, locally patched for T1/C1
  progressive FIFO reception; target board is Heltec WiFi LoRa 32 V3
  (ESP32-S3 / SX1262).
- **wM-Bus:** `components/wmbus_native_sx126x` handles radio-to-DLL
  framing, CRC verification, T1/C1 routing, meter identity, security
  checks, driver selection and mapped sensor publication.
- **Decoding:** native C++ implementations and selected generated XMQ
  rules, checked against public original `wmbusmeters` XMQ fixtures.
  The old `wmbusmeters` radio/decoder stack is **not an active
  dependency** of this receiver.
- **Home Assistant:** only explicitly configured meter IDs and mapped
  `xmq_sensors`, `xmq_text_sensors` or `rssi_sensors` can publish
  values. Unknown meters may be recognized in diagnostics, but do not
  cause entities or readings to be created.

### Audit point 4: verified T1/C1 to mapped sensor delivery (2026-11-08)

Native encrypted publication now uses the shared, host-testable
`components/wmbus_native_sx126x/secure_meter_delivery.h` from the
**actual** `NativeReceiverProbe::publish_secure_batch2_()`. It
performs the same configured sender binding, native AES-driver decode,
result validation, and mapping callback invocation as the firmware;
the receiver connects these callbacks to explicit `xmq_sensors`,
`xmq_text_sensors`, and `rssi_sensors` entities. When a configured
secure sender is recognized but decoding fails, its frame remains
**consumed and rejected**, with **zero** numeric/date/status/RSSI
updates and no fallback to plaintext/AUTO. Unconfigured senders
produce no HA updates. Recognition alone never reaches this
delivery gate.

The new `test_secure_rx_to_mapped_sensors.cpp` feeds packets through
the production `verified::process` path, covering **T1 3-of-6 plus
format-A DLL CRC, C1-A CRC and C1-B CRC**, configured meter binding,
the real mbedTLS native decoder and the shared mapped-output callbacks
with simulated HA sensor objects. Positive cases include an original
public encrypted `apator162` frame (`total_m3=7.512`), an
independently re-encrypted public `topaseskr` application
(`total_m3=481.899`), and an independently encrypted public
`mkradio4a` converter/TPL frame (`target_m3=16.1`,
`target_date=2021-12-31`). Test keys are public/synthetic.
Negative cases include bad DLL CRC, wrong AES key, wrong meter identity,
wrong model/security mode, no configured binding, and upstream
already-decrypted `NOKEY` material presented as on-air ciphertext.
The structural Stage-A regression now checks the shared header is
**actually wired into the receiver**, not just a host-only duplicate.

**Limitations:** this is host integration and ESP-IDF/ESPHome
*build-only* validation, not RF/IRQ/FIFO operation on physical
Heltec hardware and not an actual Home Assistant API assertion.
The existing clear-profile T1/C1 fixture tests remain separate.
No real meter IDs, keys or private RF captures were added.
No change to `meter_drivers`, key selection, original AES algorithms
or installed Heltec firmware. Genuine hardware/HA field acceptance
remains a separate deployment decision.

**CI 3/3 PASS:** [#42459485622](https://github.com/radugeo74/esphome-sx1262/actions/runs/42459485622) — native packet/CRC and mapped output host tests, ESP-IDF S3 link, ESPHome Heltec builds including combined T1/C1.

### OMS transport-security audit fix (2026-11-08)

Read-only audit item 2 identified two independent failures in
`components/wmbus_native_sx126x/tpl_security_probe.h`: it extracted
only the low **four** security-mode bits (instead of OMS **five**,
CF bits 8..12), and it accepted Mode 0 even when CF bits 4..7
declared nonzero encrypted blocks. Per [OMS Vol. 2 §7.2.4.2
and Table 19](https://oms-group.org/wp-content/uploads/2024/10/OMS-Spec_Vol2_Primary_v501_01.pdf),
**clear transport requires M=0 and N=0**. A nonzero N with M=0
is inconsistent and must stay `Unknown`/fail-closed; a fifth
mode bit must never alias reserved Mode 16 to clear Mode 0.

The native probe now reads M with `(CF >> 8) & 0x1F` and N with
`(CF >> 4) & 0x0F`. Known encrypted modes 4 (legacy compatibility),
5, 7, 10 and 13 are flagged as encrypted, while unsupported modes
remain `Unknown`. Existing valid Mode-0 frames with N=0 and
allowed link-control flags still pass. The publication gate continues
to block anything other than `NotEncrypted`.

Positive/negative regressions now cover both `CI=7A` short TPL and
`CI=72` long TPL in `test_tpl_security_probe.cpp` and
`test_native_publication_gate.cpp`.
The Minomess and Hydrus host tests now construct valid synthetic Mode-0
frames by clearing **both** M and N; upstream encrypted XMQ fixtures
remain unchanged. CI #42456075410 passed all three jobs (host tests,
ESP-IDF ESP32-S3 link, and ESPHome Heltec builds). This change does **not**
introduce new AES decoding, bypass existing secure bindings,
alter the manufacturer's clear-specific A0/A2 decoding, or
change any installed Heltec firmware.

### Secure-meter admission audit fix (2026-11-08)

The `secure_meters` YAML schema already accepted all five keyed native
routes, but the old C++ `NativeReceiverProbe::add_secure_meter()` rejected
`mkradio4a` and `topaseskr` at runtime. This made successfully
compiled configurations silently drop those two bindings.

The receiver now delegates registration to the host-tested
`secure_meter_binding.h`, whose explicit allowlist covers
`minomess`, `kamwater`, `apator162`, `mkradio4a` and
`topaseskr`. The DLL/TPL rule remains unchanged: only
`minomess` and `mkradio4a` may bind a distinct `dll_meter_id`.
Registration still rejects invalid decimal IDs, invalid AES hex keys
and duplicate bindings; keys remain private and AUTO/cleartext
fallback remains forbidden for encrypted packets.

`test_secure_meter_binding.cpp` executes the **same** admission
function that the native receiver calls. The native manifest test
also compares the Python YAML and C++ secure-driver allowlists to
prevent this mismatch recurring. The ESPHome CI fixture now includes
synthetic bindings for all five AES routes. This is software-only,
not a claim of successful live RF/HA delivery.

### Audit point 3: AUTO recognition is not AES decryption (2026-11-08)

**Passive AUTO recognition is available for all 16 native driver families**
on independently DLL-CRC-verified T1/C1 packets. The log label is
`NATIVE AUTO` and includes `candidate`, `source=DLL/TPL/ELL`,
`tpl_security`, `tpl_mode`, `driver_class`, and
`decode_status=not_attempted`. The last field means exactly that:
recognition alone did not call a decoder, try an AES key or publish any
Home Assistant values. A verified unknown meter remains diagnostic-only.

**Configured decoding is a separate capability:** 14 drivers remain
eligible for `meter_drivers: driver: auto` (or an explicit driver)
**only for supported clear RF profiles**. The remaining two,
`mkradio4a` and `topaseskr`, remain `SECURE_ONLY`:
encrypted Mode-5 RF requires an explicit `secure_meters` entry, its
individual AES key and a successful application decoder. Neither is
admitted as a plaintext AUTO decoder. A matching MVT is not a key and
the upstream `NOKEY` fixtures do not permit ciphertext fallback.

**Nested MK Radio4a identity:** the original XMQ warm-water profile
contains `TCH/95/37` converter DLL and `HYD/FE/06` meter TPL;
the cold-water profile contains `TCH/95/37` and `WEH/FE/07`.
The latter is validated by the original upstream fixture but missing
from `mkradio4a.xmq`'s `detect.mvt`: the passive recognizer accepts
only that **exact converter + nested TPL pair**. It does **not**
alter the generated global MVT registry, YAML schema, decoder or
security/admission requirements. A TCH converter on its own, or a
conflicting/unknown nested TPL model, is not identified as a confirmed
MK Radio4a meter. `topaseskr` is identified passively for both
documented `AMT/F1/06` and `AMT/F1/07` signatures.

The actual receiver uses the host-tested `passive_auto_identify.h`
helper, covering all 16 families and the converter-only rejection.
Regression tests `test_passive_auto_identification.cpp`,
`test_batch2_auto_identification.cpp`, `test_xmq_auto_registry.cpp`
and `test_batch2_native_catalog_dispatch.cpp` separate
metadata recognition, decoder readiness, ambiguity and plaintext
refusal. No changes were made to the AES implementation, actual meter
keys, active Heltec firmware or configured HA devices.

**CI full green:** [#42457899531](https://github.com/radugeo74/esphome-sx1262/actions/runs/42457899531)
(native packet tests, ESP-IDF ESP32-S3 link, ESPHome Heltec T1/C1
builds). The final passive helper also rejects an isolated HYD MVT
without the required TCH converter and forbids generic DLL fallback
when a different nested TPL address is present.

### Supported driver selection

| Driver | `driver: auto` for a configured meter | Explicit `meter_drivers` |
| --- | --- | --- |
| `fhkvdataiii` (Techem heat) | Yes | Yes |
| `mkradio4` (Techem water) | Yes | Yes |
| `iperl` | Yes (supported clear payloads) | Yes |
| `hydrus` | Yes (supported clear payloads) | Yes |
| `lansenpu` (pulse channels A/B) | Yes | Yes |
| `lansendw` (door/window + pulse events) | Yes | Yes |
| `cma12w` (room temperature, battery) | Yes | Yes |
| `ultraheat` (heat energy, flow, temperature) | Yes, verified clear short TPL | Yes |
| `sontex868` (heat allocation and temperature) | Yes, verified clear short TPL | Yes |
| `minomess` | Yes, clear transport only | Yes |
| `kamwater` | Yes, clear transport only | Yes |
| `supercal` (Sontex Supercal 739) | Yes, clear CI=7A subset | Yes |
| `q400` (Axioma Q400) | Yes, clear CI=7A subset only | Yes |
| `apator162` | Yes, clear transport only | Yes |

The new `ultraheat` and `sontex868` decoders currently cover the
publicly verified **unencrypted short-TPL numeric subset** only.
Ultraheat provides `heat_kwh`, `volume_m3`, `power_kw`,
`flow_m3h`, `flow_c`, `return_c`; Sontex868 provides
`current_consumption_hca`, `consumption_at_set_date_hca`,
`current_temp_c`, `current_room_temp_c`, `max_temp_c`,
`max_temp_previous_period_c`. Do not infer that other upstream
fields, wired M-Bus frames or post-decrypted ELL frames are supported.
Original source fields and strict negative-frame tests passed
[run #42393848471](https://github.com/radugeo74/esphome-sx1262/actions/runs/42393848471).

### New clear-only profiles (2026-11-07; previous code CI green)

`supercal` (SON/1B/04) has one original public clear short-TPL
fixture and publishes `total_kwh`, `total_m3`, `power_kw`,
`volume_flow_m3h`, `flow_temperature_c`, and
`return_temperature_c`. Target/storage-20 dates and values are
not yet included in this native subset.

`q400` (AXI/01/07, AXI/10/07, AXI/20/07) publishes strict
clear short-TPL `total_m3` and optional `volume_flow_m3h`,
`flow_temperature_c`, `battery_pct`. Of five original
public XMQ cases, three have unencrypted TPL and two are
Mode-5 `NOKEY` examples with post-decrypted application records.
The latter are **not** authorized for the no-key decoder.
Other Q400 historical, error, forward/backward fields are outside
this first native subset.

`topaseskr` (Integra AMT/F1/06 or 07) is additionally wired as
**explicit-key `SECURE_ONLY`**. Both public `NOKEY` examples have
already-decrypted Mode-5 applications, not original ciphertext.
The native receiver requires a matched DLL meter ID, AES-128 key,
Mode-5 short TPL with a complete 4-block CBC payload, the decrypted
`2F2F` structural prefix, and independently validated DIF/VIF
records. It publishes a deliberately narrow numeric subset:
`total_m3`, `temperature_c`, `current_flow_m3h`, and optional
`volume_year_period_m3`. Other upstream Topas history/date/battery
fields are not yet published. CBC/`2F2F` does **not** authenticate a
telegram. The source applications are re-encrypted with synthetic
keys for positive/negative CI; real on-air ciphertext and Home
Assistant operation are still unverified.

```yaml
wmbus_native_sx126x:
  # ...sx126x_id and radio options from your complete YAML...
  secure_meters:
    - meter_id: "12345678"  # synthetic configured DLL meter ID
      driver: topaseskr
      key: !secret example_topaseskr_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example Topas total"
        unit_of_measurement: "m³"
```

The `topaseskr` **decoding** route is **never** part of `meter_drivers`,
plaintext decoding or `driver: auto`; metadata-only recognition is separate.

### Topas NOKEY fixtures: keyless *offline* decode

The two original `topaseskr.xmq` examples supplied by upstream are
**already-readable EN 13757-3 DIF/VIF application bytes**. They can be
decoded **without supplying any AES key**, despite their embedded
`7A ... 40 05 2F 2F` short-TPL header (which declares Mode 5,
four AES-CBC blocks). The source's `NOKEY` label does not show
whether their source on-air packets were clear or decrypted before
publication.

A host-only reference adapter
`experimental/semtech-radio/host_only/topaseskr_supplied_clear_fixture.h`
accepts these **explicitly supplied already-clear offline samples**.
`experimental/semtech-radio/test_topaseskr_supplied_clear.cpp`
checks the original numeric outputs and malformed/header/identity
rejections with **no AES key**. It is not imported into the Heltec
receiver or exposed as a Home Assistant publication path.

For **live RF**, the existing `secure_meters` Mode-5 AES implementation
and explicit key remain mandatory when a packet advertises
`0x0540`. A `2F2F` marker alone does not prove decryption or
authorize bypassing the TPL security gate. Keyless live
`topaseskr` AUTO/`meter_drivers` support is **not** claimed without
evidence of a genuine unencrypted radio profile.



These new additions are software-only, not tested with live RF or
Home Assistant. Keep the last *fully green* CI marker above until
the new jobs complete.

### MK Radio4a NOKEY examples also decode without AES (offline)

Just as for `topaseskr`, both original `mkradio4a.xmq`
examples (`FOO` warm water and `BAR` cold water) contain
**already-readable storage-one DIF/VIF values** despite their
Mode-5 `0x0530` three-block CBC TPL and radio-converter
headers. The existing `mkradio4a_plain.h` parser has decoded
them without a key; the new **host-only** supplied-clear adapter
`experimental/semtech-radio/host_only/mkradio4a_supplied_clear_fixture.h`
additionally validates **both** DLL radio-converter and nested
TPL meter IDs, model/version, the `2F2F` fixture marker,
and complete target volume/date fields.

The regression `test_mkradio4a_supplied_clear.cpp` decodes both
original public examples **without a key**, checks their expected
`target_m3` (16.1/75.2) and `target_date` (2021-12-31),
and rejects wrong DLL/TPL IDs, malformed data, and unsupported
envelopes. This adapter is **not included in the live Heltec**
receiver, not a keyless `driver: auto` authorization.

The original examples may have been decrypted before being saved:
`NOKEY` does **not** establish unencrypted radio transmission.
For live `0x0530` Mode-5 RF, the configured `secure_meters`
route still requires the corresponding AES key and both IDs.
No OTA/flash or physical RF/HA test was performed.

### Encrypted-only driver: `mkradio4a`

`mkradio4a` is now additionally software-integrated through
**explicit `secure_meters` only**. The public original XMQ
examples use Techem `TCH/95/37` radio converters and nested
`HYD/FE/06` (warm) or `WEH/FE/07` (cold) meter headers. The
receiver must match **both configured IDs**, validate DLL CRC,
decrypt exactly three AES-CBC Mode-5 blocks with the explicit AES-128
key, then validate the original application before publishing
`target_m3` and `target_date` (ISO `YYYY-MM-DD`).

```yaml
wmbus_native_sx126x:
  # ...sx126x_id and radio options from your complete YAML...
  secure_meters:
    - meter_id: "12345678"      # synthetic nested TPL ID
      dll_meter_id: "87654321"  # synthetic radio converter DLL ID
      driver: mkradio4a
      key: !secret example_mkradio4a_aes_key
  xmq_sensors:
    - meter_id: "12345678"
      field: target_m3
      sensor:
        name: "Example MK Radio 4a target"
        unit_of_measurement: "m³"
  xmq_text_sensors:
    - meter_id: "12345678"
      field: target_date
      sensor:
        name: "Example MK Radio 4a target date"
```

Use **actual corresponding** meter/convertor IDs and the correct
private AES key in your own YAML; these values are illustrative.
The upstream `NOKEY` samples contain post-decrypted application
records, not verified on-air ciphertext. Regression tests use
synthetic AES encryption of both original sample applications; there
is **no real MK Radio 4a RF/HA validation**. All three new-driver CI jobs passed [run #42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133). AES-CBC and the
`2F 2F` marker are **not cryptographic authentication**.
`mkradio4a` is deliberately **not allowed in plaintext
`meter_drivers` or `driver: auto`**.

### Software support and acceptance status

**The table lists currently configured software driver candidates; the newly added clear-only Q400/Supercal jobs must pass CI before being counted as fully verified**, but only
the Techem baseline has been observed with real nearby meters.
The active driver manifest defines:

- `SUPPORTED`: generic native compiled/DLL AUTO drivers.
- `CONFIGURED_NATIVE = ("minomess", "kamwater", "apator162")`:
  software-tested native drivers with strict **configured-only** AUTO.
- `CONFIGURABLE = SUPPORTED + CONFIGURED_NATIVE`:
  the 14 names allowed in YAML `meter_drivers`.
- `SECURE_ONLY = ("mkradio4a", "topaseskr")`: 2 additional
  explicit-key drivers (16 software-integrated drivers in total). MK Radio4a
  requires separate DLL/TPL identities; Topas binds its DLL ID directly.

The old `BATCH2_PENDING` category was retired after software
integration. The three models are **not pending software implementation**;
they are omitted from generic `SUPPORTED` only because their
DLL/TPL/ELL identity/security admission requires a separate native
dispatcher. This restriction does not block `driver: auto` or an
explicit driver on a configured meter.

A matched manufacturer/model never proves the payload is clear,
valid or authenticated. **Physical RF-to-Home Assistant validation
is still open** for the new driver families.

## YAML: automatic or explicit driver selection

For a **configured meter**, both forms are supported. Use an
eight-digit, decimal-string meter ID (including leading zeroes). The
examples use **synthetic IDs only**.

**Option A — automatically identify the driver:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: auto
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example meter total"
        unit_of_measurement: "m³"
```

The firmware selects a driver from the verified telegram metadata,
then the chosen decoder independently checks transport, meter identity,
security and field validity. No matching/valid frame means **no
sensor update**.

**Option B — specify the driver yourself:**

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"
      driver: kamwater
  xmq_sensors:
    - meter_id: "12345678"
      field: total_m3
      sensor:
        name: "Example Kamwater total"
        unit_of_measurement: "m³"
```

In both examples, `sx126x_id` must reference a correctly configured
`sx126x:` radio in the **same full YAML**. These snippets demonstrate
driver selection; they are not standalone firmware configurations.
Map only fields supported by the selected meter and driver.

**Minomess behind a radio converter:** when the radio's DLL sender ID
differs from the actual meter ID in the long TPL address, configure
both explicitly. This works with `driver: auto` or `driver: minomess`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  meter_drivers:
    - meter_id: "12345678"      # meter identity in TPL
      dll_meter_id: "87654321"  # sender identity in DLL
      driver: auto              # or: minomess
```

The two identities must match their respective headers; a wrong or
ambiguous match blocks decoding. A distinct `dll_meter_id` is not a
general gateway auto-discovery feature.

### Encryption and discovery boundaries

**`driver: auto` does not guess AES keys or decrypt encrypted
telegrams.** For encrypted meters, use an explicit driver and the
appropriate AES-128 key in `secure_meters`, usually via `!secret`:

```yaml
wmbus_native_sx126x:
  # ...same sx126x_id and receiver options...
  secure_meters:
    - meter_id: "12345678"
      driver: minomess
      key: !secret example_minomess_aes_key
```

`secure_meters` supports the separately reviewed keyed paths for
`minomess`, `kamwater`, `apator162` and `mkradio4a`; it **requires an explicit
driver**, not `auto`. A telegram that claims encryption but lacks a
configured valid key must not be published as plaintext. Failed
integrity, identity, payload or cryptographic checks do not publish
partial, stale or synthetic values. C1 plaintext sensor publication
additionally requires the opt-in `c1_plaintext_publish: true`.

CRC-verified T1/C1 frames from **unconfigured** meters can be
recognized passively, including Minomess, Kamwater, Apator162 and
Lansen, using original XMQ manufacturer/version/type signatures.
Diagnostic `BATCH2 AUTO candidate=...` messages do **not** decode,
create entities, or publish meter values.

### New cleartext drivers: Lansen DW and Elvaco CMA12w

The native pipeline also supports `lansendw` (Lansen
door/window, `a_counter` / `b_counter`, text `status` OPEN/CLOSED,
`error_flags`) and `cma12w` (Elvaco temperature,
`current_temperature_c`, `average_temperature_1h_c`,
text `battery`, `software_version`, `status`).
Both accept `driver: auto` or the explicit driver name on
a *configured* eight-digit meter ID. They use strict mode-zero
TPL frames and four original upstream `NOKEY` examples. The
new decoders reject wrong meter/model, encrypted modes and invalid
records rather than publishing partial sensor updates.
`lansenth` is **not** enabled: its upstream NOKEY examples
carry encrypted TPL Mode 5 and must not be misclassified as clear RF.

## Building and testing

The branch includes a **build-only** Heltec T1/C1 laboratory YAML at
[experimental/semtech-radio/heltec-sx126x-combined-lab.yaml](experimental/semtech-radio/heltec-sx126x-combined-lab.yaml).
It uses synthetic or public upstream example IDs and exercises
`driver: auto` for the eight newer drivers (including Ultraheat and Sontex868). Check the board revision and real radio pin/TCXO mapping
before adapting it to your own hardware.

GitHub Actions run [#42401195133](https://github.com/radugeo74/esphome-sx1262/actions/runs/42401195133)
passed `native-packet-input`,
`stage-a-espidf-s3-portable-link` and `native-esphome-build`.
This establishes host regression coverage and ESP32-S3/Heltec
**compilation**, not reception or correct HA publication from real
Minomess/Kamwater/Apator162/Lansen/Elvaco/Ultraheat/Sontex devices. The known-working Techem
behavior has been observed separately on a Heltec, but does not
validate every newly added driver.

**Do not flash the existing production Heltec solely on the strength
of CI.** Preserve a working firmware backup and test any new build
with real reception before declaring field readiness. Do not commit
private meter IDs, encryption keys, Wi-Fi secrets or production
telegrams.

For implementation notes, start with
[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md),
[experimental/xmq/README.md](experimental/xmq/README.md),
[experimental/xmq/STAGE_B_GATE_STATUS.md](experimental/xmq/STAGE_B_GATE_STATUS.md),
[experimental/xmq/BATCH2_STATUS.md](experimental/xmq/BATCH2_STATUS.md)
and the [CI workflow](.github/workflows/native-sx126x-adapter.yml).

## Project origin, credits and licensing

This **current native SX126x + XMQ architecture is not a continuation
of the old `SzczepanLeon/esphome-components` runtime**. That project
is part of the **historical development lineage** of earlier wM-Bus
experiments in this repository; it is not the active radio/decoding
component described above.

The active project uses the ESPHome SX126x component with local
modifications, plus native implementation work in this repository.
Original XMQ driver specifications and public fixture expectations
come from [wmbusmeters](https://github.com/wmbusmeters/wmbusmeters)
and retain their upstream copyright/licence notices. Historical
SzczepanLeon contributions and any other imported source still
retain the attribution and licensing applicable to those files:
architecture changes do **not** erase the origin of reused code.
Consult the individual file headers before reuse or redistribution.

For earlier development and the former relationship with
[SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components),
see the [archived historical README](docs/history/xmq-independent-pipeline-README.md).
