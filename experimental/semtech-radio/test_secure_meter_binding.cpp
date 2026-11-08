// Exercise the exact secure_bindings::add() used by
// NativeReceiverProbe::add_secure_meter(), not a decoder-only mock.
// All IDs and AES material here are synthetic/public test values.
#include "components/wmbus_native_sx126x/secure_meter_binding.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using esphome::wmbus_native_sx126x::secure_bindings::Entry;
using esphome::wmbus_native_sx126x::secure_bindings::add;
using esphome::wmbus_native_sx126x::secure_bindings::supported_driver;

int main() {
  const std::string key = "000102030405060708090A0B0C0D0E0F";
  std::vector<Entry> bindings{};
  assert(add(bindings,"12340001","minomess",key,"55036410"));
  assert(add(bindings,"12340002","kamwater",key,"12340002"));
  assert(add(bindings,"12340003","apator162",key,"12340003"));
  assert(add(bindings,"12340004","mkradio4a",key,"55036414"));
  assert(add(bindings,"12340005","topaseskr",key,"12340005"));
  assert(add(bindings,"12340006","lansenth",key,"12340006"));
  assert(add(bindings,"12340007","lansensm",key,"12340007"));
  assert(bindings.size()==7);
  const char *drivers[]={"minomess","kamwater","apator162","mkradio4a","topaseskr","lansenth","lansensm"};
  for(size_t i=0;i<7;++i) {
    assert(supported_driver(drivers[i]));
    assert(bindings[i].driver==drivers[i]);
    assert(bindings[i].key[0]==0x00);
    assert(bindings[i].key[1]==0x01);
    assert(bindings[i].key[14]==0x0E);
    assert(bindings[i].key[15]==0x0F);
  }
  assert(bindings[3].dll_id=="55036414");
  assert(bindings[3].id=="12340004");
  assert(bindings[4].dll_id=="12340005");
  assert(!supported_driver("auto"));
  assert(!supported_driver("invalid_driver"));
  const auto rejected=[&](const std::string &id,
                          const std::string &driver,
                          const std::string &key_hex,
                          const std::string &dll_id) {
    const size_t before=bindings.size();
    assert(!add(bindings,id,driver,key_hex,dll_id));
    assert(bindings.size()==before);
  };
  rejected("12340004","mkradio4a",key,"55036417"); // duplicated actual meter
  rejected("12340008","mkradio4a",key,"55036414"); // duplicated DLL sender
  rejected("12340008","topaseskr",key,"55036418"); // different DLL prohibited
  rejected("12340009","kamwater",key,"55036419"); // different DLL prohibited
  rejected("12340010","apator162",key,"55036420"); // different DLL prohibited
  rejected("12340011","auto",key,"12340011"); // no key-guessing AUTO
  rejected("12340012","mkradio4",key,"12340012"); // plaintext driver
  rejected("12340013","unsupported",key,"12340013");
  rejected("1234A014","topaseskr",key,"1234A014");
  rejected("1234001","topaseskr",key,"1234001");
  rejected("12340015","topaseskr",key,"1234001Z");
  rejected("12340016","topaseskr",key.substr(0,30),"12340016");
  rejected("12340017","topaseskr","000102030405060708090A0B0C0D0E0Z","12340017");
  assert(bindings.size()==7);
  // Failure after parsing several valid bytes must not add a partial key.
  std::puts("PASS runtime secure_meters registration 7/7; Lansen TH/SM bound; "
            "invalid IDs, keys, duplicate bindings and unsupported AUTO fail closed.");
}
