#pragma once
// Experimental shadow diagnostics: no radio dispatch, decryption, HA publish,
// raw telegram logging, meter mutations or source-file writes.
// The caller MUST pass a previously CRC-verified, decrypted application,
// its correct INNER meter MVT/format, and values already published by the
// existing decoder. Failures do not propagate to that existing decoder.
#include "xmq_shadow_compare.h"
#include "xmq_primary_readiness.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace xmq_runtime {

enum class ShadowDisposition {
  Disabled,
  Match,
  Difference,
  UnsupportedOrError,
  BudgetExhausted,
  IncompleteReference
};

struct ShadowCounters {
  uint32_t matched=0;
  uint32_t different=0;
  uint32_t errors=0;
  uint32_t skipped=0;
  uint32_t observations=0;
  size_t last_compared=0;
  size_t last_mismatches=0;
  size_t min_free_heap=0;
  bool has_heap_sample=false;
};

// Capped, isolated, non-throwing per-session diagnostic; no identifiers or
// plaintext are retained between observations. It does not allocate logs.
class ShadowDiagnostic {
 public:
  explicit ShadowDiagnostic(uint32_t max_observations=100,
                            double tolerance=1e-6)
      : max_observations_(max_observations), tolerance_(tolerance) {}

  void set_enabled(bool enabled) noexcept { enabled_=enabled; }
  bool enabled() const noexcept { return enabled_; }
  const ShadowCounters &counters() const noexcept { return counters_; }

  ShadowDisposition inspect(const DriverRegistry &registry,
                            const Mvt &inner_mvt,
                            ApplicationEncoding encoding,
                            const std::vector<uint8_t> &clear_application,
                            const ShadowReference &old_outputs,
                            size_t free_heap=0,
                            bool valid_heap_sample=false,
                            const std::string &explicit_driver="",
                            const std::vector<std::string> &expected_numeric={},
                            PrimaryReadiness *primary=nullptr,
                            const std::vector<std::string> &primary_dates={},
                            XmqFrameResult *primary_output=nullptr,
                            const XmqFrameResult *verified_preprojection=nullptr) noexcept {
    if (primary != nullptr) *primary = PrimaryReadiness{};
    if (primary_output != nullptr) *primary_output = XmqFrameResult{};
    if (!enabled_) {
      ++counters_.skipped;
      return ShadowDisposition::Disabled;
    }
    if (max_observations_==0 || counters_.observations>=max_observations_) {
      ++counters_.skipped;
      return ShadowDisposition::BudgetExhausted;
    }
    ++counters_.observations;
    counters_.last_compared=0;
    counters_.last_mismatches=0;
    if (valid_heap_sample) {
      if (!counters_.has_heap_sample || free_heap<counters_.min_free_heap)
        counters_.min_free_heap=free_heap;
      counters_.has_heap_sample=true;
    }
    try {
      if (!(tolerance_>=0.0) || !std::isfinite(tolerance_))
        throw std::invalid_argument("invalid shadow tolerance");
      // Caller must prove byte-identical clear application, inner MVT and
      // encoding before supplying a Stage 4 direct-T1 projection. Otherwise
      // the default path decodes independently as before. Never reuse a
      // legacy snapshot as an XMQ projection.
      XmqFrameResult decoded_outputs;
      if (verified_preprojection == nullptr)
        decoded_outputs = project_verified_application(
            registry,inner_mvt,encoding,clear_application,explicit_driver);
      const XmqFrameResult &new_outputs =
          verified_preprojection != nullptr ? *verified_preprojection : decoded_outputs;
      // Primary candidate is derived from the existing XMQ projection,
      // never from legacy snapshots. No second parser execution.
      if (primary != nullptr) {
        *primary = inspect_primary_readiness(
            new_outputs, explicit_driver, expected_numeric, primary_dates);
        // Copy only when explicitly requested and the independent XMQ
        // fields satisfy the complete contract. No second XMQ parse.
        if (primary_output != nullptr && primary->ready)
          *primary_output = new_outputs;
      }
      // A zero-field comparison cannot establish parity. Fail closed even
      // when the caller forgets to supply an expected-field contract.
      if (old_outputs.numeric.empty() && old_outputs.dates.empty() &&
          old_outputs.absent_numeric.empty() && old_outputs.absent_dates.empty()) {
        ++counters_.skipped;
        return ShadowDisposition::IncompleteReference;
      }
      // In strict mode, an incomplete legacy snapshot must not pass as MATCH.
    // The complete expected field list is a caller-supplied contract for
    // each telegram variant; never infer "parity" from only one known value.
    if (!expected_numeric.empty()) {
      for (const auto &field : expected_numeric) {
        if (old_outputs.numeric.count(field)==0) {
          ++counters_.skipped;
          return ShadowDisposition::IncompleteReference;
        }
      }
    }
    const auto comparison=compare_shadow_reference(
          new_outputs,old_outputs,tolerance_);
      counters_.last_compared=comparison.compared;
      counters_.last_mismatches=comparison.mismatches.size();
      if (comparison.equal()) {
        ++counters_.matched;
        return ShadowDisposition::Match;
      }
      ++counters_.different;
      return ShadowDisposition::Difference;
    } catch (...) {
      // Bad inputs, unsupported versions and OOM/exception never break the
      // existing meter decoder or cause a frame to be marked handled.
      ++counters_.errors;
      return ShadowDisposition::UnsupportedOrError;
    }
  }

 private:
  bool enabled_=false;
  uint32_t max_observations_;
  double tolerance_;
  ShadowCounters counters_;
};

} // namespace xmq_runtime
