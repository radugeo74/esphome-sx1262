#include "xmq_mounted_vfs.h"
#include "xmq_dv_subset.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool condition,const char *message) {
  if (!condition) throw std::runtime_error(message);
}
template<class F>
static bool flash_rejects(F fn) {
  try { fn(); }
  catch (const xmq_runtime::FlashSourceError &) { return true; }
  return false;
}
template<class F>
static bool source_rejects(F fn) {
  try { fn(); }
  catch (const xmq_runtime::SourceRegistryError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  namespace fs=std::filesystem;
  const fs::path root=fs::temp_directory_path()/"xmq-stage-a-mounted-vfs-fixtures";
  try {
    fs::remove_all(root);
    fs::create_directories(root);
    const std::vector<std::string> names{
      "iperl","mkradio4a","mkradio4","fhkvdataiii"
    };
    for (const auto &name : names) {
      fs::copy_file(fs::path("experimental/xmq-runtime-stage-a/upstream/src")/
                    (name+".xmq"),root/(name+".xmq"));
    }
    MountedXmqFiles flash(root.string());
    const auto drivers=flash.load(names);
    check(drivers.select({"SEN",0x68,0x06}).name=="iperl",
          "flash iperl auto driver");
    check(drivers.select({"HYD",0xFE,0x06}).name=="mkradio4a",
          "flash mkradio4a auto driver");
    check(drivers.select({"TCH",0x95,0x62}).name=="mkradio4",
          "flash mkradio4 auto driver");
    check(drivers.select({"TCH",0x94,0x80}).name=="fhkvdataiii",
          "flash fhkvdataiii auto driver");
    const auto decoded=drivers.decode_standard(
      {"HYD",0xFE,0x06},parse_hex("4315A10000426CBF2C"));
    const auto *volume=find_projected(decoded.numeric,"target_m3");
    check(volume && std::fabs(volume->number-16.1)<1e-9,
          "mounted XMQ changed numeric decoding");
    check(decoded.dates.at("target")=="2021-12-31",
          "mounted XMQ changed calendar decoding");

    check(flash_rejects([&]{(void)flash.read("../iperl");}),
          "directory escape accepted");
    check(flash_rejects([&]{(void)flash.read("missing");}),
          "nonexistent flash file accepted");
    check(flash_rejects([&]{(void)MountedXmqFiles("relative");}),
          "relative mount root accepted");
    check(flash_rejects([&]{(void)MountedXmqFiles("/safe/../unsafe");}),
          "untrusted mount root accepted");
    check(flash_rejects([&]{(void)flash.load(names,9);}),
          "over-budget flash driver count accepted");
    check(flash_rejects([&]{(void)flash.load(names,8,32769);}),
          "over-budget total flash source size accepted");
    check(source_rejects([&]{(void)flash.load(names,2);}),
          "too many files bypassed generic registry limit");

    MountedXmqFiles tiny(root.string(),128);
    check(flash_rejects([&]{(void)tiny.read("iperl");}),
          "oversized flash file accepted");
    {
      std::ofstream file(root/"empty.xmq",std::ios::binary);
    }
    check(flash_rejects([&]{(void)flash.read("empty");}),
          "empty flash file accepted");
    {
      std::ofstream file(root/"bad.xmq",std::ios::binary);
      file<<"not an XMQ source";
    }
    bool parse_failed=false;
    try {(void)flash.load({"bad"});}
    catch (const ParseError &) { parse_failed=true; }
    check(parse_failed,"malformed flash XMQ accepted");

    // Files must remain read-only; adapter does not modify upstream copies.
    check(fs::file_size(root/"fhkvdataiii.xmq")>10000,
          "flash fixture unexpectedly changed");
    fs::remove_all(root);
    std::cout<<"MOUNTED_VFS_OK bounded read-only XMQ flash-style adapter on host\n";
    std::cout<<"Not compiled under ESP-IDF; no ESP32 RAM measurements\n";
    return 0;
  } catch (const std::exception &error) {
    fs::remove_all(root);
    std::cerr<<"FAIL: "<<error.what()<<"\n";
    return 1;
  }
}
