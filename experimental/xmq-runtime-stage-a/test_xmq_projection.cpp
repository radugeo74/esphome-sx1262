#include "xmq_projection.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static std::string read_upstream(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/" + name + ".xmq");
  if (!f) throw std::runtime_error("missing original XMQ driver");
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void require(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static void expect(const std::vector<xmq_runtime::ProjectedValue> &fields,
                   const std::string &name, double value) {
  const auto *field = xmq_runtime::find_projected(fields, name);
  require(field != nullptr, "missing projected numeric field");
  require(std::abs(field->number - value) < 0.0000001,
          "wrong projected numeric value");
}

int main() {
  using namespace xmq_runtime;
  try {
    const Driver iperl = load_driver(read_upstream("iperl"));
    auto dv1 = decode_dv_subset(parse_hex("2F2F0413181E0000023B00002F2F2F2F"));
    auto out1 = project_numeric_fields(iperl, dv1);
    require(out1.size() == 2, "iperl field count");
    expect(out1, "total_m3", 7.704);
    expect(out1, "max_flow_m3h", 0.0);

    auto dv2 = decode_dv_subset(parse_hex("041389E20100023B0000"));
    auto out2 = project_numeric_fields(iperl, dv2);
    expect(out2, "total_m3", 123.529);
    expect(out2, "max_flow_m3h", 0.0);

    const Driver mk4a = load_driver(read_upstream("mkradio4a"));
    auto out3 = project_numeric_fields(
        mk4a, decode_dv_subset(parse_hex("4315A10000426CBF2C")));
    require(out3.size() == 1, "mk4a numeric field count (date excluded)");
    expect(out3, "target_m3", 16.1);

    auto out4 = project_numeric_fields(
        mk4a, decode_dv_subset(parse_hex("4315F00200426CBF2C")));
    expect(out4, "target_m3", 75.2);

    // A malformed/ambiguous DV stream must not yield an arbitrary field.
    auto dup = dv1;
    dup.push_back(dv1.front());
    bool ambiguous = false;
    try { (void)project_numeric_fields(iperl, dup); }
    catch (const ProjectionError &) { ambiguous = true; }
    require(ambiguous, "ambiguous match accepted");

    // Upstream mkradio4 depends on IXML; match-only evaluation must not
    // claim to produce its target/total values.
    const Driver mk4 = load_driver(read_upstream("mkradio4"));
    require(project_numeric_fields(mk4, {}).empty(),
            "calculated mkradio4 fields fabricated");
    std::cout << "PROJECTION_SUBSET_OK: upstream iperl, mkradio4a numerical fields\n";
    std::cout << "Not full XMQ parity: date, status, IXML, AES are unsupported\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}
