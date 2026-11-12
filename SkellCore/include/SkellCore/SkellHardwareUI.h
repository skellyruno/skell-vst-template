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
  if(size<=0.0f)return;
  juce::Graphics::ScopedSaveState saved(g);
  g.addTransform(juce::AffineTransform::scale(size/48.0f).translated(x,y));
  const auto light=bone.brighter(0.13f);
  const auto shade=bone.darker(0.32f);

  // A recognisable bone silhouette with separate cheekbones and a real jaw.
  // Drawn at 48x48 logical pixels so it remains legible in compact VST windows.
  juce::Path skull;
  skull.startNewSubPath(24.0f,3.0f);
  skull.cubicTo(10.0f,2.3f,5.8f,10.6f,7.0f,22.0f);
  skull.cubicTo(7.3f,27.2f,11.0f,29.0f,13.7f,31.6f);
  skull.lineTo(13.5f,38.0f);
  skull.quadraticTo(13.7f,43.3f,19.1f,43.3f);
  skull.lineTo(29.9f,43.3f);
  skull.quadraticTo(35.3f,43.3f,35.4f,38.0f);
  skull.lineTo(35.2f,31.6f);
  skull.cubicTo(38.7f,28.5f,41.4f,25.4f,41.5f,19.9f);
  skull.cubicTo(42.0f,10.1f,35.7f,3.0f,24.0f,3.0f);
  skull.closeSubPath();
  g.setColour(juce::Colours::black.withAlpha(0.44f));
  g.fillPath(skull,juce::AffineTransform::translation(0.65f,1.2f));
  g.setGradientFill(juce::ColourGradient(light,13.0f,5.0f,shade,36.0f,40.0f,false));
  g.fillPath(skull);
  g.setColour(juce::Colours::white.withAlpha(0.19f));
  g.strokePath(skull,juce::PathStrokeType(0.85f));

  // Distinct dark orbital cavities and slightly angled brows give expression.
  g.setColour(bone.darker(0.20f));
  g.fillEllipse(11.3f,15.5f,12.3f,12.1f);
  g.fillEllipse(26.1f,15.5f,12.3f,12.1f);
  g.setColour(hole);
  g.fillEllipse(12.1f,17.0f,10.6f,10.0f);
  g.fillEllipse(27.0f,17.0f,10.6f,10.0f);
  g.setColour(bone.brighter(0.05f));
  g.drawLine(10.4f,15.3f,22.7f,14.6f,1.2f);
  g.drawLine(26.5f,14.6f,39.0f,15.3f,1.2f);

  // Nose and individual teeth show through the shushing finger.
  juce::Path nose;
  nose.startNewSubPath(24.5f,26.9f);
  nose.lineTo(21.6f,32.2f);nose.lineTo(27.6f,32.2f);
  nose.closeSubPath();
  g.setColour(hole);
  g.fillPath(nose);
  g.fillRoundedRectangle(16.8f,36.2f,16.2f,1.5f,0.6f);
  for(int i=0;i<4;++i)
    g.fillRoundedRectangle(17.7f+float(i)*3.5f,37.0f,0.95f,4.0f,0.45f);

  // A narrow upright finger in front of the lips, separate from the jaw:
  // dark edging, curved fingertip and lower hand make the gesture unmistakable.
  juce::Path handOutline;
  handOutline.startNewSubPath(24.5f,20.5f);
  handOutline.quadraticTo(24.5f,17.4f,26.8f,17.3f);
  handOutline.quadraticTo(29.1f,17.4f,29.1f,20.5f);
  handOutline.lineTo(29.1f,38.3f);
  handOutline.quadraticTo(32.3f,38.7f,33.8f,41.0f);
  handOutline.lineTo(35.5f,44.4f);
  handOutline.lineTo(27.2f,44.4f);
  handOutline.quadraticTo(24.5f,42.8f,24.5f,39.8f);
  handOutline.closeSubPath();
  // Small thumb and palm stay inside the jaw silhouette instead of forming
  // an oversized L shape at 30px. The narrow finger remains legible at 100%.
  g.setColour(hole.withAlpha(0.98f));
  g.strokePath(handOutline,juce::PathStrokeType(2.1f,juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
  g.setGradientFill(juce::ColourGradient(light.darker(0.07f),26.1f,18.0f,
                                         shade.darker(0.08f),33.8f,45.0f,false));
  g.fillPath(handOutline);
  g.setColour(juce::Colours::white.withAlpha(0.42f));
  g.drawLine(25.9f,20.8f,25.9f,34.8f,0.7f);
  g.setColour(hole.withAlpha(0.45f));
  g.drawLine(29.1f,40.0f,32.9f,41.3f,0.7f);

  // Minimal neon reflection rather than a separate underline or fake status LED.
  g.setColour(accent.withAlpha(0.60f));
  g.drawLine(11.9f,31.0f,13.0f,32.1f,1.0f);
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
    // No separate indicator dot: the green power glyph already indicates ON.
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
