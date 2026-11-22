#pragma once
// Read-only XMQ shadow preflight; never controls wM-Bus legacy decoding.
// Require both aggregate free 8-bit heap and a contiguous 8-bit allocation
// budget before any optional XMQ parser/registry work.
#include <cstddef>
namespace xmq_runtime {
inline constexpr std::size_t kShadowMinFreeHeap = 128u * 1024u;
inline constexpr std::size_t kShadowMinLargestBlock = 80u * 1024u;
inline bool shadow_memory_allows_decode(std::size_t free_heap,
                                        std::size_t largest_free_block) noexcept {
  return free_heap >= kShadowMinFreeHeap &&
         largest_free_block >= kShadowMinLargestBlock &&
         largest_free_block <= free_heap;
}
}  // namespace xmq_runtime
