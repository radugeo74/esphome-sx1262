# XMQ runtime — zona viitorului motor

Bază corectă: `native-espidf`, commit `c4284739`, plus `components/sx126x` patch-uit din `bd3035ed`.

**Stare: structură de lucru, fără motor XMQ sau hot reload implementat.** Aici va locui compilerul host XMQ → descriptor binar/date, iar interpretul ESPHome va fi comun pentru toate familiile, nu câte un driver C++ generat.

Priorități: adaptorul radio nativ cu un singur proprietar SPI, paritate cu motorul C++ moștenit, demonstrație `iperl.xmq` modificat și preluat de **același firmware**, apoi IXML (`mkradio4` și `fhkvdataiii`). Nu se stochează chei sau date private.
