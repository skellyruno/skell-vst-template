#include <SkellCore/AdlibFX.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace skellcore::adlibfx;
int main(){
 for(double sr:{22050.,44100.,48000.,96000.,192000.}){
  for(const auto type:availableEffects){
   Processor fx;fx.prepare(sr);
   Parameters p;p.type=type;p.amount=.83f;p.rate=.6f;p.tone=.45f;p.mix=1.f;
   double energy=0,peak=0;
   for(int i=0;i<(int)(sr*.12);++i){
    float in=.23f*std::sin((float)(6.283185307179586*220*i/sr));
    const auto out=fx.process({in,in},p);
    assert(std::isfinite(out.left)&&std::isfinite(out.right));
    energy+=out.left*out.left+out.right*out.right;
    peak=std::max(peak,(double)std::max(std::abs(out.left),std::abs(out.right)));
   }
   assert(energy>1.e-5&&peak<1.01);
   fx.reset();p.mix=0.f;
   const auto dry=fx.process({.24f,-.18f},p);
   assert(dry.left==.24f&&dry.right==-.18f);
   const auto bad=fx.process({std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()},p);
   assert(bad.left==0.f&&bad.right==0.f);
  }
 }
 std::puts("SkellCore AdlibFX PASS: 5 sample rates, 4 types, dry/finite/peak checks");
}
