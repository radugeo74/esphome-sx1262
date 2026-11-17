#include "xmq_source_registry.h"
#include "xmq_dv_subset.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool value,const char *why) {
  if (!value) throw std::runtime_error(why);
}
template<class F>
static bool source_rejected(F &&fn) {
  try { fn(); }
  catch (const xmq_runtime::SourceRegistryError &) { return true; }
  return false;
}
static std::string upstream(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/"+name+".xmq");
  if (!f) throw std::runtime_error("upstream driver not found");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}

int main() {
  using namespace xmq_runtime;
  try {
    const std::map<std::string,std::string> sources{
      {"iperl",upstream("iperl")},
      {"mkradio4a",upstream("mkradio4a")},
      {"mkradio4",upstream("mkradio4")},
      {"fhkvdataiii",upstream("fhkvdataiii")}
    };
    size_t reads=0;
    const XmqSourceReader read=[&](const std::string &key) {
      ++reads;
      return sources.at(key);
    };
    const std::vector<std::string> names{
      "iperl","mkradio4a","mkradio4","fhkvdataiii"
    };
    const auto registry=load_registry_from_reader(names,read);
    require(reads==4,"expected one read per source");
    require(registry.select({"SEN",0x68,0x06}).name=="iperl",
            "selected wrong iperl driver");
    require(registry.select({"HYD",0xFE,0x06}).name=="mkradio4a",
            "selected wrong HYD driver");
    require(registry.select({"TCH",0x95,0x62}).name=="mkradio4",
            "selected wrong mkradio4 driver");
    require(registry.select({"TCH",0x94,0x80}).name=="fhkvdataiii",
            "selected wrong heat driver");

    const auto water=registry.decode_standard({"HYD",0xFE,0x06},
                                              parse_hex("4315A10000426CBF2C"));
    const auto *target=find_projected(water.numeric,"target_m3");
    require(target && std::fabs(target->number-16.1)<1e-9,
            "XMQ source reader lost numeric matching");
    require(water.dates.at("target")=="2021-12-31",
            "XMQ source reader lost OMS date");

    require(source_rejected([&]{
      (void)load_registry_from_reader({"iperl","iperl"},read);
    }),"duplicate source identifiers accepted");
    require(source_rejected([&]{
      (void)load_registry_from_reader({"../iperl"},read);
    }),"path traversal source identifier accepted");
    require(source_rejected([&]{
      (void)load_registry_from_reader({"iperl"},read,0);
    }),"invalid driver count accepted");
    require(source_rejected([&]{
      (void)load_registry_from_reader(names,read,2);
    }),"too many XMQ sources accepted");
    require(source_rejected([&]{
      (void)load_registry_from_reader(names,read,128,64);
    }),"oversized XMQ source accepted");
    require(source_rejected([&]{
      (void)load_registry_from_reader(names,read,128,256*1024,128);
    }),"combined XMQ memory limit ignored");
    require(source_rejected([&]{
      (void)load_registry_from_reader({"iperl"},[](const std::string &){
        return std::string{};
      });
    }),"empty XMQ source accepted");
    // Driver names come from upstream XMQ and must match the source slot.
    require(source_rejected([&]{
      (void)load_registry_from_reader({"alias"},[&](const std::string &){
        return sources.at("iperl");
      });
    }),"XMQ alias was allowed to replace a driver");

    std::cout<<"SOURCE_REGISTRY_OK platform-neutral source callback and safety limits\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
