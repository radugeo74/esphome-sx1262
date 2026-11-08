// Real XMQ detect.mvt signatures, read-only batch-2 recognition on
// structurally valid, CRC-stripped headers. Each production caller must
// independently verify DLL CRC BEFORE calling the recognizer.
// No AES, application parsing, sensor registration or publication.
#include "components/wmbus_native_sx126x/batch2_auto_identify.h"
#include "xmq_auto_registry.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstring>

using namespace wmbus_pure;
namespace diag=batch2_identify;

static std::vector<uint8_t> frame(uint16_t manufacturer,uint8_t version,
                                  uint8_t device_type,uint8_t ci) {
  const size_t length=ci==0x72 ? 23 : (ci==0x8D ? 21 : 15);
  std::vector<uint8_t> f(length,0);
  f[0]=static_cast<uint8_t>(length-1);
  f[1]=0x44;
  f[2]=static_cast<uint8_t>(manufacturer);
  f[3]=static_cast<uint8_t>(manufacturer>>8);
  f[4]=0x78;f[5]=0x56;f[6]=0x34;f[7]=0x12;
  f[8]=version;f[9]=device_type;f[10]=ci;
  return f;
}

static diag::Result identify(const std::vector<uint8_t> &f) {
  return diag::inspect(f.data(),f.size(),generated::signatures,
                       generated::signature_count);
}

