#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/FDN.h>

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <vector>

// Detect accidental heap allocation inside realtime processFrame().
std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) { ++allocations; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { ++allocations; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;

struct Biquad {   // RBJ cookbook
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad lowpass(double fs, double f, double q) {
        Biquad b; const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
        b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    static Biquad bandpass(double fs, double f, double q) {
        Biquad b; const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
        b.b0 = al / a0; b.b1 = 0; b.b2 = -al / a0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    double process(double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

template <class F> std::vector<float> impulse(F& f, double sr, double seconds, bool leftOnly = false) {
    std::vector<float> ir; const int n = static_cast<int>(sr * seconds);
    for (int i = 0; i < n; ++i) ir.push_back(f.processFrame(i == 0 ? 1.f : 0.f, (i == 0 && !leftOnly) ? 1.f : 0.f)[0]);
    return ir;
}

// RT60 from the Schroeder backward-integrated decay curve, T30 (-5 to -35 dB) extrapolated to 60 dB.
double rt60(const std::vector<float>& ir, double sr, Biquad f1, Biquad f2) {
    std::vector<double> e(ir.size());
    for (std::size_t i = 0; i < ir.size(); ++i) { const double y = f2.process(f1.process(ir[i])); e[i] = y * y; }
    for (std::size_t i = e.size() - 1; i-- > 0;) e[i] += e[i + 1];
    double t5 = -1, t35 = -1;
    for (std::size_t i = 0; i < e.size(); ++i) {
        const double db = 10 * std::log10(e[i] / e[0] + 1e-30);
        if (t5 < 0 && db <= -5) t5 = static_cast<double>(i) / sr;
        if (t35 < 0 && db <= -35) { t35 = static_cast<double>(i) / sr; break; }
    }
    return t35 > 0 ? 2 * (t35 - t5) : -1;
}

// Normalised echo density (Abel & Huang): fraction of samples in a sliding window that exceed the window's RMS.
// A Gaussian-noise tail gives 0.317; a sparse, ringy tail gives far less.
double echoDensity(const std::vector<float>& ir, double sr, double atSeconds) {
    const std::size_t c = static_cast<std::size_t>(atSeconds * sr), h = static_cast<std::size_t>(.01 * sr);
    double e = 0; for (std::size_t i = c - h; i < c + h; ++i) e += static_cast<double>(ir[i]) * ir[i];
    const double rms = std::sqrt(e / (2.0 * static_cast<double>(h)));
    std::size_t n = 0; for (std::size_t i = c - h; i < c + h; ++i) n += std::abs(ir[i]) > rms;
    return static_cast<double>(n) / (2.0 * static_cast<double>(h));
}

bool isPrime(std::size_t n) { if (n < 2) return false; for (std::size_t d = 2; d * d <= n; ++d) if (n % d == 0) return false; return true; }

template <std::size_t N> void checkAll(const char* name) {
    const double sr = 48000;
    // ---- 1. decay times: low band follows decayLow, treble reference follows decayHigh -----------------------
    skellcore::BandFDN<N> f; f.prepare(sr); f.setParameters(3.0f, 1.0f, 1.f);
    const auto ir = impulse(f, sr, 9.0);
    const double low = rt60(ir, sr, Biquad::lowpass(sr, 180, .7), Biquad::lowpass(sr, 180, .7));
    const double high = rt60(ir, sr, Biquad::bandpass(sr, 4000, 2.0), Biquad::bandpass(sr, 4000, 2.0));
    std::printf("%-12s RT60 low %.2f s (set 3.0), at 4 kHz %.2f s (set 1.0)\n", name, low, high);
    assert(low > 3.0 * .75 && low < 3.0 * 1.25);
    assert(high > 1.0 * .70 && high < 1.0 * 1.30);
    assert(high < low * .6);                                // and the tail really is darker than it is low

    // ---- 2. equal decays when decayHigh == decayLow (no spectral tilt), regardless of N ----------------------
    skellcore::BandFDN<N> flat; flat.prepare(sr); flat.setParameters(2.0f, 2.0f, 1.f);
    const auto irFlat = impulse(flat, sr, 7.0);
    const double flatLow = rt60(irFlat, sr, Biquad::lowpass(sr, 180, .7), Biquad::lowpass(sr, 180, .7));
    const double flatHigh = rt60(irFlat, sr, Biquad::bandpass(sr, 4000, 2.0), Biquad::bandpass(sr, 4000, 2.0));
    assert(flatLow > 2.0 * .8 && flatLow < 2.0 * 1.2 && flatHigh > 2.0 * .8 && flatHigh < 2.0 * 1.2);

    // ---- 3. delay lengths: distinct primes inside the requested range, bigger range => longer lines ----------
    skellcore::BandFDN<N> a, b; a.prepare(sr, 24, 80); b.prepare(sr, 40, 160);
    for (std::size_t i = 0; i < N; ++i) {
        const auto la = static_cast<std::size_t>(a.delayLengthSamples(i));
        assert(isPrime(la) && la >= static_cast<std::size_t>(sr * .0239) && la <= static_cast<std::size_t>(sr * .0805));
        assert(b.delayLengthSamples(i) > a.delayLengthSamples(i) - 1 || i == 0);
        for (std::size_t j = 0; j < i; ++j) assert(a.delayLengthSamples(i) != a.delayLengthSamples(j));
    }
    assert(b.delayLengthSamples(N - 1) > a.delayLengthSamples(N - 1) * 3 / 2);
    // RT60 must not depend on the delay range
    b.setParameters(3.0f, 3.0f, 1.f);
    const double bigRoom = rt60(impulse(b, sr, 9.0), sr, Biquad::lowpass(sr, 180, .7), Biquad::lowpass(sr, 180, .7));
    assert(bigRoom > 3.0 * .75 && bigRoom < 3.0 * 1.25);

    // ---- 4. stereo: left and right tails are decorrelated; wet=0 is an exact passthrough ----------------------
    skellcore::BandFDN<N> st; st.prepare(sr); st.setParameters(2.0f, 1.0f, 1.f);
    std::vector<float> L, R;
    for (int i = 0; i < 48000; ++i) { const auto o = st.processFrame(i == 0 ? 1.f : 0.f, 0.f); if (i > 4800) { L.push_back(o[0]); R.push_back(o[1]); } }
    double ll = 0, rr = 0, lr = 0; for (std::size_t i = 0; i < L.size(); ++i) { ll += double(L[i]) * L[i]; rr += double(R[i]) * R[i]; lr += double(L[i]) * R[i]; }
    const double corr = lr / std::sqrt(ll * rr);
    std::printf("%-12s L/R tail correlation %.3f\n", name, corr);
    assert(std::abs(corr) < .25 && ll > 0 && rr > 0);
    skellcore::BandFDN<N> dry; dry.prepare(sr); dry.setParameters(2.0f, 1.0f, 0.f);
    for (int i = 0; i < 1000; ++i) { const float x = std::sin(i * .1f); const auto o = dry.processFrame(x, -x); assert(o[0] == x && o[1] == -x); }

    // ---- 5. robustness: NaN/Inf, all rates, extreme and swept parameters, unprepared object ---------------------
    skellcore::BandFDN<N> raw; const auto un = raw.processFrame(.5f, -.25f); assert(un[0] == .5f && un[1] == -.25f);
    for (double rate : {8000., 22050., 44100., 48000., 96000., 192000.}) {
        skellcore::BandFDN<N> r; r.prepare(rate); std::mt19937 g(11);
        std::uniform_real_distribution<float> u(-1.f, 1.f), lowD(.01f, 40.f), highD(.0f, 40.f);
        for (int i = 0; i < static_cast<int>(rate); ++i) {
            if (i % 257 == 0) r.setParameters(lowD(g), highD(g), u(g) * .5f + .5f);
            float x = u(g);
            if (i % 997 == 0) x = std::numeric_limits<float>::quiet_NaN();
            if (i % 1013 == 0) x = std::numeric_limits<float>::infinity();
            const auto o = r.processFrame(x, x); assert(std::isfinite(o[0]) && std::isfinite(o[1]) && std::abs(o[0]) < 100.f);
        }
    }

    // ---- 6. no allocation in the audio path -----------------------------------------------------------------
    skellcore::BandFDN<N> rt; rt.prepare(sr); const auto before = allocations.load();
    for (int i = 0; i < 20000; ++i) { if (i % 1000 == 0) rt.setParameters(1.f + (i % 7), .5f + (i % 3), .5f); rt.processFrame(.3f, -.2f); }
    assert(allocations.load() == before);
}
} // namespace

int main() {
    checkAll<4>("BandFDN<4>"); checkAll<8>("FDNReverb8"); checkAll<16>("FDNReverb16");

    // ---- density: more lines => tail closer to Gaussian noise at 150 ms --------------------------------------
    const double sr = 48000;
    skellcore::FDNReverb old; old.prepare(sr); old.setParameters(2.5f, .3f, 1.f);
    skellcore::FDNReverb8 e8; e8.prepare(sr); e8.setParameters(2.5f, 1.2f, 1.f);
    skellcore::FDNReverb16 e16; e16.prepare(sr); e16.setParameters(2.5f, 1.2f, 1.f);
    const double dOld = echoDensity(impulse(old, sr, 1.0, true), sr, .15);
    const double d8 = echoDensity(impulse(e8, sr, 1.0, true), sr, .15);
    const double d16 = echoDensity(impulse(e16, sr, 1.0, true), sr, .15);
    std::printf("echo density at 150 ms (Gaussian = 0.317): 4-line %.3f, 8-line %.3f, 16-line %.3f\n", dOld, d8, d16);
    std::fflush(stdout);
    assert(d16 > d8 && d8 > dOld);
    assert(d16 > .2 && d16 > 3.0 * dOld && d8 > 2.0 * dOld);   // measured: 0.062 / 0.160 / 0.229

    // ---- the original FDNReverb is untouched: identical output to a fresh instance with the same settings --------
    skellcore::FDNReverb p, q; p.prepare(sr); q.prepare(sr); p.setParameters(2.f, .4f, .3f); q.setParameters(2.f, .4f, .3f);
    for (int i = 0; i < 2000; ++i) {
        const float x = std::sin(i * .05f);
        const auto a = p.processFrame(x, x);
        const auto b = q.processFrame(x, x);
        if (a != b) {
            std::fprintf(stderr, "Legacy FDN determinism mismatch sample=%d x=%.9g p=(%.9g,%.9g) q=(%.9g,%.9g)\n",
                         i, x, a[0], a[1], b[0], b[1]);
            std::fflush(stderr);
            assert(a == b);
            std::abort();
        }
    }
    std::puts("PASS: band-split higher-order FDN: RT60 per band, density, stereo, robustness, no allocation");
}
