#include "xmq_telegram_capture.h"
#include "xmq_shadow_diagnostic.h"
#include <stdexcept>
#include <iostream>

struct AddressMock {
  unsigned mfct=0;
  uint8_t version=0;
  uint8_t type=0;
};

struct TelegramMock {
  std::vector<uint8_t> frame;
  int header_size=0;
  int suffix_size=0;
  int tpl_ci=0;
  bool decryption_failed=false;
  bool discard=false;
  std::vector<AddressMock> addresses;
};

static void check(bool ok,const char *msg) {
  if (!ok) throw std::runtime_error(msg);
}

static TelegramMock telegram(const std::vector<uint8_t> &body,
                             int ci,unsigned mfct=0x5068,
                             uint8_t version=0x94,uint8_t type=0x80) {
  TelegramMock t;
  t.frame={0xBB,0xCC,0xDD};
  t.frame.insert(t.frame.end(),body.begin(),body.end());
  t.frame.push_back(0xEE); // security suffix: must not reach parser
  t.header_size=3; t.suffix_size=1;
  t.tpl_ci=ci;
  // Radio converter's outer address has another manufacturer, inner is last.
  t.addresses={{0x4CAE,0x38,0x37},{mfct,version,type}};
  return t;
}

int main() {
  using namespace xmq_runtime;
  try {
    check(numeric_mfct_matches("TCH",0x5068),"TCH numeric M-field");
    check(numeric_mfct_matches("SEN",0x4CAE),"SEN numeric M-field");
    check(numeric_mfct_matches("HYD",0x2324),"HYD numeric M-field");
    check(manufacturer_code_to_flag(0x5068)=="TCH","manufacturer inverse");
    check(manufacturer_code_to_flag(0)=="","invalid manufacturer code");
    check(!encoding_matches_ci(ApplicationEncoding::StandardDv,0xA2),
          "proprietary format classified as OMS standard");

    VerifiedApplication out;
    auto hca=telegram(parse_hex(
      "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"),
      0xA2);
    check(capture_verified_payload(hca,true,ApplicationEncoding::OriginalIxml,out),
          "valid successful original frame refused");
    check(out.inner_mvt.manufacturer=="TCH" &&
          out.inner_mvt.version==0x94 && out.inner_mvt.type==0x80,
          "must use inner address instead of converter DLL");
    check(out.payload.size()==41 && out.payload.front()==0x0F &&
          out.payload.back()==0x0E,"wrong payload size/header/suffix stripped");

    auto standard=telegram(parse_hex(
      "036E980000436E440400426C3F3C0F0F0AE388E05EADCD9772"),0x7A);
    check(capture_verified_payload(standard,true,ApplicationEncoding::StandardDv,out),
          "valid OMS TPL-CI refused");
    check(out.payload.front()==0x03 && out.payload.back()==0x72,
          "standard frame extraction mismatch");

    auto denied=[&](const TelegramMock &t,bool decoded,ApplicationEncoding format) {
      VerifiedApplication ignored;
      const bool accepted=capture_verified_payload(t,decoded,format,ignored);
      return !accepted && ignored.payload.empty() &&
             ignored.inner_mvt.manufacturer.empty();
    };
    check(denied(hca,false,ApplicationEncoding::OriginalIxml),
          "id_match must not count as decoder success");
    auto wrong=hca;
    wrong.decryption_failed=true;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "encrypted/decryption-failed data reached XMQ");
    wrong=hca;wrong.discard=true;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "discarded telegram reached XMQ");
    wrong=hca;wrong.header_size=999;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "overflow header accepted");
    wrong=hca;wrong.header_size=-1;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "negative header accepted");
    wrong=hca;wrong.suffix_size=999;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "oversized suffix accepted");
    wrong=hca;wrong.suffix_size=-1;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "negative suffix accepted");
    wrong=hca;wrong.addresses.clear();
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "missing inner address accepted");
    wrong=hca;wrong.addresses.back().mfct=0;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "invalid inner manufacturer accepted");
    check(denied(hca,true,ApplicationEncoding::StandardDv),
          "TPL-CI mismatch accepted");
    wrong=hca;wrong.tpl_ci=0;
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "unknown CI accepted");
    wrong=hca;wrong.frame.resize(4);
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "empty application accepted");
    wrong=hca;wrong.frame.resize(3);wrong.frame.insert(wrong.frame.end(),1100,0x00);
    wrong.frame.push_back(0xEE);
    check(denied(wrong,true,ApplicationEncoding::OriginalIxml),
          "oversized plaintext accepted");

    std::cout<<"TELEGRAM_CAPTURE_OK post-decode bounds, inner MVT, "
                "verified CI and fail-closed plaintext extraction\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
