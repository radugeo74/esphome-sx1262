// Reproduce the actual preprocessor collision from wmbus_common/util.h.
// The XMQ method Parser::error must compile, while the legacy logging macro
// must still be defined after the protected inclusion ends.
#define error(...) ((void)0)
#include "components/xmq_shadow_probe/xmq_compat_include.h"
#ifndef error
#error "Legacy error macro was not restored by XMQ wrapper"
#endif
#include <iostream>

int main() {
  xmq_runtime::ShadowDiagnostic probe(1);
  if (probe.enabled()) return 1;
  // Verify the legacy macro still works for the next wmbus_common include.
  error("expected macro restored");
  std::cout<<"COMPAT_MACROS_OK parser method unaffected; logging macro restored\n";
  return 0;
}
