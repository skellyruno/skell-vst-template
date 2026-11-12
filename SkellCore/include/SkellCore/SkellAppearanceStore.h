#pragma once
// SKELL product-scoped appearance preferences. UI/message thread only.
// A plugin's appearance never changes audio, automation, or DAW preset state.
#include <SkellCore/StudioUI.h>
#include <juce_core/juce_core.h>
#include <cstddef>

namespace skellcore::ui {

// The legacy StudioSkinStore remains untouched for backwards compatibility.
// New plugins should use an immutable per-product ID rather than sharing the
// global studio-skin-v1.txt across unrelated plugin identities.
class SkellAppearanceStore final {
public:
    static juce::File fileFor(const juce::String& stableProductId) {
        if (stableProductId.trim().isEmpty())
            return {};
        const auto id = stableProductId.trim();
        // Hash the complete ID: filenames cannot contain '/', '..' or other
        // path separators even when names come from manifests or user input.
        const juce::MD5 digest(id.toRawUTF8(),
                               static_cast<std::size_t>(id.getNumBytesAsUTF8()));
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SkellAudio")
            .getChildFile("Appearance")
            .getChildFile("skin-" + digest.toHexString() + ".txt");
    }

    static StudioSkin load(const juce::String& stableProductId,
                           StudioSkin fallback = StudioSkin::Ivory) {
        const auto file = fileFor(stableProductId);
        if (file == juce::File() || !file.existsAsFile())
            return fallback;
        // Preserve caller's fallback on an invalid or damaged preference,
        // rather than accidentally switching it to Ivory.
        const auto stored = file.loadFileAsString().trim();
        for (int i = 0; i != 4; ++i) {
            const auto candidate = studioSkinFromIndex(i);
            if (stored.equalsIgnoreCase(studioSkinName(candidate)))
                return candidate;
        }
        return fallback;
    }

    static bool save(const juce::String& stableProductId, StudioSkin value) {
        const auto file = fileFor(stableProductId);
        if (file == juce::File()) return false;
        const auto parent = file.getParentDirectory();
        return parent.createDirectory().wasOk()
            && file.replaceWithText(juce::String(studioSkinName(value)) + "\n");
    }
};

} // namespace skellcore::ui
