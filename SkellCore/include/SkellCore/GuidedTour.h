#pragma once
// SkellCore Guided Tour v1 — opt-in onboarding for *complex* JUCE plugin editors.
// Adapted from OmniChoir FX's first-run spotlight/Next/Skip/replay concept.
// GUI/message thread only. No DSP changes, timers, audio allocations or host PDC.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace skellcore::ui {

struct GuidedTourPalette {
    juce::Colour accent { 0xff54cf83 };
    juce::Colour panel { 0xff161d19 };
    juce::Colour text { 0xfff1f1e8 };
    juce::Colour secondary { 0xffaab8ad };
    juce::Colour shade { 0xd9000000 };
};

struct GuidedTourStep {
    // SafePointer ensures a deleted JUCE control cannot be dereferenced.
    juce::Component::SafePointer<juce::Component> target;
    juce::Component::SafePointer<juce::Component> alsoTarget;
    juce::String title, description;
    // Optional: switch to the correct page/expand a panel first.
    // This callback runs on the JUCE message thread, never the audio thread.
    std::function<void()> onEnter;
    bool expectsTarget = false;

    GuidedTourStep(juce::Component* primary,
                   juce::String heading, juce::String explanation,
                   std::function<void()> activate = {},
                   juce::Component* secondary = nullptr)
        : target(primary), alsoTarget(secondary),
          title(std::move(heading)), description(std::move(explanation)),
          onEnter(std::move(activate)), expectsTarget(primary != nullptr) {}
};

enum class GuidedTourFinish { Completed, Skipped, Unavailable };

// User-level tour completion is intentionally not a preset parameter or APVTS
// state field. An updated tour version can be shown once again.
class GuidedTourPreferences {
public:
    static juce::File completionFile(const juce::String& pluginId, int version = 1) {
        auto safe = pluginId.retainCharacters(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.");
        if (safe.isEmpty()) safe = "unknown";
        safe = safe.substring(0, 80);
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SkellAudio")
            .getChildFile("GuidedTours")
            .getChildFile("tour-" + safe + "-v" + juce::String(juce::jmax(1, version)) + ".done");
    }
    static bool shouldShow(const juce::String& pluginId, int version = 1) {
        return !completionFile(pluginId, version).existsAsFile();
    }
    static bool markSeen(const juce::String& pluginId, int version = 1) {
        const auto file = completionFile(pluginId, version);
        return file.getParentDirectory().createDirectory().wasOk()
            && file.replaceWithText("seen\n");
    }
};

// Compact native tutorial navigation shared across complex Skell plugins.
// Deliberately independent of each plugin's LookAndFeel and icon font.
class GuidedTourNavigationLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    juce::Font getTextButtonFont(juce::TextButton&, int) override {
        return juce::Font(juce::FontOptions(11.5f).withStyle("Bold"));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& colour,
                              bool hover, bool down) override {
        const auto area = button.getLocalBounds().toFloat().reduced(.75f);
        const auto background = down ? colour.darker(.13f)
                                    : hover ? colour.brighter(.07f) : colour;
        const bool primary = button.getButtonText() == "NEXT"
                          || button.getButtonText() == "DONE";
        g.setColour(primary ? background : background.withAlpha(.56f));
        g.fillRoundedRectangle(area, 6.f);
        const auto border = primary ? background.brighter(.18f).withAlpha(.85f)
            : button.findColour(juce::TextButton::textColourOffId).withAlpha(.34f);
        g.setColour(border);
        g.drawRoundedRectangle(area, 6.f, .95f);
    }
};

class GuidedTourOverlay final : public juce::Component {
public:
    using Close = std::function<void(GuidedTourFinish)>;

