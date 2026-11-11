# Referință și rollback — native-espidf

- **Bază Git:** [`native-espidf`](https://github.com/radugeo74/esphome-sx1262/tree/native-espidf), commit `c4284739be7f9a5ee251de268ae3904068906ca4`.
- **Dovada istorică a buildului:** [`NATIVE_ESPIDF_REPORT.md`](../NATIVE_ESPIDF_REPORT.md) (ESPHome 2026.10.0, ESP-IDF 5.5.6, Windows, compilare CMake/Ninja, nu validare RF).
- **Sursa componentei radio patch-uite:** [`components/sx126x` la `bd3035ed`](https://github.com/radugeo74/esphome-sx1262/tree/bd3035edf2a0ee1f87ad827c344b47975061923a/components/sx126x).
- **Ramura experimentală XMQ-only:** `experimental/xmq-multidriver-auto`, păstrată ca referință; seria de migrări C++ per-driver NU reprezintă arhitectura nouă.

Nu se șterg `wmbus_common`, `wmbus_meter`, `wmbus_radio` sau `socket_transmitter` înainte ca înlocuitorul nativ/interpretul XMQ să treacă teste de paritate și hardware. Nu se atinge `native-espidf` și nu se face reset/merge în ramurile de producție.

**Criteriu de bază:** toate fișierele de cod din `native-espidf` rămân identice; adăugăm o componentă radio completă, documentație și fixture separat. Pentru rollback se poate reveni la configurația/source `native-espidf`.
