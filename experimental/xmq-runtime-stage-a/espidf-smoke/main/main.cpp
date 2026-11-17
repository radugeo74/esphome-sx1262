// Build-only ESP32-S3 compilation check for the exact Stage A headers.
// No radio components, no filesystem mount, no sensors, no flash or RF tests.
#include "xmq_mounted_vfs.h"
#include "xmq_calendar_runtime.h"
#include "xmq_driver_loader.h"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

extern "C" void app_main(void) {
  // Exercise symbol/type/link compilation in an actual ESP-IDF target.
  // A real filesystem mount and driver image are not present in this lab.
  try {
    xmq_runtime::MountedXmqFiles source("/xmq",4096);
    auto file = source.read("not-mounted");
    std::printf("Unexpected mounted source length=%u\n",
                static_cast<unsigned>(file.size()));
  } catch (const std::exception &error) {
    std::printf("XMQ ESP-IDF build smoke: unmounted source correctly rejected: %s\n",
                error.what());
  }
}
