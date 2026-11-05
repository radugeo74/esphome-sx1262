#include "components/wmbus_native_sx126x/kamwater_clear_pipeline.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;int hi=-1;
  for(;*s;++s) {
    const int n=(*s>='0'&&*s<='9')?*s-'0':
                (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(n>=0);
    if(hi<0)hi=n;
    else {out.push_back(static_cast<uint8_t>(hi*16+n));hi=-1;}
  }
  assert(hi<0);return out;
}
static void refresh_ell_crc(std::vector<uint8_t> &frame) {
  using namespace xmq_portable::kamwater;
  const uint16_t crc=signature::crc16_en13757(frame.data()+9,frame.size()-9);
  frame[7]=static_cast<uint8_t>(crc);
  frame[8]=static_cast<uint8_t>(crc>>8);
}
int main() {
  using namespace xmq_portable::kamwater;
  using namespace xmq_portable::kamwater::pipeline;
  using PipelineStatus = xmq_portable::kamwater::pipeline::Status;
  // Official compact TPL reference carried in a SYNTHETIC clear ELL II.
  auto tpl=hex("79EDA86AB6710008190000081900007F13");
  std::vector<uint8_t> frame={0x8D,0x20,0x01,0x03,0,0,0,0,0};
  frame.insert(frame.end(),tpl.begin(),tpl.end());
  refresh_ell_crc(frame);
  Preconditions trusted{true,true};
  Output result{};
  assert(decode_clear_ell_ii_tpl79(frame.data(),frame.size(),trusted,result)
         ==PipelineStatus::Ok);
  assert(result.profile_index==0);
  assert(std::fabs(result.reading.total_m3-6.408)<1e-8);
  assert(result.reading.has_target &&
         std::fabs(result.reading.target_m3-6.408)<1e-8);
  assert(result.reading.has_status && result.reading.status_bits==0x71);
  // Both independent external preconditions remain mandatory.
  assert(decode_clear_ell_ii_tpl79(frame.data(),frame.size(),
                                  Preconditions{false,true},result)
         ==PipelineStatus::UnverifiedDll);
  assert(result.profile_index==-1 && result.reading.total_m3==0);
  assert(decode_clear_ell_ii_tpl79(frame.data(),frame.size(),
                                  Preconditions{true,false},result)
         ==PipelineStatus::UnverifiedIdentity);
  assert(result.profile_index==-1 && !result.reading.has_target);
  // Corrupt ELL CRC.
  auto bad=frame;bad[7]^=1;
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::EllRejected);
  assert(result.profile_index==-1 && result.reading.total_m3==0);
  // ELL AES-CTR cannot be bypassed even if the payload looks valid.
  bad=frame;bad[6]=0x20;
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::EllRejected);
  // Corrupt TPL data CRC while recomputing ELL to isolate the TPL check.
  bad=frame;bad[12]^=1;refresh_ell_crc(bad);
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::CompactRejected);
  assert(result.profile_index==-1 && result.reading.total_m3==0);
  // Corrupt the format signature; a recomputed ELL CRC must not help.
  bad=frame;bad[10]^=1;refresh_ell_crc(bad);
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::CompactRejected);
  // Corrupt a value but preserve ELL CRC; TPL CRC still rejects.
  bad=frame;bad.back()^=1;refresh_ell_crc(bad);
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::CompactRejected);
  // A truncated compact payload may have its ELL CRC recalculated, but
  // must not be reinterpreted as a successful shorter profile.
  bad=frame;bad.pop_back();refresh_ell_crc(bad);
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::CompactRejected);
  // Unknown TPL: ELL probe refuses entry into compact decoding.
  bad=frame;bad[9]=0x72;refresh_ell_crc(bad);
  assert(decode_clear_ell_ii_tpl79(bad.data(),bad.size(),trusted,result)
         ==PipelineStatus::EllRejected);
  assert(result.profile_index==-1 && !result.reading.has_status);
  return 0;
}
