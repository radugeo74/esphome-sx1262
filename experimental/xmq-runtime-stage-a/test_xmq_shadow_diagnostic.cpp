#include "xmq_shadow_diagnostic.h"
#include <cmath>
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
    XmqFrameResult stage1_candidate;
    ShadowDiagnostic single_pass(2);
    single_pass.set_enabled(true);
    require(single_pass.inspect(registry, {"HYD",0xFE,0x06},
                ApplicationEncoding::StandardDv, valid, old,
                0, false, "mkradio4a", {"target_m3"}, &independent,
                {"target_date"}, &stage1_candidate) == ShadowDisposition::Match,
            "single-pass readiness changed legacy shadow outcome");
    require(independent.ready && independent.required == 2 &&
            independent.available == 2,
            "single-pass independent readiness omitted XMQ date/numeric");
    require(stage1_candidate.driver == "mkradio4a" &&
            stage1_candidate.numeric.count("target_m3") == 1 &&
            stage1_candidate.dates.count("target_date") == 1,
            "one-pass XMQ numeric/date output missing");
    independent.ready = true;
    require(single_pass.inspect(registry, {"HYD",0xFE,0x06},
                ApplicationEncoding::StandardDv, {}, old,
                0, false, "mkradio4a", {"target_m3"}, &independent,
                {"target_date"}, &stage1_candidate) == ShadowDisposition::UnsupportedOrError &&
            !independent.ready && independent.available == 0 &&
            stage1_candidate.driver.empty() &&
            stage1_candidate.numeric.empty() && stage1_candidate.dates.empty(),
            "failed parse retained stale readiness/output from prior telegram");

    // Stage 1: one selected mkradio4 uses ONLY the generic XMQ projection
    // returned by the same diagnostic pass; no legacy value is copied into it.
    DriverRegistry mk4_registry({load_driver(upstream("mkradio4"))});
    const auto mk4_payload=parse_hex(
        "06702901006017030004000300000000000000000000000000000000000000000000");
    ShadowReference mk4_legacy;
    mk4_legacy.numeric={{"target_m3",0.1},{"total_m3",0.4}};
    ShadowDiagnostic mk4_probe(3);
    mk4_probe.set_enabled(true);
    XmqFrameResult mk4_xmq;
    PrimaryReadiness mk4_ready;
    require(mk4_probe.inspect(mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, mk4_payload, mk4_legacy,
                160000, true, "mkradio4", {"target_m3","total_m3"},
                &mk4_ready, {}, &mk4_xmq)==ShadowDisposition::Match,
            "Stage 1 mkradio4 XMQ and legacy fixture did not match");
    require(mk4_ready.ready && mk4_ready.available==2 &&
            mk4_xmq.driver=="mkradio4" &&
            std::fabs(mk4_xmq.numeric.at("target_m3").value-0.1)<1e-8 &&
            std::fabs(mk4_xmq.numeric.at("total_m3").value-0.4)<1e-8,
            "Stage 1 independent XMQ numeric outputs missing or incorrect");
    // A mismatch must be detected; independent XMQ values stay original.
    mk4_legacy.numeric["total_m3"]=9.4;
    require(mk4_probe.inspect(mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, mk4_payload, mk4_legacy,
                160000, true, "mkradio4", {"target_m3","total_m3"},
                &mk4_ready, {}, &mk4_xmq)==ShadowDisposition::Difference &&
            std::fabs(mk4_xmq.numeric.at("total_m3").value-0.4)<1e-8,
            "Stage 1 XMQ copied mismatched legacy output");
    require(mk4_probe.inspect(mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, {}, mk4_legacy,
                160000, true, "mkradio4", {"target_m3","total_m3"},
                &mk4_ready, {}, &mk4_xmq)==ShadowDisposition::UnsupportedOrError &&
            !mk4_ready.ready && mk4_xmq.numeric.empty(),
            "Stage 1 stale XMQ values retained after invalid telegram");

    // Stage 4/5 reuse: the caller has separately checked exact raw-T1
    // application bytes, MVT, encoding and legacy parity. The shadow
    // comparator must still apply its own expected-field contract.
    const auto preprojected = project_verified_application(
        mk4_registry, {"TCH",0x95,0x62},
        ApplicationEncoding::OriginalIxml, mk4_payload, "mkradio4");
    ShadowDiagnostic reuse_probe(3, 0.051);
    reuse_probe.set_enabled(true);
    ShadowReference reuse_reference;
    reuse_reference.numeric={{"target_m3",0.1},{"total_m3",0.4}};
    PrimaryReadiness reused_ready;
    XmqFrameResult reused_output;
    require(reuse_probe.inspect(
                mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, mk4_payload, reuse_reference,
                160000,true,"mkradio4",{"target_m3","total_m3"},
                &reused_ready,{},&reused_output,&preprojected)
                == ShadowDisposition::Match &&
            reused_ready.ready && reused_output.driver=="mkradio4" &&
            reused_output.numeric.count("target_m3")==1 &&
            reused_output.numeric.count("total_m3")==1,
            "preprojection reuse rejected a fully matched MK4 frame");
    reuse_reference.numeric["total_m3"] = 9.4;
    require(reuse_probe.inspect(
                mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, mk4_payload, reuse_reference,
                160000,true,"mkradio4",{"target_m3","total_m3"},
                &reused_ready,{},&reused_output,&preprojected)
                == ShadowDisposition::Difference,
            "preprojection reuse concealed legacy mismatch");
    reuse_reference.numeric.erase("total_m3");
    require(reuse_probe.inspect(
                mk4_registry, {"TCH",0x95,0x62},
                ApplicationEncoding::OriginalIxml, mk4_payload, reuse_reference,
                160000,true,"mkradio4",{"target_m3","total_m3"},
                &reused_ready,{},&reused_output,&preprojected)
                == ShadowDisposition::IncompleteReference,
            "preprojection reuse accepted incomplete legacy reference");

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
