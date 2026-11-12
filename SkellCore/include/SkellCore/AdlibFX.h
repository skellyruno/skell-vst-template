#pragma once
// Shared JUCE-free C++17 adlib effects for AdlibLab and Skell Preset.
// No allocations, blocking calls or exceptions in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace skellcore::adlibfx {
enum class Type : std::uint8_t { Empty=0, Chop, Motion, Filter, Saturate };
struct Parameters {
    Type type=Type::Empty;
    float amount=0.5f,rate=0.5f,tone=0.5f,mix=1.f,bpm=120.f;
};
struct Stereo {float left=0.f,right=0.f;};
inline float clamp(float v,float low,float high) noexcept {
    return std::isfinite(v)?std::clamp(v,low,high):low;
}
inline float sample(float v) noexcept {
    return std::isfinite(v)?(std::abs(v)<1.e-19f?0.f:std::clamp(v,-8.f,8.f)):0.f;
}
inline float clean(float v) noexcept {return std::abs(v)<1.e-19f?0.f:v;}
class Processor final {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):48000.;
        reset();
    }
    void reset() noexcept {phase=0.;leftLow=rightLow=0.f;}
    Stereo process(Stereo source,Parameters p) noexcept {
        const float l=sample(source.left),r=sample(source.right);
        const float amount=clamp(p.amount,0.f,1.f);
        const float rate=clamp(p.rate,0.f,1.f);
        const float tone=clamp(p.tone,0.f,1.f);
        const float mix=clamp(p.mix,0.f,1.f);
        const double bpm=(double)clamp(p.bpm,20.f,400.f);
        Stereo wet{l,r};
        constexpr double pi=3.14159265358979323846;
        switch(p.type){
          case Type::Chop: {
            // Rhythmic gated cuts with short entry/exit ramps, clocked to BPM.
            // The first version is not yet reverse/retrigger-sample playback.
            const double divisions=std::pow(2.,std::floor(rate*3.999f));
            phase+=(bpm/60.)*divisions/sr;phase-=std::floor(phase);
            const float duty=.82f-.55f*tone;
            const float attack=std::clamp((float)phase*35.f,0.f,1.f);
            const float release=std::clamp((duty-(float)phase)*35.f,0.f,1.f);
            const float gain=1.f-amount+amount*std::min(attack,release);
            wet={l*gain,r*gain};break;
          }
          case Type::Motion: {
            phase+=(.08+rate*9.92)/sr;phase-=std::floor(phase);
            const float pan=std::sin((float)(2.*pi*phase))*amount;
            wet={l*(pan>0.f?1.f-pan:1.f),r*(pan<0.f?1.f+pan:1.f)};
            wet.left+=r*std::max(0.f,-pan)*.85f;
            wet.right+=l*std::max(0.f,pan)*.85f;
            break;
          }
          case Type::Filter: {
            const double cutoff=80.*std::pow(18000./80.,(double)tone);
            const float alpha=(float)(1.-std::exp(-2.*pi*cutoff/sr));
            leftLow=clean(leftLow+(l-leftLow)*alpha);
            rightLow=clean(rightLow+(r-rightLow)*alpha);
            wet={leftLow*(1.f-rate)+(l-leftLow)*rate,
                 rightLow*(1.f-rate)+(r-rightLow)*rate};
            wet.left=l+(wet.left-l)*amount;
            wet.right=r+(wet.right-r)*amount;
            break;
          }
          case Type::Saturate: {
            const float drive=1.f+amount*14.f;
            const float inverse=1.f/std::tanh(drive);
            const float colour=.6f+tone*.4f;
            const float a=std::tanh(l*drive)*inverse*colour+l*(1.f-colour);
            const float b=std::tanh(r*drive)*inverse*colour+r*(1.f-colour);
            wet={l+(a-l)*amount,r+(b-r)*amount};break;
          }
          case Type::Empty:default:return {l,r};
        }
        return {clean(sample(l+(wet.left-l)*mix)),clean(sample(r+(wet.right-r)*mix))};
    }
private:
    double sr=48000.,phase=0.;
    float leftLow=0.f,rightLow=0.f;
};
inline constexpr std::array<Type,4> availableEffects{{Type::Chop,Type::Motion,Type::Filter,Type::Saturate}};
inline const char* name(Type t) noexcept {
    switch(t){case Type::Chop:return "CHOP";case Type::Motion:return "MOTION";
    case Type::Filter:return "FILTER";case Type::Saturate:return "SATURATE";
    case Type::Empty:default:return "EMPTY";}
}
} // namespace skellcore::adlibfx
