// Non-gating relative performance measurements. No arbitrary "fast enough"
// claims: timings are hardware, compiler and load dependent.
#include <SkellCore/LevelBridge.h>
#include <SkellCore/MeterBallistics.h>
#include <chrono>
#include <cmath>
#include <cstdio>

int main() {
    using Clock = std::chrono::steady_clock;
    constexpr int n = 1000000;
    volatile float sink = 0.f;

    skellcore::LevelBridge bridge;
    const auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
        bridge.publish(float(i % 101) / 100.f);
        if ((i & 15) == 0) sink += bridge.take();
    }
    const auto t1 = Clock::now();

    skellcore::MeterBallistics meter;
    const auto t2 = Clock::now();
    for (int i = 0; i < n; ++i) {
        const float sample = float(i % 101) / 100.f;
        sink += meter.advance(sample, 1.f / 60.f).level;
    }
    const auto t3 = Clock::now();

    const auto bridgeNs = std::chrono::duration<double, std::nano>(t1-t0).count()/n;
    const auto meterNs = std::chrono::duration<double, std::nano>(t3-t2).count()/n;
    if (!std::isfinite(bridgeNs) || !std::isfinite(meterNs)
        || bridgeNs <= 0. || meterNs <= 0. || !std::isfinite(sink))
        return 1;
    std::printf("SkellCore UI-metric CPU baseline, iterations=%d\n", n);
    std::printf("LevelBridge publish/drain pattern: %.2f ns/iteration\n", bridgeNs);
    std::printf("MeterBallistics advance: %.2f ns/iteration\n", meterNs);
    std::puts("Informational only; never use this timing as a real-time audio deadline.");
    return 0;
}
