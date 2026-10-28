#include "ixml_subset.h"
#include <cassert>
#include <cstring>
using namespace xmq_portable;
int main() {
  // Original mkradio4 manufacturer payload, straight from official test.
  const uint8_t water[] = {0x06,0x70,0x29,0x01,0x00,0x60,0x17,0x03,
                           0x00,0x04,0x00,0x03,0x00,0x00,0x00};
  const Step prev[] = {{Op::Word,0,nullptr},{Op::Capture,0,"DV_prev"}};
  const Step curr[] = {{Op::Word,0,nullptr},{Op::Capture,0,"DV_curr"}};
  const Step frame[] = {
    {Op::Byte,0,nullptr},{Op::Byte,0,nullptr},{Op::Byte,0,nullptr},
    {Op::Call,0,"Prev"},{Op::Byte,0,nullptr},{Op::Byte,0,nullptr},
    {Op::Call,0,"Curr"},{Op::Rest,0,nullptr}
  };
  const Rule rules[] = {{"decode",frame,8},{"Prev",prev,2},{"Curr",curr,2}};
  Capture captures[4]{};
  Result result{captures,4,0,0};
  assert(execute(rules,3,"decode",water,sizeof(water),result) == ParseStatus::Ok);
  assert(result.count == 2 && result.consumed == sizeof(water));
  assert(std::strcmp(captures[0].name,"DV_prev") == 0);
  assert(captures[0].length == 2 && captures[0].data[0] == 1 && captures[0].data[1] == 0);
  assert(std::strcmp(captures[1].name,"DV_curr") == 0);
  assert(captures[1].length == 2 && captures[1].data[0] == 3 && captures[1].data[1] == 0);
  const uint8_t short_data[] = {0x06,0x70,0x29,0x01};
  assert(execute(rules,3,"decode",short_data,sizeof(short_data),result) == ParseStatus::Truncated);
  return 0;
}