    GuidedTourOverlay(std::vector<GuidedTourStep> steps, GuidedTourPalette colours, Close close)
        : steps_(std::move(steps)), colours_(colours), onClose_(std::move(close)) {
        setName("SkellCore Guided Tour");
        setComponentID("skellcore-guided-tour");
        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(true);

        back_.setButtonText("BACK");
        next_.setButtonText("NEXT");
        skip_.setButtonText("SKIP");
        back_.setTooltip("Previous tutorial step");
        next_.setTooltip("Next tutorial step");
        skip_.setTooltip("Dismiss this introduction. Replay it from Help.");
        for (auto* button : { &back_, &next_, &skip_ }) {
            // Editor-specific LookAndFeel implementations may ignore JUCE
            // button colours (or draw text in the same colour as the panel).
            // Own a neutral LAF for tutorial navigation, not the host editor's.
            button->setLookAndFeel(&buttonLook_);
            button->setColour(juce::TextButton::buttonColourId, colours_.panel.brighter(.10f));
            button->setColour(juce::TextButton::textColourOffId, colours_.text);
            button->setColour(juce::TextButton::buttonOnColourId, colours_.accent);
            addAndMakeVisible(*button);
        }
        next_.setColour(juce::TextButton::buttonColourId, colours_.accent);
        next_.setColour(juce::TextButton::textColourOffId, colours_.panel);
        back_.onClick = [this] { previous(); };
        next_.onClick = [this] { advance(); };
        skip_.onClick = [this] { requestClose(GuidedTourFinish::Skipped); };
    }

    ~GuidedTourOverlay() override {
        for (auto* button : { &back_, &next_, &skip_ })
            button->setLookAndFeel(nullptr);
    }

    void setColours(GuidedTourPalette palette) {
        colours_ = palette;
        for (auto* button : { &back_, &next_, &skip_ }) {
            button->setColour(juce::TextButton::buttonColourId, colours_.panel.brighter(.10f));
            button->setColour(juce::TextButton::textColourOffId, colours_.text);
        }
        next_.setColour(juce::TextButton::buttonColourId, colours_.accent);
        next_.setColour(juce::TextButton::textColourOffId, colours_.panel);
        repaint();
    }
    void begin() {
        if (steps_.empty()) { requestClose(GuidedTourFinish::Unavailable); return; }
        select(0, +1);
        grabKeyboardFocus();
    }
    int stepIndex() const noexcept { return static_cast<int>(index_); }
    int stepCount() const noexcept { return static_cast<int>(steps_.size()); }
    bool isClosing() const noexcept { return closing_; }

    void advance() {
        if (closing_) return;
        if (index_ + 1 >= steps_.size()) {
            requestClose(GuidedTourFinish::Completed);
            return;
        }
        select(index_ + 1, +1);
    }
    void previous() {
        if (!closing_ && index_ > 0)
            select(index_ - 1, -1);
    }
    void skip() { requestClose(GuidedTourFinish::Skipped); }

