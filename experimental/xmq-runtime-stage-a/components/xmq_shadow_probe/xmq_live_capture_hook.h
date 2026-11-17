#pragma once
// Lab-only post-decode hook. No XMQ parser headers here: this file is also
// included by the cloned wmbus_meter and by generated esphome.h.
// The original receiver and meter code under /components are unchanged.
struct Telegram;
namespace esphome {
namespace xmq_shadow_probe {
// Returns without allocating when the optional probe is disabled.
// Never modifies the legacy Telegram, its handled status, or sensors.
void observe_legacy_telegram(const ::Telegram &telegram, bool decoded) noexcept;
}  // namespace xmq_shadow_probe
}  // namespace esphome