int main() {
  using State=diag::State;
  using Source=diag::Source;
  // Minomess directly on its own DLL address.
  auto minomess=frame(0x6A49,0x01,0x06,0x72);
  minomess[11]=0x78;minomess[12]=0x56;
  minomess[13]=0x34;minomess[14]=0x12;
  minomess[15]=0x49;minomess[16]=0x6A;
  minomess[17]=0x01;minomess[18]=0x06;
  auto got=identify(minomess);
  assert(got.state==State::Candidate && got.source==Source::Tpl);
  assert(std::strcmp(got.driver,"minomess")==0);
  assert(got.decimal_bcd && got.meter_id==12345678);
  assert(!got.different_dll_id);
  // Converter DLL reports Techem: must look into long TPL for actual
  // Minomess. The radio ID is explicitly different from the meter ID.
  auto converter=minomess;
  converter[2]=0x68;converter[3]=0x50; // TEH radio converter
  converter[4]=0x21;converter[5]=0x43;
  converter[6]=0x65;converter[7]=0x87;
  converter[8]=0x95;converter[9]=0x72;
  got=identify(converter);
  assert(got.state==State::Candidate && got.source==Source::Tpl);
  assert(std::strcmp(got.driver,"minomess")==0);
  assert(got.decimal_bcd && got.meter_id==12345678);
  assert(got.different_dll_id);
  // Two non-batch2 radio-converter MVTs may overlap. The nested
  // Minomess identity still wins because there is NO batch2 collision.
  const DriverSignature converter_overlap[]={
    {0x5068,0x95,0x72,"unrelated_a"},
    {0x5068,0x95,0x72,"unrelated_b"},
    {0x6A49,0x01,0x06,"minomess"}};
  const auto still_nested=diag::inspect(
      converter.data(),converter.size(),converter_overlap,3);
  assert(still_nested.state==State::Candidate &&
         still_nested.source==Source::Tpl &&
         std::strcmp(still_nested.driver,"minomess")==0);
  // An unknown nested TPL must not become a guessed Minomess decoder.
  converter[15]=0x68;converter[16]=0x50;
  got=identify(converter);
  assert(got.state==State::Unknown);
  // Kamwater DLL: all original KAM/KAW signatures are registered.
  const DriverSignature kam_variants[]={
    {0x2C2D,0x1B,0x06,"kamwater"},{0x2C2D,0x1B,0x16,"kamwater"},
    {0x2C2D,0x1D,0x16,"kamwater"},{0x2C37,0x3A,0x16,"kamwater"},
    {0x2C37,0x3C,0x06,"kamwater"},{0x2C37,0x3C,0x16,"kamwater"},
    {0x2C37,0x42,0x06,"kamwater"},{0x2C37,0x42,0x16,"kamwater"}
  };
  for(const auto &v:kam_variants) {
    auto f=frame(v.manufacturer,v.version,v.device_type,0x8D);
    got=identify(f);
    assert(got.state==State::Candidate && got.source==Source::Dll);
    assert(std::strcmp(got.driver,"kamwater")==0);
    assert(got.meter_id==12345678);
  }
  // APA versions and device types from the upstream Apator162 detector.
  for(uint8_t type:{uint8_t(0x06),uint8_t(0x07)}) {
    got=identify(frame(0x0601,0x05,type,0x7A));
    assert(got.state==State::Candidate && got.source==Source::Dll);
    assert(std::strcmp(got.driver,"apator162")==0);
  }
  // Lansen recognized passively on verified yet unconfigured T1/C1.
  const DriverSignature lansen_variants[]={
    {0x3033,0x14,0x00,"lansenpu"},{0x3033,0x1B,0x00,"lansenpu"},
    {0x3033,0x0B,0x02,"lansenpu"}};
  for(const auto &v:lansen_variants) {
    got=identify(frame(v.manufacturer,v.version,v.device_type,0x7A));
    assert(got.state==State::Candidate && got.source==Source::Dll);
    assert(std::strcmp(got.driver,"lansenpu")==0);
  }
  const DriverSignature lansen_collision[]={
    {0x3033,0x14,0x00,"lansenpu"},
    {0x3033,0x14,0x00,"unrelated"}};
  auto colliding_lansen=frame(0x3033,0x14,0x00,0x7A);
  assert(diag::inspect(colliding_lansen.data(),colliding_lansen.size(),
                       lansen_collision,2).state==State::Ambiguous);
  // ELL address overrides an unrelated converter DLL, but never
  // treats the ELL application or CRC as already decoded.
  auto ell=frame(0x5068,0x95,0x72,0x8D);
  ell[10]=0x8E;
  ell[0]=0x14;
  ell[13]=0x37;ell[14]=0x2C;
  ell[15]=0x78;ell[16]=0x56;
  ell[17]=0x34;ell[18]=0x12;
  ell[19]=0x3C;ell[20]=0x16;
  got=identify(ell);
  assert(got.state==State::Candidate && got.source==Source::Ell);
  assert(std::strcmp(got.driver,"kamwater")==0);
  // Two different original batch-2 models in DLL/TPL is ambiguous.
  auto conflicting=frame(0x2C2D,0x1B,0x16,0x72);
  conflicting[11]=0x78;conflicting[12]=0x56;
  conflicting[13]=0x34;conflicting[14]=0x12;
  conflicting[15]=0x01;conflicting[16]=0x06;
  conflicting[17]=0x05;conflicting[18]=0x06;
  assert(identify(conflicting).state==State::Ambiguous);
  // Topas ESKR: BOTH original AMT/F1/06 and F1/07 variants are
  // identified from DLL metadata, with no key and no application decode.
  for (uint8_t t : {uint8_t{0x06}, uint8_t{0x07}}) {
    auto topas=frame(0x05B4,0xF1,t,0x7A);
    got=identify(topas);
    assert(got.state==State::Candidate && got.source==Source::Dll);
    assert(std::strcmp(got.driver,"topaseskr")==0);
    assert(got.decimal_bcd && got.meter_id==12345678);
    assert(!got.different_dll_id);
  }
  // Original MK Radio4a warm-water fixture: TCH/95/37 is only the
  // radio converter, nested HYD/FE/06 is the meter behind it.
  auto mk_warm=frame(0x5068,0x95,0x37,0x72);
  mk_warm[4]=0x36;mk_warm[5]=0x49;mk_warm[6]=0x46;mk_warm[7]=0x00;
  mk_warm[11]=0x25;mk_warm[12]=0x38;
  mk_warm[13]=0x95;mk_warm[14]=0x66;
  mk_warm[15]=0x24;mk_warm[16]=0x23;
  mk_warm[17]=0xFE;mk_warm[18]=0x06;
  got=identify(mk_warm);
  assert(got.state==State::Candidate && got.source==Source::Tpl);
  assert(std::strcmp(got.driver,"mkradio4a")==0);
  assert(got.decimal_bcd && got.meter_id==66953825);
  assert(got.different_dll_id);
  // Original cold-water fixture has WEH/FE/07 in TPL, absent from the
  // upstream detect.mvt declarations. Admit ONLY this exact nested pair.
  auto mk_cold=mk_warm;
  mk_cold[11]=0x02;mk_cold[12]=0x00;
  mk_cold[13]=0x77;mk_cold[14]=0x01;
  mk_cold[15]=0xA8;mk_cold[16]=0x5C;
  mk_cold[17]=0xFE;mk_cold[18]=0x07;
  got=identify(mk_cold);
  assert(got.state==State::Candidate && got.source==Source::Tpl);
  assert(std::strcmp(got.driver,"mkradio4a")==0);
  assert(got.meter_id==1770002 && got.different_dll_id);
  // A converter by itself, or one with an unrecognized nested meter,
  // cannot be promoted to a verified mkradio4a device.
  assert(identify(frame(0x5068,0x95,0x37,0x7A)).state==State::Unknown);
  auto mk_unknown=mk_cold;
  mk_unknown[17]=0xFD; // not WEH/FE/07
  assert(identify(mk_unknown).state==State::Unknown);
  // A converter advertising a *different supported* nested driver is
  // ambiguous; it must not be silently called MK Radio4a.
  auto mk_conflict=mk_warm;
  mk_conflict[15]=0xB4;mk_conflict[16]=0x05;
  mk_conflict[17]=0xF1;mk_conflict[18]=0x06;
  assert(identify(mk_conflict).state==State::Ambiguous);
  // MVT overlap on a new secure-only model must fail closed.
  constexpr DriverSignature topas_collision[]={
    {0x05B4,0xF1,0x06,"topaseskr"},
    {0x05B4,0xF1,0x06,"other_driver"}};
  auto topas_ambiguous=frame(0x05B4,0xF1,0x06,0x7A);
  assert(diag::inspect(topas_ambiguous.data(),topas_ambiguous.size(),
                       topas_collision,2).state==State::Ambiguous);
  // Original Techem never masquerades as an experimental model.
  assert(identify(frame(0x5068,0x95,0x72,0xA2)).state==State::Unknown);
  auto truncated=minomess;truncated.pop_back();
  assert(identify(truncated).state==State::Invalid);
  auto unsupported=frame(0x2C2D,0x1B,0x16,0xA2);
  assert(identify(unsupported).state==State::Invalid ||
         identify(unsupported).state==State::Candidate);
  // Recognize only MVT identities; collision with a different
  // upstream driver is NOT automatically a batch-2 selection.
  const DriverSignature overlap[]={
      {0x2C2D,0x1B,0x16,"kamwater"},
      {0x2C2D,0x1B,0x16,"another_driver"}};
  auto colliding=frame(0x2C2D,0x1B,0x16,0x8D);
  assert(diag::inspect(colliding.data(),colliding.size(),
                       overlap,2).state==State::Ambiguous);
  assert(diag::inspect(nullptr,0,generated::signatures,
                       generated::signature_count).state==State::Invalid);
  std::cout<<"PASS metadata-only native AUTO: Topas F1/06/07, MK Radio4a "
            <<"warm HYD/cold WEH converter-TPL, conflicts, ELL and bounds; "
            <<"no AES/decoder/publication.\\n";
  return 0;
}
