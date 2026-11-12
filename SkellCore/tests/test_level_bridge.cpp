#include <SkellCore/LevelBridge.h>
#include <algorithm>
#include <atomic>
#if defined(NDEBUG)
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>
#include <thread>

// Real-time peak bridge regression. No JUCE or plugin host needed.
int main() {
    skellcore::LevelBridge bridge;
    assert(bridge.take() == 0.0f);
    bridge.publish(0.2f);
    bridge.publish(0.85f);
    bridge.publish(0.01f);
    assert(std::abs(bridge.take() - 0.85f) < 0.00001f);
    assert(bridge.take() == 0.0f);

    bridge.publish(-0.7f);
    bridge.publish(std::numeric_limits<float>::quiet_NaN());
    bridge.publish(std::numeric_limits<float>::infinity());
    bridge.publish(-std::numeric_limits<float>::infinity());
    assert(bridge.take() == 0.0f);
    bridge.publish(3.0f);
    assert(bridge.take() == 1.0f);

    // Stress the two intended participants without relying on event timing:
    // writer repeatedly publishes finite peaks; reader drains in parallel.
    std::atomic<bool> done {false};
    std::atomic<int> count {0};
    std::thread writer([&] {
        for (int i = 0; i < 200000; ++i) {
            bridge.publish(float(i % 101) / 100.0f);
        }
        done.store(true, std::memory_order_release);
    });
    while (!done.load(std::memory_order_acquire)) {
        const float observed = bridge.take();
        assert(std::isfinite(observed));
        assert(observed >= 0.0f && observed <= 1.0f);
        count.fetch_add(1, std::memory_order_relaxed);
    }
    writer.join();
    const float tail = bridge.take();
    assert(std::isfinite(tail) && tail >= 0.0f && tail <= 1.0f);
    (void)count; // diagnostic iteration counter; not timing-dependent
    return 0;
}
