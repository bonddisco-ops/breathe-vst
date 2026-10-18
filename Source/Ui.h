#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Help.h"

juce::Font uiFont (float height, bool bold = false, float kerning = 0.0f);
inline const juce::Colour kInk (0xffececf0), kMute (0xffa5a5ad), kWidget (0xff1b1b1f);

class BLook : public juce::LookAndFeel_V4
{
public:
    BLook();
    void setTheme (const Theme&);
    const Theme& theme() const { return *current; }

    juce::Font getPopupMenuFont() override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

private:
    const Theme* current = nullptr;
};

const Theme& themeOf (const juce::Component&);

//==============================================================================
// Step-by-step tour: dims everything except the part being explained
class TourOverlay : public juce::Component
{
public:
    TourOverlay();
    void start (Help::Lang);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}
    std::function<juce::Rectangle<float> (const juce::String&)> resolveTarget;
    std::function<void()> onFinish;
    void showStep (int);
private:
    juce::Rectangle<float> calloutArea() const;
    juce::TextButton back, next, skip;
    int step = 0;
    Help::Lang lang = Help::Lang::en;
};

// First-run question: show the tour?
class WelcomePrompt : public juce::Component
{
public:
    WelcomePrompt();
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLang (Help::Lang);
    std::function<void (int)> onChoice;            // 0 yes, 1 not now, 2 never
    std::function<void (Help::Lang)> onLanguage;
private:
    juce::Rectangle<float> card() const;
    juce::TextButton yes, notNow, never, en { "English" }, ru;
    Help::Lang lang = Help::Lang::en;
};
