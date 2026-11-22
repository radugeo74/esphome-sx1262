#include "xmq_shadow_memory_gate.h"
#include <iostream>
#include <stdexcept>
using namespace xmq_runtime;
int main() {
 const auto need=[](bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);};
 try {
  need(shadow_memory_allows_decode(160852,112640),"typical reported heap rejected");
  need(!shadow_memory_allows_decode(120000,112000),"total free floor bypassed");
  need(!shadow_memory_allows_decode(160852,60000),"fragmentation contiguous floor bypassed");
  need(!shadow_memory_allows_decode(0,0),"empty heap admitted");
  need(!shadow_memory_allows_decode(160852,160853),"invalid largest block > free accepted");
  need(shadow_memory_allows_decode(kShadowMinFreeHeap,kShadowMinLargestBlock),
       "boundary should be inclusive");
  need(!shadow_memory_allows_decode(kShadowMinFreeHeap-1,kShadowMinLargestBlock),
       "below free floor admitted");
  need(!shadow_memory_allows_decode(kShadowMinFreeHeap,kShadowMinLargestBlock-1),
       "below contiguous floor admitted");
  std::cout<<"SHADOW_MEMORY_GATE_OK: heap + contiguous block fail-closed; legacy unaffected\n";
  return 0;
 } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
