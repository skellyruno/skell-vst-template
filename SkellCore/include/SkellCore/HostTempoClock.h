#pragma once
// Use only in JUCE AudioProcessor code after JUCE has been initialized.
// Tempo remains host-controlled; advance each processed block exactly once.
#include <SkellCore/Core.h>
#include <juce_audio_processors/juce_audio_processors.h>
namespace skellcore {
inline void updateTempoFromPlayhead(TempoClock& clock,
                                    const juce::AudioPlayHead::PositionInfo& position) noexcept {
    // JUCE 8 PositionInfo returns juce::Optional, which does not expose the
    // C++23 std::optional::value_or API. Read it via the checked dereference.
    double bpm=clock.bpm();
    if (const auto hostBpm=position.getBpm()) bpm=*hostBpm;
    double ppq=clock.beatPosition();
    if (const auto hostPpq=position.getPpqPosition()) ppq=*hostPpq;
    clock.update(bpm,ppq,position.getIsPlaying());
}
} // namespace skellcore
