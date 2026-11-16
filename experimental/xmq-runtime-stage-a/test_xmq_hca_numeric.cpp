#include "xmq_numeric_runtime.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

static void check(bool ok, const char *what) {
  if (!ok) throw std::runtime_error(what);
}
static std::string upstream(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/" + name + ".xmq");
  if (!f) throw std::runtime_error("missing upstream XMQ");
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static void expect(const std::map<std::string,xmq_runtime::Number> &values,
                   const char *name, double wanted,
                   xmq_runtime::Dimension dim) {
  auto it=values.find(name);
  if (it==values.end()) throw std::runtime_error(std::string("missing field ")+name);
  check(it->second.dimension==dim,"wrong numeric dimension");
  check(std::abs(it->second.value-wanted)<0.00000001,"wrong numeric value");
}
int main() {
  using namespace xmq_runtime;
  try {
    const Driver d=load_driver(upstream("fhkvdataiii"));
    // Plaintext manufacturer payload from original upstream XMQ fixture
    // ('Rooom', version 0x94, 2021-02-05, current=4 HCA).
    const auto clear=parse_hex(
        "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E");
    const auto dv=decode_ixml_numeric(d,clear);
    check(dv.size()==6,"expected six synthetic DIF/VIF records");
    const auto values=evaluate_numeric_fields(d,dv);
    expect(values,"previous_hca",45,Dimension::HCA);
    expect(values,"current_hca",4,Dimension::HCA);
    expect(values,"temp_room_c",22.37,Dimension::TemperatureC);
    expect(values,"temp_radiator_c",23.60,Dimension::TemperatureC);
    expect(values,"previous_date_raw_counter",0x299F,Dimension::Counter);
    expect(values,"current_date_raw_counter",0x2450,Dimension::Counter);
    expect(values,"previous_day_counter",31,Dimension::Counter);
    expect(values,"previous_month_counter",12,Dimension::Counter);
    expect(values,"previous_year_counter",20,Dimension::Counter);
    expect(values,"current_day_counter",5,Dimension::Counter);
    expect(values,"current_month_counter",2,Dimension::Counter);
    expect(values,"current_year_counter",21,Dimension::Counter);

    // Semantic isolation: an unsupported DV range must never produce
    // made-up HCA or temperature values.
    const auto only_volume=decode_dv_subset(parse_hex("0413181E0000"));
    const auto empty=evaluate_numeric_fields(
        load_driver(upstream("iperl")),only_volume);
    check(empty.find("previous_hca")==empty.end(),"HCA value fabricated");

    std::cout<<"HCA_NUMERIC_OK: original fhkvdataiii 0F payload; "
             <<"HCA, temperatures and calendar components\n";
    std::cout<<"Complete calendar dates and full upstream parity NOT yet checked\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
