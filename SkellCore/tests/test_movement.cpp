// Regression tests for the reusable, JUCE-free visual pulse engine.
// Reference is the original SkellGlitch ui/GlitchPulse.h behaviour.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/SkellMovement.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
struct OriginalGlitchPulse {
    std::uint32_t state=0x5E11C07u, seedForBurst=0, burstCount=0;
    int quietFrames=120, remaining=0, phase=0;
    std::uint32_t next() noexcept {
        state^=state<<13; state^=state>>17; state^=state<<5; return state;
    }
    OriginalGlitchPulse() { quietFrames=120+int(next()%151u); }
    void tick() noexcept {
        if(remaining>0){--remaining;++phase;return;}
        if(--quietFrames>0)return;
        seedForBurst=next();
        remaining=3+int(next()%3u);
        phase=0;
        quietFrames=165+int(next()%196u);
        ++burstCount;
    }
    int intensity() const noexcept {
        return remaining>0?1+int((seedForBurst+phase)%3u):0;
    }
    std::uint32_t visualSeed() const noexcept {
        return seedForBurst ^ (0x9e3779b9u * std::uint32_t(phase+1));
    }
};
} // namespace

int main() {
    skellcore::ui::SkellMovement movement;
    OriginalGlitchPulse reference;
    int repaintTicks=0, idleTicks=0;
    for(int frame=0;frame<100000;++frame){
        const auto before=movement.intensity();
        const bool repaint=movement.tick();
        reference.tick();
        assert(movement.intensity()==reference.intensity());
        assert(movement.bursts()==reference.burstCount);
        assert(movement.visualSeed()==reference.visualSeed());
        assert(repaint==(before!=0||movement.intensity()!=0));
        repaintTicks+=repaint;
        idleTicks+=!repaint;
    }
    assert(movement.bursts()>100 && idleTicks>repaintTicks*5);
    movement.reset();
    reference=OriginalGlitchPulse{};
    for(int frame=0;frame<1000;++frame){
        movement.tick(); reference.tick();
        assert(movement.intensity()==reference.intensity());
    }
    // No background motion when a user enables reduced-motion mode.
    skellcore::ui::SkellMovement a;
    for(int i=0;i<800 && !a.active();++i)a.tick();
    assert(a.active());
    assert(a.setReducedMotion(true));
    for(int i=0;i<1000;++i){assert(!a.tick());assert(a.intensity()==0);}
    assert(!a.setReducedMotion(true));
    a.setReducedMotion(false);
    assert(a.active());
    assert(a.setEnabled(false));
    for(int i=0;i<100;++i)assert(!a.tick());
    assert(a.intensity()==0);
    a.setEnabled(true);
    // Invalid config is clamped, never modulo zero or unbounded bursts.
    skellcore::ui::SkellMovementConfig invalid;
    invalid.firstQuietSpreadTicks=0;
    invalid.quietSpreadTicks=-400;
    invalid.burstSpreadTicks=0;
    invalid.maximumIntensity=0;
    invalid.burstMinTicks=0;
    skellcore::ui::SkellMovement edge(0,invalid);
    for(int i=0;i<30000;++i){
        edge.tick();
        assert(edge.intensity()>=0 && edge.intensity()<=8);
    }
    // Same seeds produce identical motion sequences.
    skellcore::ui::SkellMovement b(12345),c(12345),d(654321);
    int differences=0;
    for(int i=0;i<15000;++i){
        b.tick();c.tick();d.tick();
        assert(b.intensity()==c.intensity()&&b.visualSeed()==c.visualSeed());
        differences+=(b.intensity()!=d.intensity()||b.bursts()!=d.bursts());
    }
    assert(differences>0);
    std::printf("PASS SkellMovement: GlitchPulse parity, %u bursts, %d repaints/%d idle ticks, reduced motion, config safety\n",
                movement.bursts(),repaintTicks,idleTicks);
}