    // This is useful when a host/plugin changes pages or resizes a panel.
    void refresh() { resized(); repaint(); }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key.getKeyCode() == juce::KeyPress::escapeKey) {
            skip(); return true;
        }
        if (key.getKeyCode() == juce::KeyPress::leftKey) {
            previous(); return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey
            || key.getKeyCode() == juce::KeyPress::returnKey
            || key.getTextCharacter() == ' ') {
            advance(); return true;
        }
        return false;
    }

    void mouseDown(const juce::MouseEvent&) override {
        // Swallow clicks on dimmed areas. Parameters underneath never move.
        grabKeyboardFocus();
    }

    juce::Rectangle<int> informationBounds() const { return bubbleBounds(); }

    juce::Rectangle<int> spotlightBounds() const {
        if (steps_.empty() || index_ >= steps_.size()) return {};
        const auto* primary = steps_[index_].target.getComponent();
        if (!isUsable(primary)) return {};

        auto box = getLocalArea(primary, primary->getLocalBounds());
        const auto* secondary = steps_[index_].alsoTarget.getComponent();
        if (isUsable(secondary))
            box = box.getUnion(getLocalArea(secondary, secondary->getLocalBounds()));
        return box.expanded(6).getIntersection(getLocalBounds().reduced(3));
    }

    void resized() override {
        const auto bubble = bubbleBounds();
        if (bubble.getWidth() < 220) {
            // Narrow side-docked cards: readable, full-word BACK and NEXT
            // controls, with SKIP in the heading rather than 3 tiny buttons.
            constexpr int edge = 10;
            constexpr int gutter = 6;
            const int buttonHeight = juce::jmin(24, juce::jmax(19, bubble.getHeight()/6));
            const int buttonWidth = juce::jmax(1,(bubble.getWidth()-2*edge-gutter)/2);
            const int y = bubble.getBottom()-edge-buttonHeight;
            back_.setBounds(bubble.getX()+edge, y, buttonWidth, buttonHeight);
            next_.setBounds(bubble.getRight()-edge-buttonWidth, y, buttonWidth, buttonHeight);
            skip_.setBounds(bubble.getRight()-edge-45, bubble.getY()+6, 45, 22);
            return;
        }
        // Compact buttons preserve the reading area and are more proportional
        // to the 13.5pt heading. Keep hit targets at least 24px high.
        const int edge = juce::jmax(8, juce::jmin(12, bubble.getWidth() / 24));
        const int buttonHeight = juce::jmin(25, juce::jmax(24, bubble.getHeight() / 6));
        const int rowTop = bubble.getBottom() - edge - buttonHeight;
        const int available = juce::jmax(1, bubble.getWidth() - edge * 2);
        const int buttonWidth = juce::jmin(62, juce::jmax(1, (available - 14) / 3));
        next_.setBounds(bubble.getRight() - edge - buttonWidth, rowTop, buttonWidth, buttonHeight);
        skip_.setBounds(next_.getX() - 7 - buttonWidth, rowTop, buttonWidth, buttonHeight);
        back_.setBounds(skip_.getX() - 7 - buttonWidth, rowTop, buttonWidth, buttonHeight);
    }

    void paint(juce::Graphics& g) override {
        if (steps_.empty() || index_ >= steps_.size() || closing_) return;
        const auto highlight = spotlightBounds().toFloat();
        juce::Path dim;
        dim.addRectangle(getLocalBounds().toFloat());
        if (!highlight.isEmpty()) dim.addRoundedRectangle(highlight, 10.f);
        dim.setUsingNonZeroWinding(false);
        g.setColour(colours_.shade);
        g.fillPath(dim);

        if (!highlight.isEmpty()) {
            g.setColour(colours_.accent.withAlpha(.16f));
            g.fillRoundedRectangle(highlight.expanded(2.f), 11.f);
            g.setColour(colours_.accent);
            g.drawRoundedRectangle(highlight, 10.f, 2.f);
        }

        const auto card = bubbleBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(.25f));
        g.fillRoundedRectangle(card.translated(0.f, 3.f), 10.f);
        g.setColour(colours_.panel);
        g.fillRoundedRectangle(card, 10.f);
        g.setColour(colours_.accent.withAlpha(.82f));
        g.drawRoundedRectangle(card.reduced(.7f), 10.f, 1.2f);
        const auto bubble = bubbleBounds();
        const bool compact = bubble.getWidth() < 220;
        const auto counter = juce::String(static_cast<int>(index_)+1)
                           + " / " + juce::String(static_cast<int>(steps_.size()));
        const auto head = juce::Rectangle<int>(bubble.getX()+13,
            bubble.getY()+9, juce::jmax(1, bubble.getWidth()-(compact ? 76 : 62)), 22);
        g.setColour(colours_.accent);
        g.setFont(juce::Font(juce::FontOptions(compact ? 12.0f : 13.5f).withStyle("Bold")));
        g.drawFittedText(steps_[index_].title, head, juce::Justification::centredLeft, 1);
        auto body = bubble.reduced(13, 0);
        body.setY(head.getBottom()+(compact ? 16 : 3));
        body.setBottom(juce::jmax(body.getY(), back_.getY()-8));
        g.setColour(colours_.text);
        g.setFont(juce::Font(juce::FontOptions(compact ? 11.0f : 11.5f)));
        g.drawFittedText(steps_[index_].description, body,
            juce::Justification::topLeft, 4);
        g.setColour(colours_.secondary);
        g.setFont(juce::Font(juce::FontOptions(10.f)));
        g.drawText(counter,
            compact ? juce::Rectangle<int>(bubble.getX()+13, bubble.getY()+33,
                                            juce::jmax(1,bubble.getWidth()-26), 14)
                    : juce::Rectangle<int>(bubble.getRight()-45, bubble.getY()+9, 32, 22),
            compact ? juce::Justification::centredLeft : juce::Justification::centredRight, true);
    }

