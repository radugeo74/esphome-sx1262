// Host-only ownership model for the Radio queue contract.
// This tests RAII transfer and cleanup, not the ESP-IDF queue implementation.
#include <cassert>
#include <cstddef>
#include <memory>
#include <queue>

struct TrackedPacket {
  static int alive;
  static int destroyed;
  TrackedPacket() { ++alive; }
  ~TrackedPacket() { --alive; ++destroyed; }
  TrackedPacket(const TrackedPacket&) = delete;
  TrackedPacket& operator=(const TrackedPacket&) = delete;
};
int TrackedPacket::alive = 0;
int TrackedPacket::destroyed = 0;

struct QueueModel {
  std::queue<TrackedPacket*> pending;
  const size_t limit = 3;
  ~QueueModel() {
    while (!pending.empty()) {
      delete pending.front();
      pending.pop();
    }
  }
  bool submit(std::unique_ptr<TrackedPacket> packet) {
    if (pending.size() == limit) return false;
    TrackedPacket* raw = packet.get();
    pending.push(raw);
    packet.release();
    return true;
  }
  bool consume(bool valid_frame) {
    if (pending.empty()) return false;
    std::unique_ptr<TrackedPacket> packet(pending.front());
    pending.pop();
    return valid_frame;
  }
};

int main() {
  {
    QueueModel queue;
    assert(queue.submit(std::make_unique<TrackedPacket>()));
    assert(queue.submit(std::make_unique<TrackedPacket>()));
    assert(queue.submit(std::make_unique<TrackedPacket>()));
    assert(TrackedPacket::alive == 3);
    const int destroyed_before_drop = TrackedPacket::destroyed;
    assert(!queue.submit(std::make_unique<TrackedPacket>()));
    assert(TrackedPacket::destroyed == destroyed_before_drop + 1);
    assert(TrackedPacket::alive == 3);
    // A failed frame conversion still frees its packet.
    assert(!queue.consume(false));
    assert(TrackedPacket::alive == 2);
    assert(queue.consume(true));
    assert(TrackedPacket::alive == 1);
  }
  // Destroying a queue also frees packets that were never consumed.
  assert(TrackedPacket::alive == 0);
  assert(TrackedPacket::destroyed == 4);
  return 0;
}
