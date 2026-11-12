#pragma once
// Bounded UI-only layout helpers. Deliberately return rectangles rather than
// owning controls or forcing one plugin layout onto another.
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace skellcore::ui {

enum class SkellLayoutDensity { Compact, Regular, Wide };

struct SkellBreakpoints {
    int compactBelow = 420;
    int wideAt = 760;

    SkellLayoutDensity classify(int width) const noexcept {
        const int compact = juce::jmax(1, compactBelow);
        const int wide = juce::jmax(compact+1, wideAt);
        if (width < compact) return SkellLayoutDensity::Compact;
        if (width >= wide) return SkellLayoutDensity::Wide;
        return SkellLayoutDensity::Regular;
    }
};

// For editors with many controls, return an empty vector if the requested
// region cannot safely fit even 1px-high rows. The host can choose scrolling,
// collapsed panels or a different view; no overlaps are silently introduced.
inline std::vector<juce::Rectangle<int>> skellResponsiveCells(
    juce::Rectangle<int> area, int count,
    int minCellWidth = 128, int preferredCellHeight = 64,
    int preferredGap = 8, int maxColumns = 4) {
    if (count <= 0 || count > 1024 || area.getWidth() <= 0
        || area.getHeight() <= 0) return {};

    const int w = area.getWidth(), h = area.getHeight();
    const int gap = juce::jlimit(0, 512, preferredGap);
    const int minimum = juce::jmax(1, minCellWidth);
    const int desiredColumns = static_cast<int>(
        (static_cast<std::int64_t>(w) + gap)
        / (static_cast<std::int64_t>(minimum) + gap));
    const int columns = juce::jlimit(1, std::min({count, juce::jmax(1,maxColumns), w}),
                                    juce::jmax(1, desiredColumns));
    const int rows = (count + columns - 1) / columns;
    if (rows > h) return {};

    const int horizontalGap = columns > 1
        ? juce::jmin(gap, (w - columns) / (columns - 1)) : 0;
    const int verticalGap = rows > 1
        ? juce::jmin(gap, (h - rows) / (rows - 1)) : 0;

    const int availableWidth = w - (columns-1)*horizontalGap;
    const int baseWidth = availableWidth / columns;
    const int extraPixels = availableWidth % columns;
    const int availableHeight = h - (rows-1)*verticalGap;
    const int rowHeight = juce::jlimit(1, availableHeight / rows,
        juce::jmax(1, preferredCellHeight));

    std::vector<juce::Rectangle<int>> cells;
    cells.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int col = i % columns;
        const int row = i / columns;
        const int x = area.getX() + col*(baseWidth+horizontalGap)
                      + juce::jmin(col, extraPixels);
        const int y = area.getY() + row*(rowHeight+verticalGap);
        const int cellWidth = baseWidth + (col < extraPixels ? 1 : 0);
        cells.emplace_back(x, y, cellWidth, rowHeight);
    }
    return cells;
}

} // namespace skellcore::ui
