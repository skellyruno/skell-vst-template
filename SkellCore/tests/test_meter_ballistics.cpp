#include <SkellCore/MeterBallistics.h>
#include <cmath>
#include <cstdio>
#include <limits>

static bool near(float a, float b, float tolerance = 1e-4f) {
    return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= tolerance;
}
static bool check(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "MeterBallistics: %s\n", message);
    return condition;
}

int main() {
    skellcore::MeterBallistics meter;
    meter.reset();
    if (!check(near(meter.reading().level, 0.f), "reset level")
        || !check(near(meter.reading().peak, 0.f), "reset peak")) return 1;

    auto reading = meter.advance(.75f, 1.f/60.f);
    if (!check(near(reading.level, .75f), "attack is not immediate")
        || !check(near(reading.peak, .75f), "peak missed an attack")) return 1;
    reading = meter.advance(0.f, .1f);
    if (!check(reading.level > 0.f && reading.level < .75f, "release failed")
        || !check(near(reading.peak, .75f), "peak did not hold")) return 1;

    for (int i = 0; i < 15; ++i) reading = meter.advance(0.f, .1f);
    if (!check(reading.level >= 0.f && reading.level < .002f, "level did not settle")
        || !check(reading.peak >= reading.level && reading.peak < .002f,
                  "peak did not settle")) return 1;

    meter.reset();
    reading = meter.advance(4.f, 0.f);
    if (!check(near(reading.level, 1.f) && near(reading.peak, 1.f),
               "out-of-range positive input not bounded")) return 1;
    reading = meter.advance(std::numeric_limits<float>::quiet_NaN(), 1.f/60.f);
    if (!check(std::isfinite(reading.level) && std::isfinite(reading.peak),
               "NaN corrupted state")) return 1;
    reading = meter.advance(std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::infinity());
    if (!check(std::isfinite(reading.level) && std::isfinite(reading.peak),
               "Infinity corrupted state")) return 1;
    reading = meter.advance(-17.f, -1.f);
    if (!check(reading.level >= 0.f && reading.level <= 1.f
               && reading.peak >= reading.level && reading.peak <= 1.f,
               "negative values damaged bounds")) return 1;

    meter.configure(0.f, std::numeric_limits<float>::quiet_NaN());
    meter.reset();
    reading = meter.advance(.4f, .2f);
    reading = meter.advance(0.f, .2f);
    if (!check(std::isfinite(reading.level) && std::isfinite(reading.peak),
               "invalid configuration damaged state")) return 1;

    // Nominal 24Hz/60Hz GUI updates should converge to the same envelope
    // within roundoff for an immediate attack followed by a one-second decay.
    skellcore::MeterBallistics slow, fast;
    slow.configure(.3f, 0.f);
    fast.configure(.3f, 0.f);
    slow.advance(1.f, 0.f); fast.advance(1.f, 0.f);
    for (int i = 0; i < 24; ++i) slow.advance(0.f, 1.f/24.f);
    for (int i = 0; i < 60; ++i) fast.advance(0.f, 1.f/60.f);
    if (!check(near(slow.reading().level, fast.reading().level, .002f),
               "GUI refresh cadence changed meter release")) return 1;

    std::puts("SkellCore meter ballistics: passed");
    return 0;
}
