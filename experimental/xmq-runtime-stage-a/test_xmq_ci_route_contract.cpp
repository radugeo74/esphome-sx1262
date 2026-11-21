#include "xmq_ci_route_contract.h"
#include <iostream>
#include <stdexcept>
using namespace xmq_runtime;
static void check(bool ok,const char *why) {
  if(!ok) throw std::runtime_error(why);
}
int main() {
 try {
  using R=XmqTelegramRoute;
  using E=ApplicationEncoding;
  for(auto ci : {0xA0,0xA2}) {
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,true,true,false)==
          R::LegacyVerifiedProprietary,"original CI legacy verified route rejected");
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,false,true,true)==
          R::Reject,"proprietary CI falsely admitted as direct standard TPL5");
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,false,true,false)==
          R::Reject,"unverified proprietary application accepted");
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,true,false,false)==
          R::Reject,"proprietary CRC absence accepted");
    check(classify_xmq_telegram_route(ci,E::StandardDv,true,true,false)==
          R::Reject,"proprietary CI accepted standard encoding");
  }
  for(auto ci : {0x72,0x7A}) {
    check(classify_xmq_telegram_route(ci,E::StandardDv,true,true,false)==
          R::LegacyVerifiedStandard,"OMS verified legacy path rejected");
    check(classify_xmq_telegram_route(ci,E::StandardDv,false,true,true)==
          R::DirectTpl5StandardCandidate,"direct standard-only candidate rejected");
    check(classify_xmq_telegram_route(ci,E::StandardDv,false,false,true)==
          R::Reject,"direct path without CRC accepted");
    check(classify_xmq_telegram_route(ci,E::StandardDv,false,true,false)==
          R::Reject,"unknown OMS plaintext provenance accepted");
    check(classify_xmq_telegram_route(ci,E::StandardDv,true,true,true)==
          R::Reject,"ambiguous competing source accepted");
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,true,true,false)==
          R::Reject,"OMS CI mislabeled as proprietary IXML accepted");
  }
  for(auto ci : {0x00,0x8C,0xA1,0xB7}) {
    check(classify_xmq_telegram_route(ci,E::OriginalIxml,true,true,false)==
          R::Reject,"unsupported CI allowed proprietary route");
    check(classify_xmq_telegram_route(ci,E::StandardDv,true,true,false)==
          R::Reject,"unsupported CI allowed standard route");
  }
  std::cout<<"XMQ_CI_ROUTE_OK: proprietary/standard, origin, CRC, ambiguity\n";
  return 0;
 } catch(const std::exception &e) {
  std::cerr<<"FAIL: "<<e.what()<<'\n';
  return 1;
 }
}
