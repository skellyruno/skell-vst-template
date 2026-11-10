#pragma once
// Shared SKELL palettes and SkellKey skull-key logo. JUCE message thread only.
// Visual identity, not audio processing; independent of plugin-specific layout.
#include <juce_graphics/juce_graphics.h>
#include <cstdint>

namespace skellcore::ui {
enum class SkullSkin : std::uint8_t { Ossuary = 0, Neon = 1, Ivory = 2, Obsidian = 3 };
inline SkullSkin skinFromIndex(int value) noexcept {
    return value >= 0 && value < 4 ? static_cast<SkullSkin>(value) : SkullSkin::Ossuary;
}
inline SkullSkin nextSkin(SkullSkin previous) noexcept {
    return skinFromIndex((static_cast<int>(previous) + 1) % 4);
}
inline const char* skinName(SkullSkin selected) noexcept {
    switch(selected) {
        case SkullSkin::Ossuary: return "Ossuary";
        case SkullSkin::Neon: return "Neon";
        case SkullSkin::Ivory: return "Ivory";
        case SkullSkin::Obsidian: return "Obsidian";
    }
    return "Ossuary";
}
struct SkullPalette {
    juce::Colour background, panel, panelHi, line, bone, boneDim, boneFaint, accent;
};
inline SkullPalette skullPalette(SkullSkin skin) noexcept {
    using C = juce::Colour;
    switch (skin) {
        case SkullSkin::Neon:
            return {C(0xff080d0a),C(0xff121b14),C(0xff1b3021),C(0xff2f4330),
                    C(0xffecf8eb),C(0xff9dad9c),C(0xff465848),C(0xff90ff44)};
        case SkullSkin::Ivory:
            return {C(0xffe8e1d2),C(0xfff5efe5),C(0xffddd5c5),C(0xffa9a08e),
                    C(0xff292923),C(0xff67675e),C(0xffb7b2a6),C(0xff456c4d)};
        case SkullSkin::Obsidian:
            return {C(0xff0c0c0e),C(0xff1a1a20),C(0xff24242b),C(0xff393942),
                    C(0xffe9edf0),C(0xffafb3bd),C(0xff585963),C(0xffbecdd6)};
        default:  // Original SkellKey ossuary: preserve its exact existing look
            return {C(0xff0b0a09),C(0xff131210),C(0xff1b1915),C(0xff2f2b23),
                    C(0xffe9e1cc),C(0xff9b937d),C(0xff4d483c),C(0xffb6f56a)};
    }
}
// Geometry extracted from the current SkellKey header mark. Keep ONE reusable
// shape in SkellCore; plugin branding and surrounding layouts remain theirs.
inline void drawSkullKeyMark(juce::Graphics& g, float x, float y,
                             juce::Colour bone, juce::Colour hole) {
    g.setColour(bone);
    g.fillEllipse(x+1.0f,y,25.0f,23.0f);
    g.fillRoundedRectangle(x+6.5f,y+17.0f,14.0f,11.0f,3.0f);
    g.fillRoundedRectangle(x+25.0f,y+10.5f,48.0f,4.2f,1.6f);
    g.fillRoundedRectangle(x+33.0f,y+7.0f,3.2f,11.5f,1.2f);
    g.fillRect(x+56.0f,y+14.0f,4.2f,9.0f);
    g.fillRect(x+63.0f,y+14.0f,4.2f,6.0f);
    g.fillRect(x+69.5f,y+14.0f,3.5f,10.0f);
    g.setColour(hole);
    g.fillEllipse(x+6.2f,y+8.4f,6.4f,6.8f);
    g.fillEllipse(x+14.6f,y+8.4f,6.4f,6.8f);
    juce::Path nose;
    nose.addTriangle(x+13.5f,y+15.0f,x+11.6f,y+19.0f,x+15.4f,y+19.0f);
    g.fillPath(nose);
    for(int i=0;i<3;++i)g.fillRect(x+9.6f+3.4f*i,y+22.0f,1.0f,5.0f);
}
} // namespace skellcore::ui
