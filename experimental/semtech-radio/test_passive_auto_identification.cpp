// Pure host regression for the EXACT passive AUTO helper used by
// NativeReceiverProbe::log_native_auto_ after the radio DLL CRC gate.
// No AES key, decode, RF ownership, HA output, or actual user meter IDs.
#include "components/wmbus_native_sx126x/passive_auto_identify.h"
#include "xmq_auto_registry.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace wmbus_pure;
namespace passive=passive_auto;
namespace diag=batch2_identify;

static std::vector<uint8_t> frame(uint16_t manufacturer,uint8_t ver,
                                  uint8_t type,uint8_t ci) {
  const size_t size=ci==0x72 ? 23 : 15;
  std::vector<uint8_t> out(size,0);
  out[0]=static_cast<uint8_t>(size-1);
  out[1]=0x44;
  out[2]=static_cast<uint8_t>(manufacturer);
  out[3]=static_cast<uint8_t>(manufacturer>>8);
  out[4]=0x78;out[5]=0x56;out[6]=0x34;out[7]=0x12;
  out[8]=ver;out[9]=type;out[10]=ci;
  return out;
}
static diag::Result recognize(const std::vector<uint8_t> &bytes,
                              const DriverSignature *s=generated::signatures,
                              size_t count=generated::signature_count) {
  return passive::inspect(bytes.data(),bytes.size(),s,count);
}

int main() {
  // Exactly 38 configured native driver families. Match their actual
  // upstream detect.mvt entries, not handpicked invented signatures.
  const char *const drivers[]={
    "fhkvdataiii","mkradio4","iperl","hydrus",
    "lansenpu","lansendw","cma12w","ultraheat","sontex868",
    "supercal","q400","minomess","kamwater","apator162",
    "mkradio4a","topaseskr","lansenth","lansensm",
    "elvsense","lansenrp","qsmoke","qcaloric","rfmamb","sensostar","evo868","supercom587","hcae2","bfw240radio","qwaterv2","hydrodigit","itron","qualcosonic","lse_08","waterstarm","lse_07_17","munia","qheat_55_us","maddalena"
  };
  size_t recognized=0;
  for(const char *name : drivers) {
    const DriverSignature *signature=nullptr;
    for(size_t i=0;i<generated::signature_count;++i) {
      if(std::strcmp(generated::signatures[i].driver,name)==0) {
        signature=&generated::signatures[i];
        break;
      }
    }
    assert(signature!=nullptr);
    if(std::strcmp(name,"mkradio4a")==0) continue; // converter+TPL below
    const uint8_t ci=std::strcmp(name,"fhkvdataiii")==0 ? 0xA0 :
                     std::strcmp(name,"mkradio4")==0 ? 0xA2 : 0x7A;
    const auto packet=frame(signature->manufacturer,signature->version,
                            signature->device_type,ci);
    const auto candidate=recognize(packet);
    assert(candidate.state==diag::State::Candidate);
    assert(candidate.source==diag::Source::Dll);
    assert(candidate.driver && std::strcmp(candidate.driver,name)==0);
    assert(candidate.decimal_bcd && candidate.meter_id==12345678);
    ++recognized;
  }
  // MK Radio4a requires the actual nested meter identity, not just
  // a Techem converter MVT. Both documented warm/cold cases are included.
  auto converter=frame(0x5068,0x95,0x37,0x72);
  converter[11]=0x25;converter[12]=0x38;
  converter[13]=0x95;converter[14]=0x66;
  converter[15]=0x24;converter[16]=0x23;
  converter[17]=0xFE;converter[18]=0x06; // HYD warm
  auto candidate=recognize(converter);
  assert(candidate.state==diag::State::Candidate);
  assert(candidate.source==diag::Source::Tpl);
  assert(candidate.driver && std::strcmp(candidate.driver,"mkradio4a")==0);
  assert(candidate.meter_id==66953825 && candidate.different_dll_id);
  ++recognized;
  // A bare HYD signature (or unrelated DLL gateway) does not prove
  // the actual MK Radio4a converter + nested-TPL profile.
  assert(recognize(frame(0x2324,0xFE,0x06,0x7A)).state==diag::State::Unknown);
  auto bad_gateway=converter;
  bad_gateway[9]=0x72;
  assert(recognize(bad_gateway).state==diag::State::Unknown);
  auto cold=converter;
  cold[11]=0x02;cold[12]=0x00;
  cold[13]=0x77;cold[14]=0x01;
  cold[15]=0xA8;cold[16]=0x5C;
  cold[17]=0xFE;cold[18]=0x07; // WEH cold (upstream fixture)
  candidate=recognize(cold);
  assert(candidate.state==diag::State::Candidate);
  assert(candidate.source==diag::Source::Tpl);
  assert(candidate.meter_id==1770002 && candidate.different_dll_id);

  assert(recognized==38);
  assert(recognize(frame(0x5068,0x95,0x37,0x7A)).state==diag::State::Unknown);
  cold[17]=0xF0; // unrecognized TPL version, no converter-only promotion
  assert(recognize(cold).state==diag::State::Unknown);
  // A generic DLL MVT must not eclipse a different nested TPL
  // identity: radio-converter metadata is not the target meter model.
  auto nested_other=frame(0x5068,0x95,0x62,0x72);
  nested_other[11]=0x25;nested_other[12]=0x38;
  nested_other[13]=0x95;nested_other[14]=0x66;
  nested_other[15]=0x24;nested_other[16]=0x23;
  nested_other[17]=0xFE;nested_other[18]=0x06;
  assert(recognize(nested_other).state==diag::State::Unknown);
  auto unknown=frame(0x5068,0x95,0x38,0x7A);
  assert(recognize(unknown).state==diag::State::Unknown);
  const DriverSignature colliding[]={
    {0x5068,0x95,0x62,"mkradio4"},
    {0x5068,0x95,0x62,"unrelated_driver"}
  };
  assert(recognize(frame(0x5068,0x95,0x62,0xA2),colliding,2).state==
         diag::State::Ambiguous);
  auto invalid=converter;
  invalid.pop_back();
  assert(recognize(invalid).state==diag::State::Invalid);
  assert(passive::inspect(nullptr,0,generated::signatures,
                          generated::signature_count).state==diag::State::Invalid);
  std::puts("PASS passive native AUTO: all 38 XMQ driver families, MK warm/cold "
            "nested identity and strict converter/ambiguity negatives; "
            "no AES or HA publication.");
}
