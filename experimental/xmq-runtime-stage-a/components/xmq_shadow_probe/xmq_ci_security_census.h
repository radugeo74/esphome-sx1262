#pragma once
// HOST-ONLY privacy-preserving CI/TPL security census.
// Accepts metadata ONLY from a successfully parsed legacy Telegram.
// Never stores meter IDs, manufacturer, payload, access numbers or keys.
// No radio hook or Home Assistant publishing.
#include <array>
#include <cstddef>
#include <cstdint>
namespace xmq_runtime {
struct CiSecurityCensus {
  // 0=A0,1=A2,2=72,3=7A,4=other; security 0=none,1=mode5,
  // 2=mode7,3=other. Counts intentionally cannot be broken down by meter.
  std::array<std::array<std::uint32_t,4>,5> accepted{};
  std::uint32_t rejected{0};
  bool observe(int ci, int tpl_mode, bool successful_legacy_parse,
               bool decryption_failed, bool discarded) noexcept {
    if (!successful_legacy_parse || decryption_failed || discarded ||
        ci < 0 || ci > 255 || tpl_mode < 0 || tpl_mode > 31) {
      ++rejected;
      return false;
    }
    const std::size_t ci_bucket =
        ci == 0xA0 ? 0 : ci == 0xA2 ? 1 :
        ci == 0x72 ? 2 : ci == 0x7A ? 3 : 4;
    const std::size_t sec_bucket =
        tpl_mode == 0 ? 0 : tpl_mode == 5 ? 1 :
        tpl_mode == 7 ? 2 : 3;
    ++accepted[ci_bucket][sec_bucket];
    return true;
  }
  template<typename TelegramT>
  bool observe_telegram(const TelegramT &telegram,
                        bool handle_telegram_success) noexcept {
    return observe(telegram.tpl_ci, static_cast<int>(telegram.tpl_sec_mode),
                   handle_telegram_success, telegram.decryption_failed,
                   telegram.discard);
  }
};
} // namespace xmq_runtime