private:
    bool isUsable(const juce::Component* target) const {
        if (target == nullptr || target->getWidth() <= 0 || target->getHeight() <= 0)
            return false;
        const auto* parent = getParentComponent();
        for (auto* current = target; current != nullptr; current = current->getParentComponent()) {
            // The root editor may be intentionally non-showing during a
            // headless snapshot. Its own children must still be tested.
            if (current == parent) return true;
            if (!current->isVisible()) return false;
        }
        return false;
    }

    juce::Rectangle<int> bubbleBounds() const {
        if (getWidth() <= 0 || getHeight() <= 0) return {};
        int width = juce::jmin(350, juce::jmax(1, getWidth() - 24));
        int height = juce::jmin(140, juce::jmax(1, getHeight() - 16));
        const auto target = spotlightBounds();
        constexpr int gap = 8;
        if (target.isEmpty())
            return { (getWidth()-width)/2, (getHeight()-height)/2, width, height };

        const int above = target.getY() - gap;
        const int below = getHeight() - target.getBottom() - gap;
        const bool fitsAbove = above >= height + gap;
        const bool fitsBelow = below >= height + gap;
        if (fitsAbove || fitsBelow) {
            const bool useBelow = fitsBelow && (!fitsAbove || below >= above);
            const int y = useBelow ? target.getBottom() + gap
                                   : target.getY() - height - gap;
            const int x = juce::jlimit(0, juce::jmax(0, getWidth()-width),
                                      target.getCentreX()-width/2);
            return {x, y, width, height};
        }

        // Compact editors may have too little room vertically, yet 130-180px
        // on either side of a knob. Dock there instead of obscuring the
        // highlighted control with a centered panel.
        const int sideLeft = juce::jmax(0, target.getX()-2*gap);
        const int sideRight = juce::jmax(0, getWidth()-target.getRight()-2*gap);
        if (juce::jmax(sideLeft,sideRight) >= 128) {
            const bool useRight = sideRight >= sideLeft;
            width = juce::jmin(280, useRight ? sideRight : sideLeft);
            height = juce::jmin(150, juce::jmax(1, getHeight()-16));
            const int x = useRight ? target.getRight()+gap : target.getX()-gap-width;
            const int y = juce::jlimit(0, juce::jmax(0,getHeight()-height),
                                      target.getCentreY()-height/2);
            return {x, y, width, height};
        }

        // Very narrow hosts: keep the card inside the viewport, favoring the
        // side with the least visual obstruction of the target.
        const int y = juce::jlimit(0, juce::jmax(0,getHeight()-height),
                                   above >= below ? gap : getHeight()-height-gap);
        const int x = juce::jlimit(0, juce::jmax(0,getWidth()-width),
                                  target.getCentreX()-width/2);
        return {x,y,width,height};
    }

    void select(std::size_t requested, int direction) {
        if (closing_) return;
        const auto before = index_;
        for (int index = static_cast<int>(requested);
             index >= 0 && index < static_cast<int>(steps_.size()); index += direction) {
            index_ = static_cast<std::size_t>(index);
            if (steps_[index_].onEnter) steps_[index_].onEnter();
            const auto* candidate = steps_[index_].target.getComponent();
            // A no-target step is a valid full-editor introduction.
            if (candidate == nullptr && steps_[index_].expectsTarget)
                continue;
            if (candidate == nullptr || isUsable(candidate)) {
                back_.setEnabled(index_ > 0);
                next_.setButtonText(index_+1==steps_.size() ? "DONE" : "NEXT");
                resized();
                repaint();
                return;
            }
        }
        // A later page may have been removed by the plugin. The earlier
        // valid steps still count as an introduction rather than a broken tour.
        if (direction > 0 && requested > 0)
            requestClose(GuidedTourFinish::Completed);
        else if (direction < 0) {
            index_ = before;
            resized();
            repaint();
        } else {
            // No usable first step: let the next editor opening retry.
            requestClose(GuidedTourFinish::Unavailable);
        }
    }

    void requestClose(GuidedTourFinish result) {
        if (closing_) return;
        closing_ = true;
        setVisible(false);
        auto weak = juce::Component::SafePointer<GuidedTourOverlay>(this);
        juce::MessageManager::callAsync([weak, result] {
            if (weak == nullptr) return;
            const auto callback = weak->onClose_;
            if (callback) callback(result);
            // callback may delete the overlay; do not access weak after it.
        });
    }

    std::vector<GuidedTourStep> steps_;
    GuidedTourPalette colours_;
    Close onClose_;
    std::size_t index_ = 0;
    bool closing_ = false;
    // Constructed before buttons and destroyed afterwards (reverse member
    // destruction order); a local LAF gives consistent legible controls.
    GuidedTourNavigationLookAndFeel buttonLook_;
    juce::TextButton back_, next_, skip_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuidedTourOverlay)
};

