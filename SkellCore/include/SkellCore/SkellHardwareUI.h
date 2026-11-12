#pragma once
// Shared hardware materials and plugin-specific glyphs for SkellCore editors.
// All drawing is message-thread only. No audio processing, file I/O, DSP state,
// or host parameter IDs are modified by these UI primitives.
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace skellcore::ui {

// The shushing overlay changes the expression, never adds a key to the skull.
inline void drawShushingSkullMark(juce::Graphics& g,float x,float y,float size,
                                  juce::Colour bone,juce::Colour hole,juce::Colour accent) {
  if (size<=0.0f) return;
  juce::Graphics::ScopedSaveState saved(g);
  g.addTransform(juce::AffineTransform::scale(size/48.0f).translated(x,y));
  const auto light=bone.brighter(0.13f);
  // Sculpted cranium, cheekbones and lower jaw with distinct dark cavities.
  g.setColour(juce::Colours::black.withAlpha(0.45f));
  g.fillEllipse(5.1f,5.0f,38.5f,33.0f);
  g.setGradientFill(juce::ColourGradient(light,11.0f,3.0f,bone.darker(0.30f),38.0f,35.0f,false));
  g.fillEllipse(5.0f,2.5f,38.0f,32.8f);
  g.fillRoundedRectangle(12.0f,26.8f,24.0f,15.0f,5.0f);
  g.setColour(juce::Colours::white.withAlpha(0.16f));
  g.drawEllipse(7.5f,5.0f,32.0f,25.5f,0.75f);
  g.setColour(hole);
  g.fillEllipse(11.6f,16.6f,11.3f,10.4f);
  g.fillEllipse(27.0f,16.6f,11.3f,10.4f);
  // Lower, rounded orbital rims read like a deliberate facial expression.
  g.setColour(bone.darker(0.10f));
  g.drawLine(10.6f,16.1f,22.7f,14.5f,1.4f);
  g.drawLine(27.1f,14.5f,39.0f,16.1f,1.4f);
  g.setColour(hole);
  juce::Path nose;
  nose.addTriangle(24.7f,25.0f,21.7f,30.4f,27.2f,30.4f);
  g.fillPath(nose);
  g.fillRoundedRectangle(16.0f,34.6f,17.0f,1.4f,0.7f);
  for(int i=0;i<3;++i)
    g.fillRoundedRectangle(17.4f+float(i)*5.0f,35.3f,1.2f,5.1f,0.6f);

  // Index finger in front of the lips: outline first prevents merging
  // into the skull at 30–48px sizes. A hand rests naturally under the chin.
  g.setColour(hole.withAlpha(0.94f));
  g.fillRoundedRectangle(25.1f,17.7f,10.6f,27.7f,5.0f);
  g.fillRoundedRectangle(28.0f,36.8f,16.2f,10.7f,4.5f);
  g.setGradientFill(juce::ColourGradient(light,27.0f,18.0f,
                                         bone.darker(0.19f),36.0f,44.0f,false));
  g.fillRoundedRectangle(27.0f,19.0f,6.75f,24.9f,3.35f);
  g.fillRoundedRectangle(30.5f,38.4f,11.6f,6.0f,3.0f);
  g.setColour(juce::Colours::white.withAlpha(0.30f));
  g.drawLine(28.6f,21.8f,28.6f,35.3f,0.85f);
  g.setColour(accent.withAlpha(0.90f));
  g.drawLine(24.0f,46.0f,42.0f,46.0f,1.25f);
}

// Shared control: recessed mount, heavy dark metal bezel, convex anodized
// cap, directional highlight, precision index, machined ticks and glassy track.
inline void drawHardwareKnob(juce::Graphics& g,juce::Rectangle<float> bounds,
                              float position,float startAngle,float endAngle,
                              juce::Colour accent,juce::Colour face,
                              juce::Colour rim,juce::Colour pointer) {
  if(bounds.isEmpty())return;
  const float scale=std::min(bounds.getWidth(),bounds.getHeight());
  const auto center=bounds.getCentre();
  const float cx=center.x,cy=center.y-0.01f*scale;
  const float r=std::max(7.0f,scale*0.31f);
  const float p=std::isfinite(position)?std::clamp(position,0.0f,1.0f):0.0f;
  const float angle=startAngle+p*(endAngle-startAngle);
  const auto darkest=juce::Colour(0xff050808);
  // Contact shadow + deeply recessed mounting well.
  g.setColour(juce::Colours::black.withAlpha(0.24f));
  g.fillEllipse(cx-r-8.0f,cy-r-4.0f,2*r+16.0f,2*r+16.0f);
  g.setGradientFill(juce::ColourGradient(rim.brighter(0.16f),cx-r-6.0f,cy-r-6.0f,
                                         darkest,cx+r+6.0f,cy+r+6.0f,false));
  g.fillEllipse(cx-r-5.0f,cy-r-5.0f,2*r+10.0f,2*r+10.0f);
  g.setColour(darkest);
  g.fillEllipse(cx-r-2.6f,cy-r-2.6f,2*r+5.2f,2*r+5.2f);
  g.setColour(juce::Colours::white.withAlpha(0.14f));
  g.drawEllipse(cx-r-4.3f,cy-r-4.3f,2*r+8.6f,2*r+8.6f,0.75f);
  // The thick two-part cap is inset under the bezel.
  const float capR=r*0.91f;
  g.setGradientFill(juce::ColourGradient(face.brighter(0.48f),cx-capR,cy-capR,
                                         face.darker(0.64f),cx+capR,cy+capR,false));
  g.fillEllipse(cx-capR,cy-capR,2*capR,2*capR);
  const float topR=capR*0.88f;
  g.setGradientFill(juce::ColourGradient(face.brighter(0.32f),cx-topR*0.75f,cy-topR,
                                         face.darker(0.33f),cx+topR,cy+topR,false));
  g.fillEllipse(cx-topR,cy-topR,2*topR,2*topR);
  // Directional steel rim: small sharp upper crescent and lower edge shading.
  g.setColour(pointer.withAlpha(0.24f));
  g.drawEllipse(cx-capR+1.6f,cy-capR+1.5f,2*capR-3.2f,2*capR-3.2f,0.9f);
  g.setColour(darkest.withAlpha(0.45f));
  g.drawEllipse(cx-topR+1.4f,cy-topR+1.4f,2*topR-2.8f,2*topR-2.8f,1.0f);
  // Discrete engraved calibrations outside the cap.
  for(int i=0;i<=12;++i) {
    const float a=startAngle+float(i)/12.0f*(endAngle-startAngle);
    const float inside=r+9.0f;
    const float outside=inside+(i%3==0?3.8f:2.1f);
    g.setColour((i%3==0?pointer:rim).withAlpha(i%3==0?0.67f:0.52f));
    g.drawLine(cx+std::sin(a)*inside,cy-std::cos(a)*inside,
               cx+std::sin(a)*outside,cy-std::cos(a)*outside,i%3==0?1.2f:0.8f);
  }
  juce::Path inactive,active;
  inactive.addCentredArc(cx,cy,r+5.4f,r+5.4f,0.0f,startAngle,endAngle,true);
  active.addCentredArc(cx,cy,r+5.4f,r+5.4f,0.0f,startAngle,angle,true);
  g.setColour(darkest);
  g.strokePath(inactive,juce::PathStrokeType(4.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  g.setColour(rim.brighter(0.27f));
  g.strokePath(inactive,juce::PathStrokeType(1.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  if(p>0.0001f) {
    g.setColour(accent.withAlpha(0.19f));
    g.strokePath(active,juce::PathStrokeType(5.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(active,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  }
  // Single deep index notch, not a bright needle drawn through the face.
  const float inner=topR*0.43f,outer=topR*0.84f;
  const float dx=std::sin(angle),dy=-std::cos(angle);
  g.setColour(darkest.withAlpha(0.84f));
  g.drawLine(cx+dx*inner+0.9f,cy+dy*inner+1.1f,
             cx+dx*outer+0.9f,cy+dy*outer+1.1f,4.0f);
  g.setColour(pointer);
  g.drawLine(cx+dx*inner,cy+dy*inner,cx+dx*outer,cy+dy*outer,2.15f);
  g.setColour(darkest.withAlpha(0.50f));
  g.fillEllipse(cx-2.9f,cy-2.9f,5.8f,5.8f);
  g.setColour(face.brighter(0.24f));
  g.fillEllipse(cx-1.5f,cy-1.5f,3.0f,3.0f);
}

enum class HardwareGlyph { Settings, Power };

// Shared header switches: compact, bevelled housing and true iconography.
inline void drawHardwareIconButton(juce::Graphics& g,juce::Rectangle<float> bounds,
                                  HardwareGlyph glyph,bool active,bool over,bool down,
                                  juce::Colour accent,juce::Colour metal,juce::Colour ink,
                                  bool focused=false) {
  auto r=bounds.reduced(1.0f);
  if(r.isEmpty())return;
  g.setColour(juce::Colours::black.withAlpha(0.46f));
  g.fillRoundedRectangle(r.translated(0.0f,2.0f),8.0f);
  g.setGradientFill(juce::ColourGradient(metal.brighter(down?0.04f:over?0.46f:0.27f),
                                         r.getX(),r.getY(),
                                         metal.darker(down?0.31f:0.45f),
                                         r.getRight(),r.getBottom(),false));
  g.fillRoundedRectangle(r,8.0f);
  g.setColour(juce::Colours::white.withAlpha(over?0.19f:0.10f));
  g.drawHorizontalLine(juce::roundToInt(r.getY()+1),r.getX()+7.0f,r.getRight()-7.0f);
  g.setColour(focused?accent:metal.brighter(0.48f));
  g.drawRoundedRectangle(r.reduced(0.45f),7.4f,focused?1.6f:0.95f);
  const auto c=r.getCentre();
  const float cx=c.x,cy=c.y+(down?1.0f:0.0f);
  const auto glyphColour=glyph==HardwareGlyph::Power
      ?(active?accent:juce::Colour(0xffa2aca9))
      :(over?accent:juce::Colour(0xfff1f6f2));
  if(glyph==HardwareGlyph::Power) {
    juce::Path ring;
    ring.addCentredArc(cx,cy+0.6f,9.0f,9.0f,0.0f,0.60f,
                       juce::MathConstants<float>::twoPi-0.60f,true);
    g.setColour(ink.withAlpha(0.7f));
    g.strokePath(ring,juce::PathStrokeType(4.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(glyphColour);
    g.strokePath(ring,juce::PathStrokeType(2.35f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.drawLine(cx,cy-11.6f,cx,cy+0.2f,2.6f);
    if(active) {
      g.setColour(accent.withAlpha(0.52f));
      g.fillEllipse(cx+8.5f,cy-14.0f,3.7f,3.7f);
    }
  } else {
    juce::Path gear;
    for(int i=0;i<32;++i) {
      const float a=juce::MathConstants<float>::twoPi*float(i)/32.0f;
      const float radius=(i%4==0 || i%4==1)?11.0f:8.65f;
      const float xx=cx+std::sin(a)*radius, yy=cy-std::cos(a)*radius;
      if(i==0)gear.startNewSubPath(xx,yy);else gear.lineTo(xx,yy);
    }
    gear.closeSubPath();
    g.setColour(ink.withAlpha(0.75f));
    g.fillPath(gear,juce::AffineTransform::translation(0.8f,1.1f));
    g.setColour(glyphColour);
    g.fillPath(gear);
    g.setColour(metal.darker(0.55f));
    g.fillEllipse(cx-4.7f,cy-4.7f,9.4f,9.4f);
    g.setColour(glyphColour.withAlpha(0.78f));
    g.drawEllipse(cx-4.2f,cy-4.2f,8.4f,8.4f,1.1f);
  }
}

// Rendered by SkellCore; the host plug-in still owns its audio parameter.
class HardwareIconButton final : public juce::Button {
public:
  HardwareIconButton(const juce::String& name,HardwareGlyph icon,
                     juce::Colour accent,juce::Colour metal,juce::Colour ink)
      : juce::Button(name),glyph_(icon),accent_(accent),metal_(metal),ink_(ink) {
    setButtonText(name);
  }
  void paintButton(juce::Graphics& g,bool hover,bool down) override {
    drawHardwareIconButton(g,getLocalBounds().toFloat(),glyph_,
                           glyph_==HardwareGlyph::Power ? getToggleState() : true,
                           hover,down,accent_,metal_,ink_,hasKeyboardFocus(true));
  }
private:
  HardwareGlyph glyph_;
  juce::Colour accent_,metal_,ink_;
};

} // namespace skellcore::ui
