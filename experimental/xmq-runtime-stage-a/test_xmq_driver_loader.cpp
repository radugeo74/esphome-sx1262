#include "xmq_driver_loader.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

static void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
template <class F>
static bool rejected(F &&f) {
  try { f(); }
  catch (const xmq_runtime::DriverLoadError &) { return true; }
  return false;
}
int main() {
  namespace fs=std::filesystem;
  using namespace xmq_runtime;
  const fs::path original="experimental/xmq-runtime-stage-a/upstream/src";
  try {
    const auto loaded=load_registry_from_directory(original);
    check(loaded.select({"SEN",0x68,0x06}).name=="iperl","iperl load");
    check(loaded.select({"HYD",0xFE,0x06}).name=="mkradio4a","mkradio4a load");
    check(loaded.select({"TCH",0x95,0x62}).name=="mkradio4","mkradio4 load");
    check(loaded.select({"TCH",0x94,0x80}).name=="fhkvdataiii","HCA load");
    const auto selected=loaded.decode_standard(
        {"HYD",0xFE,0x06},parse_hex("4315A10000426CBF2C"));
    const auto *target=find_projected(selected.numeric,"target_m3");
    check(target!=nullptr && std::fabs(target->number-16.1)<1e-9,
          "loaded driver decode");
    check(selected.dates.at("target")=="2021-12-31",
          "loaded driver OMS date");
    check(rejected([&] { (void)load_registry_from_directory(original,2); }),
          "driver count limit");
    check(rejected([&] { (void)load_registry_from_directory(original,128,64); }),
          "file size limit");
    check(rejected([&] { (void)load_registry_from_directory(original/"missing_directory"); }),
          "nonexistent directory accepted");
    check(rejected([&] { (void)load_registry_from_directory(original,0); }),
          "zero driver count allowed");

    // Only a test-local temporary directory. Never write into upstream.
    const auto sandbox=fs::temp_directory_path()/"xmq-stage-a-loader-tests";
    fs::remove_all(sandbox);
    fs::create_directories(sandbox);
    try {
      {
        std::ofstream out(sandbox/"alias.xmq");
        out<<"driver { name = not_alias\n detect { mvt = TCH,95,37 } }";
      }
      check(rejected([&] { (void)load_registry_from_directory(sandbox); }),
            "driver alias/name mismatch accepted");
      fs::remove(sandbox/"alias.xmq");
      {
        std::ofstream out(sandbox/"bad.xmq");
        out<<"driver { name = bad\n";
      }
      bool parse_error=false;
      try { (void)load_registry_from_directory(sandbox); }
      catch (const ParseError &) { parse_error=true; }
      check(parse_error,"malformed XMQ accepted");
      fs::remove(sandbox/"bad.xmq");
      fs::create_symlink(original/"iperl.xmq",sandbox/"linked.xmq");
      check(rejected([&] { (void)load_registry_from_directory(sandbox); }),
            "XMQ symlink accepted");
      fs::remove_all(sandbox);
    } catch (...) { fs::remove_all(sandbox); throw; }

    std::cout<<"DRIVER_LOADER_OK original XMQ directory / limits / safety / decode\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
