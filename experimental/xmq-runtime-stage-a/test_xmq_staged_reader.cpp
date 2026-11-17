// ESPHome auto-includes every component .h through generated esphome.h.
// Simulate direct inclusion AFTER legacy util.h defined error(...).
#define error(...) ((void)0)
#include "components/xmq_shadow_probe/xmq_reader.h"
#ifndef error
#error "Staged XMQ reader must restore legacy error macro"
#endif
#include <iostream>
int main() {
  error("legacy logging macro restored");
  std::cout<<"STAGED_READER_OK direct auto-include is safe under legacy error macro\n";
  return 0;
}
