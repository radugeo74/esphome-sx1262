#include "xmq_reader.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using xmq_runtime::Node;
using xmq_runtime::Parser;

static void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

static std::string read_file(const std::string &path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static const Node &root(const std::vector<Node> &nodes,
                        const std::string &kind) {
  for (const auto &node : nodes)
    if (node.key == kind && node.block) return node;
  throw std::runtime_error("missing block: " + kind);
}

static void check_driver(const std::string &path, const std::string &name,
                         bool needs_ixml, bool needs_calc) {
  Parser parser;
  const auto tree = parser.parse(read_file(path));
  const auto &driver = root(tree, "driver");
  const Node *field_section = driver.first("fields");
  require(driver.first("name") && driver.first("name")->value == name,
          "wrong driver name: " + path);
  require(driver.first("detect") && driver.first("detect")->block,
          "missing detect block: " + path);
  require(field_section && field_section->block,
          "missing fields block: " + path);
  bool found_ixml = false, found_calc = false, found_test = false;
  for (const auto &field : field_section->children) {
    if (field.key != "field" || !field.block) continue;
    found_ixml |= field.first("ixml") != nullptr;
    found_calc |= field.first("calculate") != nullptr;
  }
  if (const auto *tests = driver.first("tests"))
    for (const auto &test : tests->children)
      found_test |= test.key == "test" && test.block;
  require(!needs_ixml || found_ixml, "missing IXML rule: " + name);
  require(!needs_calc || found_calc, "missing calculate rule: " + name);
  require(found_test, "missing upstream test fixture: " + name);
  std::cout << "STRUCTURAL_OK " << name << "\n";
}

int main() {
  const std::string base = "experimental/xmq-runtime-stage-a/upstream/";
  try {
    check_driver(base + "src/iperl.xmq", "iperl", false, false);
    check_driver(base + "src/mkradio4a.xmq", "mkradio4a", false, false);
    check_driver(base + "src/mkradio4.xmq", "mkradio4", true, true);
    check_driver(base + "src/fhkvdataiii.xmq", "fhkvdataiii", true, true);
    const auto lib = Parser().parse(read_file(base + "library.xmq"));
    require(!root(lib, "library").children.empty(), "empty upstream library");

    bool bad_input_rejected = false;
    try { Parser().parse("driver { name = broken\n"); }
    catch (const xmq_runtime::ParseError &) { bad_input_rejected = true; }
    require(bad_input_rejected, "unclosed block accepted");

    bad_input_rejected = false;
    try { Parser(8).parse("driver { name = oversized }"); }
    catch (const xmq_runtime::ParseError &) { bad_input_rejected = true; }
    require(bad_input_rejected, "size limit ignored");

    std::cout << "STRUCTURAL_OK upstream library; invalid inputs rejected\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
  }
}
