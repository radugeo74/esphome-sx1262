// Stage-0 host smoke test: verifies C++ source embedding only.
// Build: c++ -std=c++17 -Wall -Wextra -pedantic experimental/xmq/tests/embedding_smoke.cpp -o /tmp/xmq_embed_smoke
#include "../generated/embedded_xmq_drivers.h"
#include <cstring>
#include <cstdio>

using namespace esphome::wmbus_xmq_experimental;
static_assert(driver_count == 2, "Expected exactly two experimental drivers");

int main() {
  for (std::size_t i = 0; i < driver_count; ++i) {
    const auto &d = drivers[i];
    if (!d.name || !d.source || !d.size || std::strlen(d.source) != d.size) return 1;
    if (!std::strstr(d.source, "driver {") || !std::strstr(d.source, "tests {")) return 2;
    std::printf("%s: %zu XMQ bytes embedded\n", d.name, d.size);
  }
  return 0;
}
