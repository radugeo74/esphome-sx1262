#include "xmq_expression.h"
#include "xmq_semantic.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

static void require(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static std::string read_driver(const std::string &name) {
  std::ifstream in("experimental/xmq-runtime-stage-a/upstream/src/" + name + ".xmq");
  if (!in) throw std::runtime_error("cannot open upstream XMQ");
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
static const xmq_runtime::FieldRule *formula(
    const xmq_runtime::Driver &driver, const std::string &field) {
  for (const auto &r : driver.fields)
    if (r.name == field && !r.calculate.empty()) return &r;
  return nullptr;
}
static bool rejects(xmq_runtime::NumericExpression &eval,
                    const std::string &expression) {
  try { (void) eval.evaluate(expression); }
  catch (const xmq_runtime::ExpressionError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    const auto mk = load_driver(read_driver("mkradio4"));
    const auto *previous = formula(mk,"prev");
    const auto *current = formula(mk,"curr");
    const auto *target = formula(mk,"target");
    const auto *total = formula(mk,"total");
    require(previous && current && target && total,"original mkradio4 formulas present");

    std::map<std::string,Number> vars{
      {"prev_raw_counter",{89.1,Dimension::Counter}},
      {"curr_raw_counter",{2.2,Dimension::Counter}}
    };
    NumericExpression calc(vars);
    const auto prev = calc.evaluate(previous->calculate);
    const auto curr = calc.evaluate(current->calculate);
    require(prev.dimension == Dimension::VolumeM3 &&
            std::abs(prev.value-89.1) < 1e-9,"mkradio4 prev unit");
    require(curr.dimension == Dimension::VolumeM3 &&
            std::abs(curr.value-2.2) < 1e-9,"mkradio4 curr unit");
    vars["prev_m3"] = prev;
    vars["curr_m3"] = curr;
    require(std::abs(NumericExpression(vars).evaluate(target->calculate).value-89.1) < 1e-9,
            "mkradio4 target");
    auto total_result = NumericExpression(vars).evaluate(total->calculate);
    require(total_result.dimension == Dimension::VolumeM3 &&
            std::abs(total_result.value-91.3) < 1e-9,"mkradio4 total");

    const auto hca = load_driver(read_driver("fhkvdataiii"));
    const auto *day = formula(hca,"previous_day");
    const auto *month = formula(hca,"previous_month");
    const auto *year = formula(hca,"previous_year");
    require(day && month && year,"original fhkvdataiii formulas present");
    const unsigned raw_date = (26u << 9) | (11u << 5) | 16u;
    std::map<std::string, Number> dates{
      {"previous_date_raw_counter",{static_cast<double>(raw_date),Dimension::Counter}}
    };
    NumericExpression hca_calc(dates);
    require(hca_calc.evaluate(day->calculate).value == 16,"HCA previous day");
    require(hca_calc.evaluate(month->calculate).value == 11,"HCA previous month");
    require(hca_calc.evaluate(year->calculate).value == 26,"HCA previous year");

    std::map<std::string, Number> logical{{"current_month_counter",{12,Dimension::Counter}},
      {"previous_month_counter",{11,Dimension::Counter}},
      {"previous_year_counter",{26,Dimension::Counter}},
      {"current_day_counter",{1,Dimension::Counter}},
      {"previous_day_counter",{16,Dimension::Counter}},
      {"current_date_raw_counter",{static_cast<double>(raw_date),Dimension::Counter}}};
    NumericExpression c(logical);
    require(c.evaluate("(previous_year_counter + (current_month_counter > previous_month_counter))").value == 27,
            "year rollover comparison");
    require(c.evaluate("((current_date_raw_counter >> 4counter) % 32counter) + ((((current_date_raw_counter >> 4counter) % 32counter) <= 0counter) * 1counter)").value >= 0,
            "nested comparison and modulo");

    require(rejects(c,"'2000-01-01' + 1month"),"unsupported date syntax rejected");
    require(rejects(c,"1m3 + 2counter"),"incompatible units rejected");
    require(rejects(c,"1 / 0"),"division by zero rejected");
    require(rejects(c,"999counter >> 63counter"),"oversized shift rejected");
    require(rejects(c,"unknown_counter + 1counter"),"unresolved variable rejected");
    std::cout << "NUMERIC_SUBSET_OK upstream mkradio4 and fhkvdataiii formulas\n";
    std::cout << "Date/IXML/telegram parity NOT implemented\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
  }
}
