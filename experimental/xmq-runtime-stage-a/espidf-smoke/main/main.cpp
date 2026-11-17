// Build-only ESP32-S3 compilation check for the exact Stage A headers.
// No radio components, no filesystem mount, no sensors, no flash or RF tests.
#include "xmq_mounted_vfs.h"
#include "xmq_calendar_runtime.h"
#include "xmq_frame_bridge.h"
#include "xmq_shadow_diagnostic.h"
#include "xmq_telegram_capture.h"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

extern "C" void app_main(void) {
  // Exercise symbol/type/link compilation in an actual ESP-IDF target.
  // A real filesystem mount and driver image are not present in this lab.
  try {
    // Force the cross-compiler to instantiate and link both the generic
    // registry and application bridge. No live radio input is used.
    xmq_runtime::Driver d;
    d.name="xmq_smoke";
    d.detect.push_back({"TCH",0x95,0x37});
    xmq_runtime::DriverRegistry registry({d});
    const auto result=xmq_runtime::project_verified_application(
        registry,{"TCH",0x95,0x37},
        xmq_runtime::ApplicationEncoding::StandardDv,
        {0x43,0x15,0xA1,0x00,0x00});
    std::printf("XMQ bridge smoke records=%u\\n",
                static_cast<unsigned>(result.numeric.size()));
    // Opt-in isolated shadow diagnostics are compiled and linked for the
    // Xtensa target; they do not intercept live WMBus or publish sensors.
    xmq_runtime::ShadowDiagnostic shadow(1);
    shadow.set_enabled(true);
    xmq_runtime::ShadowReference legacy;
    const auto observation=shadow.inspect(
        registry,{"TCH",0x95,0x37},
        xmq_runtime::ApplicationEncoding::StandardDv,
        {0x43,0x15,0xA1,0x00,0x00},legacy);
    std::printf("XMQ shadow result=%u observations=%u\n",
                static_cast<unsigned>(observation),
                static_cast<unsigned>(shadow.counters().observations));
    // Compile and link the post-decryption snapshot guard with a shape
    // matching wmbus_common::Telegram; no live radio or AES keys in smoke.
    struct AddressShape {
      unsigned mfct;
      uint8_t version;
      uint8_t type;
    };
    struct TelegramShape {
      std::vector<uint8_t> frame;
      int header_size;
      int suffix_size;
      int tpl_ci;
      bool decryption_failed;
      bool discard;
      std::vector<AddressShape> addresses;
    };
    const TelegramShape existing{{0x7A,0x43,0x15,0xA1,0,0},
                                 1,0,0x7A,false,false,{{0x5068,0x95,0x37}}};
    xmq_runtime::VerifiedApplication snapshot;
    const bool captured=xmq_runtime::capture_verified_payload(
        existing,true,xmq_runtime::ApplicationEncoding::StandardDv,snapshot);
    std::printf("XMQ verified capture=%u length=%u\n",
                static_cast<unsigned>(captured),
                static_cast<unsigned>(snapshot.payload.size()));
    xmq_runtime::MountedXmqFiles source("/xmq",4096);
    auto file = source.read("not-mounted");
    std::printf("Unexpected mounted source length=%u\n",
                static_cast<unsigned>(file.size()));
  } catch (const std::exception &error) {
    std::printf("XMQ ESP-IDF build smoke: unmounted source correctly rejected: %s\n",
                error.what());
  }
}
