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
 CalcValue env[16]{};
 size_t env_count = 0;
 assert(seed_calculation_fields(result, generated_ixml::mkradio4::capture_keys, 2,
     generated_ixml::mkradio4::field_keys, 2, env, env_count, 16) == NumericStatus::Ok);
 assert(calculate(generated_ixml::mkradio4::calculations,
     generated_ixml::mkradio4::calculation_count, env, env_count, 16) == CalcStatus::Ok);
 bool target_ok = false, total_ok = false;
 for (size_t i = 0; i < env_count; ++i) {
   if (std::strcmp(env[i].name, "target") == 0)
     target_ok = std::fabs(env[i].value - 0.1) < 1e-8;
   if (std::strcmp(env[i].name, "total") == 0)
     total_ok = std::fabs(env[i].value - 0.4) < 1e-8;
 }
 assert(target_ok && total_ok);
 Result missing{captures,8,1,0};
 assert(decode_mapped_water(missing, generated_ixml::mkradio4::capture_keys, 2, generated_ixml::mkradio4::field_keys, 2, values)==NumericStatus::MissingCapture);
 const uint8_t heat[] = {0x0F,0x9F,0x29,0x2D,0x00,0x50,0x24,0x04,0x00,
                         0x11,0xBD,0x08,0x38,0x09,0x04,0x00,0x00,0x00,
                         0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                         0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
                         0x00,0x00,0x03,0x14,0x0E};
 Capture heat_caps[12]{};
 Result heat_result{heat_caps,12,0,0};
 assert(execute(generated_ixml::fhkvdataiii::rules,generated_ixml::fhkvdataiii::count,
                "decode",heat,sizeof(heat),heat_result)==ParseStatus::Ok);
 Record hca{};
 assert(lookup_mapped_record(heat_result,generated_ixml::fhkvdataiii::capture_keys,6,
                             generated_ixml::fhkvdataiii::field_keys,2,
                             "previous_date_raw",hca)==NumericStatus::Ok);
 assert(hca.raw==0x299F);
 return 0;
}
