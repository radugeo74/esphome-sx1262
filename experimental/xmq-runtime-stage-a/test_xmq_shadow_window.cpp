#include "components/xmq_shadow_probe/xmq_shadow_budget.h"
#include <cassert>
#include <cstdint>
#include <iostream>

using esphome::xmq_shadow_probe::shadow_window_elapsed;
using esphome::xmq_shadow_probe::ShadowObservationBudget;
using esphome::xmq_shadow_probe::kShadowWindowMillis;
using esphome::xmq_shadow_probe::kShadowWindowMaxObservations;

int main() {
  static_assert(kShadowWindowMillis == 3600000UL, "exactly one hour");
  static_assert(kShadowWindowMaxObservations == 128, "bounded observations");
  assert(!shadow_window_elapsed(0U, 0U));
  assert(!shadow_window_elapsed(3599999U, 0U));
  assert(shadow_window_elapsed(3600000U, 0U));
  assert(shadow_window_elapsed(3600001U, 0U));
  assert(shadow_window_elapsed(7200000U, 3600000U));

  // A real millis() overflow: start only 16ms before UINT32_MAX,
  // then cross zero. Do not falsely renew at wrap, but do renew at 1h.
  const uint32_t start = UINT32_MAX - 15U;
  assert(!shadow_window_elapsed(3U, start)); // 20ms elapsed
  const uint32_t just_before = static_cast<uint32_t>(start + 3599999U);
  const uint32_t at_boundary = static_cast<uint32_t>(start + 3600000U);
  assert(!shadow_window_elapsed(just_before, start));
  assert(shadow_window_elapsed(at_boundary, start));
  const uint32_t next_start = at_boundary;
  assert(!shadow_window_elapsed(next_start, next_start));
  assert(!shadow_window_elapsed(static_cast<uint32_t>(next_start + 1U), next_start));
  assert(shadow_window_elapsed(static_cast<uint32_t>(next_start + 3600000U), next_start));
  ShadowObservationBudget budget;
  assert(budget.may_observe());
  assert(budget.used() == 0U);
  // Two pre/post-decode queries must not themselves consume a slot.
  for (uint32_t i = 0; i < kShadowWindowMaxObservations; ++i) {
    assert(budget.may_observe());
    assert(budget.may_observe());
    assert(budget.consume());
    assert(budget.used() == i + 1U);
  }
  assert(budget.exhausted());
  assert(!budget.may_observe());
  assert(!budget.consume()); // observation 129 rejected
  assert(budget.used() == 128U);
  assert(!budget.renew_if_elapsed(3599999U));
  assert(budget.exhausted());
  assert(budget.renew_if_elapsed(3600000U));
  assert(budget.used() == 0U);
  assert(budget.may_observe());
  assert(budget.consume()); // resumed observation 1
  assert(budget.used() == 1U);
  assert(!budget.renew_if_elapsed(3600001U));
  assert(budget.used() == 1U);
  // Cross unsigned millis() rollover on an independently renewed session.
  ShadowObservationBudget rollover;
  assert(rollover.renew_if_elapsed(UINT32_MAX - 15U));
  assert(rollover.consume());
  assert(!rollover.renew_if_elapsed(3U));
  assert(rollover.used() == 1U);
  assert(rollover.renew_if_elapsed(static_cast<uint32_t>(UINT32_MAX - 15U + 3600000U)));
  assert(rollover.used() == 0U && rollover.may_observe());
  std::cout << "XMQ_SHADOW_BUDGET_OK: 128 accepted, 129 rejected, 1h renewal, double-query safe, uint32 rollover\\n";
  std::cout << "XMQ_SHADOW_WINDOW_OK: 1h boundaries, millis overflow and post-renewal reset tested\n";
}
