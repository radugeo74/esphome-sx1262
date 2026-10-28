#include "numeric_bridge.h"
#include "generated_ixml.h"
#include <cassert>
#include <cmath>
using namespace xmq_portable;
int main() {
 const uint8_t payload[] = {0x06,0x70,0x29,0x01,0x00,0x60,0x17,0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00};
 Capture captures[8]{};
 Result result{captures,8,0,0};
 assert(execute(generated_ixml::mkradio4::rules,generated_ixml::mkradio4::count,"decode",payload,sizeof(payload),result)==ParseStatus::Ok);
 NumericValue values{};
 assert(decode_mapped_water(result, generated_ixml::mkradio4::capture_keys, 2, generated_ixml::mkradio4::field_keys, 2, values)==NumericStatus::Ok);
 assert(std::fabs(values.target_m3-0.1)<1e-8);
 assert(std::fabs(values.total_m3-0.4)<1e-8);
 Result missing{captures,8,1,0};
 assert(decode_mapped_water(missing, generated_ixml::mkradio4::capture_keys, 2, generated_ixml::mkradio4::field_keys, 2, values)==NumericStatus::MissingCapture);
 return 0;
}
