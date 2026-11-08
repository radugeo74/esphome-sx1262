// Audit 6: upstream NOKEY data is never proof of clear on-air RF.
// Test the *actual* TPL security gate and configured pure decoder with
// selected unmodified PUBLIC XMQ fixtures. Concatenating '_' means these
// are test artefacts, NOT a reconstructed original radio capture.
#include "components/wmbus_native_sx126x/tpl_security_probe.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<uint8_t> from_xmq(const char *src) {
  std::vector<uint8_t> out;
  int high = -1;
  for (const char *p=src;*p;++p) {
    if (*p == '_' || *p == ' ' || *p == '\n') continue;
    const int d=(*p>='0'&&*p<='9')?*p-'0':
      (*p>='A'&&*p<='F')?*p-'A'+10:
      (*p>='a'&&*p<='f')?*p-'a'+10:-1;
    assert(d >= 0);
    if (high < 0) high=d;
    else { out.push_back(static_cast<uint8_t>(high*16+d)); high=-1; }
  }
  assert(high<0);
  return out;
}

int main() {
  using wmbus_pure::tpl_security::State;
  using wmbus_pure::publication::Decision;
  using xmq_portable::PureDecodeStatus;
  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload out{values,24,0,dates,2,0};
  auto plain = [&](const std::vector<uint8_t> &frame,
                   const char *id,const char *dll,const char *driver) {
    return xmq_portable::batch2_plain::decode_configured(
        frame.data(),frame.size(),id,dll,driver,captures,12,out);
  };
  const auto mode5 = [&](const std::vector<uint8_t> &frame) {
    assert(!frame.empty() && frame.size()==static_cast<size_t>(frame[0])+1);
    const auto s=wmbus_pure::tpl_security::inspect(frame.data(),frame.size());
    assert(s.has_config_word && s.mode==5 && s.state==State::Encrypted);
  };

  // Official Q400 XMQ case 1: NOKEY + clear records supplied after '_'
  // while RF header STILL declares 2 AES-CBC Mode-5 blocks.
  const auto q400=from_xmq(
    "2E4409077272727210077AD71020052F2F_"
    "046D040D742C041377000000446D0000612C4413000000002F2F2F2F2F2F");
  mode5(q400);
  assert(q400[15]==0x2F && q400[16]==0x2F);
  assert(wmbus_pure::publication::inspect(q400.data(),q400.size(),
                                         72727272)==Decision::UnverifiedSecurity);
  assert(plain(q400,"72727272","72727272","q400")!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);

  // Official Topas XMQ: same problem; no secure-only plaintext admission.
  const auto topas=from_xmq(
    "4E44B40512345678F1077A310040052F2F_"
    "01FD08040C13991848004C1359423500CC101300000000CC201359423500"
    "426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600");
  mode5(topas);
  assert(wmbus_pure::publication::inspect(topas.data(),topas.size(),
                                         78563412)==Decision::UnverifiedSecurity);
  assert(!xmq_portable::batch2_plain::is_explicit_driver("topaseskr"));
  assert(plain(topas,"78563412","78563412","topaseskr")!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);

  // Official MK Radio4a: TCH DLL radio gateway + separate HYD TPL meter.
  const auto mk=from_xmq(
    "4B44685036494600953772253895662423FE064E0030052F2F_"
    "4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390"
    "FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9");
  mode5(mk);
  assert(wmbus_pure::publication::inspect(mk.data(),mk.size(),
                                         464936)==Decision::ConflictingIdentity);
  assert(!xmq_portable::batch2_plain::is_explicit_driver("mkradio4a"));
  assert(plain(mk,"66953825","00464936","mkradio4a")!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);

  // Original Minomess upstream NOKEY after '_' is Mode-5 long TPL.
  const auto mino=from_xmq(
    "6644496A1064035514377251345015496A0007EE0050052F2F_0C1359000000026CBE2B8"
    "2046CA12B8C0413FFFFFFFF8D0493132CFBFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF02FD1700002F2F");
  mode5(mino);
  assert(plain(mino,"15503451","55036410","minomess")!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);

  // iPERL is a portable compiled driver on the generic native path,
  // not in the dedicated batch-2 plain dispatcher. Exercise the same
  // decode_configured_plain_frame() function used for iPERL in T1/C1.
  auto iperl_plain = [&](const std::vector<uint8_t> &frame,const char *id) {
    return xmq_portable::decode_configured_plain_frame(
        frame.data(),frame.size(),id,"iperl",captures,12,out);
  };
  // In contrast, the genuine Mode-0 N=0 iPERL fixture is valid
  // as clear test data; we must not ban legitimate keyless profiles.
  const auto iperl_clear=from_xmq(
    "1844AE4C4455223368077A55000000_041389E20100023B0000");
  const auto clear=wmbus_pure::tpl_security::inspect(
      iperl_clear.data(),iperl_clear.size());
  assert(clear.state==State::NotEncrypted && clear.mode==0);
  assert(wmbus_pure::publication::inspect(iperl_clear.data(),
          iperl_clear.size(),33225544)==Decision::Allowed);
  assert(iperl_plain(iperl_clear,"33225544")==PureDecodeStatus::Ok);
  assert(out.value_count>0);

  // Another iPERL NOKEY example has M=0 but N=1; never publish.
  const auto iperl_inconsistent=from_xmq(
    "1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F");
  const auto inconsistent=wmbus_pure::tpl_security::inspect(
      iperl_inconsistent.data(),iperl_inconsistent.size());
  assert(inconsistent.has_config_word && inconsistent.mode==0 &&
         inconsistent.state==State::Unknown);
  assert(wmbus_pure::publication::inspect(
      iperl_inconsistent.data(),iperl_inconsistent.size(),
      12345699)==Decision::UnverifiedSecurity);
  assert(iperl_plain(iperl_inconsistent,"12345699")!=
         PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);
  std::puts("PASS NOKEY transport: Q400/Topas/MK/Minomess Mode5 refused; "
            "iPERL valid Mode0 allowed, invalid Mode0+N rejected; 2F2F "
            "never overrides security; no AES trial or HA publish.");
}
