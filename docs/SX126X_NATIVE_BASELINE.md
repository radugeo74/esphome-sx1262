# SX126x nativ ESP-IDF — origine, patch și integrare

## Ce a cerut utilizatorul

Baza de lucru pentru noul motor XMQ este **[`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf)**, commit `c4284739be7f9a5ee251de268ae3904068906ca4`, peste care păstrăm modificările din driverul SX126x nativ ESP-IDF.

## Ce am adăugat

Am importat **identic** subarborele [`components/sx126x`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-multidriver-auto/components/sx126x) din commitul `bd3035edf2a0ee1f87ad827c344b47975061923a` (ultima versiune verificată a ramurii experimentale, workflow [#881](https://github.com/radugeo74/esphome-sx1262/actions/runs/43063443189) SUCCESS acolo).

Acesta conține `sx126x.cpp`, `sx126x.h`, `sx126x_reg.h`, `gfsk_rssi.h`, `automation.h`, `packet_transport` și schema Python ESPHome.

Patch-uri importante, direct în `components/sx126x/sx126x.cpp` / `.h`:

- `wait_busy_()` returnează `bool` și oprește tranzacția la timeout ori dacă componenta a eșuat.
- `read_fifo_`, `write_fifo_`, `read_opcode_`, `write_opcode_`, `read_register_` și `write_register_` verifică BUSY **înainte de selectarea SPI**.
- `configure()` verifică `is_failed()` după etapele critice.
- `version_[16]{}` este inițializat defensiv.
- Păstrăm patch-urile de recepție GFSK/T1, fluxul FIFO și helpers din același snapshot.

## O diferență arhitecturală importantă

În `native-espidf`, radio existent este `components/wmbus_radio/transceiver_sx1262.cpp`, care folosește o interfață diferită de componenta modernă `components/sx126x`. **Simpla importare nu face ca `wmbus_meter` să primească date de la noua componentă.**

Până când construim adaptorul:

- Configurația `wmbus_radio` legacy trebuie să rămână neschimbată.
- Noua componentă poate fi compilată **izolat** din `tests/sx126x-native-espidf.yaml`, fără `wmbus_radio`.
- Nu configura ambele drivere SPI pe aceleași GPIO-uri/radio în același firmware.
- Nu interpreta rezultatul CI #881 de pe ramura veche ca CI al acestei combinații.
- Nu instala firmware pe Heltec până la validarea pinilor, RF T1/C1, CRC, RSSI și comportamentului BUSY.

## Planul de integrare

1. Compilare YAML izolată, apoi repetare cu adaptorul dedicat.
2. Legătură `sx126x` → verificări fizice T1/C1 și CRC → evenimente wM-Bus existente, fără utilizarea transceiverului vechi.
3. Controlul unui singur radio/SPI owner; watchdog/recovery și RSSI verificat.
4. Înlocuire treptată a decoderelor C++ vechi cu interpretul comun de date XMQ, fără schimbarea senzorilor HA.
5. Test de actualizare XMQ fără OTA și fără ocolirea criptării.

**Stadiu:** codul componentului importat; adaptorul și motorul dinamic sunt viitoare.
