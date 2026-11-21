#include "xmq_shadow_diagnostic.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string upstream(const char *name) {
  std::ifstream f(std::string("experimental/xmq-runtime-stage-a/upstream/src/")+name+".xmq");
  if (!f) throw std::runtime_error("upstream missing");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static void require(bool value,const char *reason) {
  if (!value) throw std::runtime_error(reason);
}
int main() {
  using namespace xmq_runtime;
  try {
    DriverRegistry registry({load_driver(upstream("mkradio4a"))});
    const auto valid=parse_hex("4315A10000426CBF2C");
    ShadowReference old;
    old.numeric={{"target_m3",16.1}};
    old.dates={{"target_date","2021-12-31"}};
    ShadowDiagnostic disabled(3);
    require(disabled.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,valid,old)==
            ShadowDisposition::Disabled,"default must be disabled");
    require(disabled.counters().observations==0,"disabled performed a decode");
    disabled.set_enabled(true);
    require(disabled.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,valid,old,
                            120000,true)==ShadowDisposition::Match,
            "matching old decoder output not detected");
    require(disabled.counters().matched==1 &&
            disabled.counters().last_compared==2 &&
            disabled.counters().min_free_heap==120000,
            "matched counters / heap sample incorrect");

    // The same XMQ parse reports field readiness without reading legacy
    // values or making a second projection. A bad contract fails closed.
    PrimaryReadiness independent;
    ShadowDiagnostic single_pass(2);
    single_pass.set_enabled(true);
    require(single_pass.inspect(registry, {"HYD",0xFE,0x06},
                ApplicationEncoding::StandardDv, valid, old,
                0, false, "mkradio4a", {"target_m3"}, &independent,
                {"target_date"}) == ShadowDisposition::Match,
            "single-pass readiness changed legacy shadow outcome");
    require(independent.ready && independent.required == 2 &&
            independent.available == 2,
            "single-pass independent readiness omitted XMQ date/numeric");
    independent.ready = true;
    require(single_pass.inspect(registry, {"HYD",0xFE,0x06},
                ApplicationEncoding::StandardDv, {}, old,
                0, false, "mkradio4a", {"target_m3"}, &independent,
                {"target_date"}) == ShadowDisposition::UnsupportedOrError &&
            !independent.ready && independent.available == 0,
            "failed parse retained stale readiness from prior telegram");

    // A caller that provides no reference fields must never report MATCH.
    // Zero comparisons are not independent evidence of parser parity.
    ShadowReference empty_reference;
    ShadowDiagnostic empty_probe(1);
    empty_probe.set_enabled(true);
    require(empty_probe.inspect(registry,{"HYD",0xFE,0x06},
                                ApplicationEncoding::StandardDv,valid,
                                empty_reference)==ShadowDisposition::IncompleteReference,
            "empty shadow reference falsely reported parity");
    require(empty_probe.counters().matched==0 &&
            empty_probe.counters().last_compared==0 &&
            empty_probe.counters().skipped==1,
            "empty shadow reference counters incorrect");

    // A snapshot containing only one of two required readings must never
    // be counted as a successful live parity comparison.
    ShadowReference incomplete;
    incomplete.numeric={{"target_m3",16.1}};
    ShadowDiagnostic strict(2);
    strict.set_enabled(true);
    require(strict.inspect(registry,{"HYD",0xFE,0x06},
                           ApplicationEncoding::StandardDv,valid,incomplete,
                           0,false,"",{"target_m3","total_m3"})==
            ShadowDisposition::IncompleteReference,
            "partial legacy field reference misreported as MATCH");
    require(strict.counters().matched==0 && strict.counters().skipped==1,
            "incomplete parity counters incorrect");

    // Calendar parity is checked by the same comparator, not silently
    // reduced to numeric equality.
    auto bad_date=old;
    bad_date.dates["target_date"]="2022-01-01";
    ShadowDiagnostic strict_date(1);
    strict_date.set_enabled(true);
    require(strict_date.inspect(registry,{"HYD",0xFE,0x06},
                                ApplicationEncoding::StandardDv,valid,bad_date)==
            ShadowDisposition::Difference,
            "incorrect legacy calendar value was not detected");

    auto wrong=old;
    wrong.numeric["target_m3"]=25.0;
    require(disabled.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,valid,wrong,
                            110000,true)==ShadowDisposition::Difference,
            "numeric divergence not flagged");
    require(disabled.counters().different==1 &&
            disabled.counters().last_mismatches==1 &&
            disabled.counters().min_free_heap==110000,
            "divergence counters incorrect");
    require(disabled.inspect(registry,{"ABC",0,0},
                            ApplicationEncoding::StandardDv,valid,old,
                            100000,true)==ShadowDisposition::UnsupportedOrError,
            "unsupported MVT must not propagate");
    require(disabled.counters().errors==1 &&
            disabled.counters().observations==3,
            "error statistics incorrect");
    require(disabled.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,valid,old)==
            ShadowDisposition::BudgetExhausted,"observation cap bypassed");
    require(disabled.counters().observations==3 &&
            disabled.counters().skipped==2 &&
            disabled.counters().has_heap_sample &&
            disabled.counters().min_free_heap==100000,
            "budget or heap statistics changed unexpectedly");

    ShadowDiagnostic invalid(1,-1.0);
    invalid.set_enabled(true);
    require(invalid.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,valid,old)==
            ShadowDisposition::UnsupportedOrError,
            "bad tolerance bypassed non-throwing boundary");
    ShadowDiagnostic partial(1);
    partial.set_enabled(true);
    require(partial.inspect(registry,{"HYD",0xFE,0x06},
                            ApplicationEncoding::StandardDv,{},old)==
            ShadowDisposition::UnsupportedOrError,
            "empty plaintext bypassed non-throwing boundary");
    // A diagnostic error cannot change the existing reference object.
    require(old.numeric.at("target_m3")==16.1 &&
            old.dates.at("target_date")=="2021-12-31",
            "reference outputs changed during diagnostics");

    std::cout<<"SHADOW_DIAGNOSTIC_OK opt-in, bounded, non-throwing, "
                "counter isolation, heap samples, error containment\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr<<"FAIL "<<error.what()<<"\n";
    return 1;
  }
}