// Editor-owned helper. The actual plugin opts in: simple SKELL plugins never
// get a tutorial unless they deliberately instantiate this controller.
class GuidedTourController final : private juce::ComponentListener {
public:
    GuidedTourController(juce::Component& editor, juce::String stablePluginId,
                         int tourVersion = 1)
        : editor_(editor), pluginId_(std::move(stablePluginId)),
          version_(juce::jmax(1, tourVersion)) {
        editor_.addComponentListener(this);
    }
    ~GuidedTourController() override {
        editor_.removeComponentListener(this);
        overlay_.reset();
    }

    void setSteps(std::vector<GuidedTourStep> steps) {
        dismiss();
        steps_ = std::move(steps);
    }
    void setPalette(GuidedTourPalette palette) {
        palette_ = palette;
        if (overlay_) overlay_->setColours(palette_);
    }

    bool startIfFirstRun() {
        return GuidedTourPreferences::shouldShow(pluginId_, version_) && show();
    }
    bool replay() { return show(); }
    void dismiss() { overlay_.reset(); }
    bool isShowing() const noexcept { return overlay_ != nullptr && !overlay_->isClosing(); }
    GuidedTourOverlay* overlay() const noexcept { return overlay_.get(); }

private:
    bool show() {
        if (overlay_ != nullptr) return false;
        if (steps_.empty()) return false;
        overlay_ = std::make_unique<GuidedTourOverlay>(
            steps_, palette_, [this](GuidedTourFinish reason) {
                // Finishing or intentionally skipping counts as seen.
                // An unavailable/hidden target does NOT disable future tours.
                if (reason != GuidedTourFinish::Unavailable)
                    (void) GuidedTourPreferences::markSeen(pluginId_, version_);
                overlay_.reset();
            });
        editor_.addAndMakeVisible(*overlay_);
        overlay_->setBounds(editor_.getLocalBounds());
        overlay_->toFront(true);
        overlay_->begin();
        return overlay_ != nullptr && !overlay_->isClosing();
    }

    void componentMovedOrResized(juce::Component& component, bool, bool resized) override {
        if (&component == &editor_ && resized && overlay_ != nullptr)
            overlay_->setBounds(editor_.getLocalBounds());
    }

    juce::Component& editor_;
    juce::String pluginId_;
    int version_;
    GuidedTourPalette palette_;
    std::vector<GuidedTourStep> steps_;
    std::unique_ptr<GuidedTourOverlay> overlay_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuidedTourController)
};

} // namespace skellcore::ui
