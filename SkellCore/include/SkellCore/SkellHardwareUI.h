#pragma once
// Shared matte-hardware control language and skull gesture for Skell editors.
// UI-only: no allocation or state on the audio thread. Keep the master
// SkellCore skull proportions; the shushing finger is a semantic overlay.
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>

namespace skellcore::ui {

inline void drawShushingSkullMark(juce::Graphics& g, float x, float y, float size,
                                  juce::Colour bone, juce::Colour hole, juce::Colour accent) {
  juce::Graphics::ScopedSaveState saved(g);
  g.addTransform(juce::AffineTransform::scale(size / 48.0f).translated(x, y));
  // Core skull: cranium, jaw, eye sockets, nose and three teeth.
  g.setColour(bone);
  g.fillEllipse(5.0f, 2.5f, 37.0f, 32.0f);
  g.fillRoundedRectangle(12.0f, 27.0f, 24.0f, 14.0f, 5.0f);
  g.setColour(hole);
  g.fillEllipse(12.0f, 16.0f, 10.0f, 10.5f);
  g.fillEllipse(28.0f, 16.0f, 10.0f, 10.5f);
  juce::Path nose;
  nose.addTriangle(24.5f, 25.0f, 21.6f, 30.0f, 27.4f, 30.0f);
  g.fillPath(nose);
  for (int i = 0; i < 3; ++i)
    g.fillRoundedRectangle(17.0f + 5.0f * float(i), 34.0f, 1.4f, 5.5f, 0.7f);
  // Raised index finger directly across the lips, with the supporting hand
  // below the jaw. This silhouette stays legible at the plugin's compact size.
  g.setColour(hole);
  g.fillRoundedRectangle(25.2f, 18.7f, 11.2f, 26.3f, 5.2f);
  g.fillRoundedRectangle(29.0f, 35.0f, 15.0f, 11.0f, 4.0f);
  g.setColour(bone);
  g.fillRoundedRectangle(27.0f, 19.6f, 7.5f, 23.0f, 3.7f);
  g.fillRoundedRectangle(30.5f, 37.2f, 11.2f, 6.7f, 3.1f);
  g.setColour(accent.withAlpha(0.82f));
  g.drawLine(28.0f, 25.0f, 28.0f, 34.0f, 1.0f);
}

inline void drawHardwareKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                              float position, float startAngle, float endAngle,
                              juce::Colour accent, juce::Colour face,
                              juce::Colour rim, juce::Colour pointer) {
  const auto center = bounds.getCentre();
  const float radius = std::max(7.0f, std::min(bounds.getWidth(), bounds.getHeight()) * 0.315f);
  const float normalized = std::clamp(position, 0.0f, 1.0f);
  const float angle = startAngle + normalized * (endAngle - startAngle);
  g.setColour(juce::Colours::black.withAlpha(0.65f));
  g.fillEllipse(center.x-radius-6.0f, center.y-radius-4.0f, 2*radius+12.0f, 2*radius+12.0f);
  g.setColour(rim.withAlpha(0.75f));
  g.fillEllipse(center.x-radius-3.4f, center.y-radius-3.4f, 2*radius+6.8f, 2*radius+6.8f);
  g.setColour(juce::Colours::black);
  g.fillEllipse(center.x-radius-1.7f, center.y-radius-1.7f, 2*radius+3.4f, 2*radius+3.4f);
  juce::ColourGradient metal(face.brighter(0.30f), center.x-radius, center.y-radius,
                              face.darker(0.42f), center.x+radius, center.y+radius, false);
  metal.addColour(0.48, face.brighter(0.06f));
  g.setGradientFill(metal);
  g.fillEllipse(center.x-radius, center.y-radius, 2*radius, 2*radius);
  g.setColour(pointer.withAlpha(0.08f));
  g.drawEllipse(center.x-radius+3.0f, center.y-radius+3.0f, 2*radius-6.0f, 2*radius-6.0f, 1.0f);
  for (int i=0;i<=12;++i) {
    const float a=startAngle+float(i)/12.0f*(endAngle-startAngle);
    const float r1=radius+8.0f, r2=r1+(i%3==0?4.0f:2.0f);
    g.setColour((i%3==0?pointer:rim).withAlpha(i%3==0?0.54f:0.43f));
    g.drawLine(center.x+std::sin(a)*r1,center.y-std::cos(a)*r1,
               center.x+std::sin(a)*r2,center.y-std::cos(a)*r2,1.0f);
  }
  juce::Path empty, filled;
  empty.addCentredArc(center.x,center.y,radius+5.0f,radius+5.0f,0,startAngle,endAngle,true);
  filled.addCentredArc(center.x,center.y,radius+5.0f,radius+5.0f,0,startAngle,angle,true);
  g.setColour(rim.withAlpha(0.7f));
  g.strokePath(empty,juce::PathStrokeType(3.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  if (normalized > 0.0001f) {
    g.setColour(accent.withAlpha(0.20f));
    g.strokePath(filled,juce::PathStrokeType(5.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(filled,juce::PathStrokeType(2.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  }
  const float tip=radius*0.72f;
  g.setColour(juce::Colours::black.withAlpha(0.65f));
  g.drawLine(center.x+1.0f,center.y+1.0f,
             center.x+std::sin(angle)*tip+1.0f,center.y-std::cos(angle)*tip+1.0f,3.7f);
  g.setColour(pointer);
  g.drawLine(center.x,center.y,
             center.x+std::sin(angle)*tip,center.y-std::cos(angle)*tip,2.5f);
  g.setColour(accent.withAlpha(0.85f));
  g.fillEllipse(center.x-1.75f,center.y-1.75f,3.5f,3.5f);
}

} // namespace skellcore::ui
