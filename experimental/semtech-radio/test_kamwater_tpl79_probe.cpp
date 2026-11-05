#include "components/wmbus_native_sx126x/kamwater_tpl79_probe.h"
#include "components/wmbus_native_sx126x/kamwater_ell_ii_probe.h"
#include <cassert>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> result;
  int upper=-1;
  for(;*s;++s) {
    const char ch=*s;
    const int d=(ch>='0'&&ch<='9')?ch-'0':
                (ch>='A'&&ch<='F')?ch-'A'+10:-1;
    assert(d>=0);
    if(upper<0) upper=d;
    else {result.push_back(static_cast<uint8_t>(upper*16+d));upper=-1;}
  }
  assert(upper<0);
  return result;
}
int main() {
  using namespace xmq_portable::kamwater;
  uint8_t output[96]{};
  tpl79::Reading verified{};
  // Public upstream MULTICAL21 fixture: compact 79 ED A8 6A B6 followed
  // by NOKEY clear bytes. CRC B66A covers expanded full DIF/VIF records.
  auto first=hex("79EDA86AB6710008190000081900007F13");
  auto first_full=hex(
      "02FF207100041308190000441308190000615B7F616713");
  assert(tpl79::verify(first.data(),first.size(),output,sizeof(output),verified)
         ==tpl79::Status::Ok);
  assert(verified.profile_index==0 && verified.expanded_size==first_full.size());
  for(size_t i=0;i<verified.expanded_size;++i) assert(output[i]==first_full[i]);
  assert(signature::crc16_en13757(output,verified.expanded_size)==0xB66A);
  assert(signature::crc16_en13757(first.data()+5,first.size()-5)!=0xB66A);
  // Second official compact pair: signature 12 C4, data CRC 0D FF.
  auto second=hex("7912C40DFF00002F4E00003D010203");
  auto second_full=hex(
      "02FF20000004132F4E000092013B3D01A1015B028101E7FF0F03");
  assert(tpl79::verify(second.data(),second.size(),output,sizeof(output),verified)
         ==tpl79::Status::Ok);
  assert(verified.profile_index==1 && verified.expanded_size==second_full.size());
  for(size_t i=0;i<verified.expanded_size;++i) assert(output[i]==second_full[i]);
  assert(signature::crc16_en13757(output,verified.expanded_size)==0xFF0D);
  // Unknown format, bad format byte, wrong data CRC, truncated and extra
  // application data are all denied without returning a previous reading.
  auto bad=first;
  bad[1]^=1;
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::UnknownProfile);
  assert(verified.profile_index==-1 && verified.expanded_size==0);
  bad=first;
  bad[3]^=1;
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::DataCrcMismatch);
  assert(verified.expanded_size==0);
  bad=first;
  bad.back()^=1;
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::DataCrcMismatch);
  bad=first;
  bad.pop_back();
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::InvalidCompactData);
  bad=first;bad.push_back(0);
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::InvalidCompactData);
  bad=first;bad[0]=0x78;
  assert(tpl79::verify(bad.data(),bad.size(),output,sizeof(output),verified)
         ==tpl79::Status::UnsupportedCi);
  assert(tpl79::verify(first.data(),first.size(),output,4,verified)
         ==tpl79::Status::Capacity);
  assert(verified.expanded_size==0);
  // ELL II AES-CTR must be denied regardless of valid TPL compact CRC.
  std::vector<uint8_t> ell={
    0x8D,0x20,0x91,0x03,0,0,0x20,0,0
  };
  ell.insert(ell.end(),first.begin(),first.end());
  ell::ClearPayload clear{};
  assert(ell::inspect_ell_ii(ell.data(),ell.size(),clear)
         ==ell::Status::Encrypted);
  assert(clear.tpl==nullptr);
  return 0;
}
