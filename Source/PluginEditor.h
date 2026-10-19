#pragma once
#include "PluginProcessor.h"
#include "Ui.h"

// "About" card shown when the logo is clicked
class AboutOverlay : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
    std::function<juce::StringArray()> info;
    juce::Colour accent { 0xffff73b2 };
};

// The whole front panel is drawn by hand on a 1180 x 560 grid and scaled to the window.
// Below it: the ADVANCED strip and, when opened, the Advanced panel.
class BreatheEditor : public juce::AudioProcessorEditor, public juce::TooltipClient, private juce::Timer
{
public:
    explicit BreatheEditor (BreatheProcessor&);
    ~BreatheEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String getTooltip() override;

    void applyTheme (const juce::String& name);
    void renderForTest (double seconds);   // test helper: advance the animations without a message loop
    void hoverForTest (juce::Point<float> logical);
    void startTourForTest (int step) { welcome.setVisible (false); tour.start (lang); tour.showStep (step); }
    void hideWelcomeForTest() { welcome.setVisible (false); }

    static constexpr int baseW = 1180, baseH = 560, stripH = 34, advH = 360;
    void setAdvancedOpen (bool open);
    void showAboutForTest() { showAbout(); }
    void showDynForTest (int band) { advView = 1; dynBand = band; dynSticky = true; }

private:
    BreatheProcessor& proc;
    BLook look;
    TourOverlay tour;
    WelcomePrompt welcome;
    AboutOverlay about;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    Help::Lang lang = Help::Lang::en;
    bool helpOn = true, ready = false;

    struct KnobDef { const char* id; const char* name; float x, y, r; bool big; float def; };
    static constexpr int numKnobs = 8;    // 4 bands, MIX, AGE, OUTPUT, IN
    static const KnobDef knobDefs[numKnobs];
    juce::RangedAudioParameter* knobParam[numKnobs] {};
    juce::RangedAudioParameter* modeParam = nullptr;
    struct KnobCache { juce::Image base, glow; float px = 0.0f; juce::String theme; };
    KnobCache knobCache[numKnobs];
    juce::Image filGlow;
    float filGlowPx = 0.0f, physScale = 1.0f, shownInLevel = -100.0f;
    void buildKnobCache (int k, float px);
    juce::Colour inColour() const;
    // Advanced panel: 12 trims, index = band * 3 + kind (0 EQ, 1 DRIVE, 2 COMP/LIFT)
    bool advOpen = false;
    juce::RangedAudioParameter* advParam[12] {};
    int hoverAdv = -1, dragAdv = -1;
    float dragAdvStart = 0.0f, dragAdvAccum = 0.0f;
    int advView = 0, dynBand = 0;             // 0 = EQ screen, 1 = dynamics screen of dynBand
    bool dynSticky = false, dragThr = false;
    double dynHoldUntil = 0.0;
    std::vector<float> bandPre = std::vector<float> (1 << 17, 0.0f), bandPost = std::vector<float> (1 << 17, 0.0f);
    int bandPos = 0;
    std::array<float, 120> grHist {};
    int grPos = 0;
    std::array<float, 121> specPre {}, specPost {};
    float grShown = 0.0f, liftShown = 0.0f, waveScale = 0.01f;
    std::vector<float> waveHi = std::vector<float> (361, 0.0f), waveLo = std::vector<float> (361, 0.0f);
    void readBandScope();
    void drawEqScreen (juce::Graphics&, juce::Rectangle<float>);
    void drawCompScreen (juce::Graphics&, juce::Rectangle<float>, int band);
    void drawLiftScreen (juce::Graphics&, juce::Rectangle<float>, int band);
    void touchAdv (int index);
    float logicalH() const { return (float) (baseH + stripH + (advOpen ? advH : 0)); }
    float stripY() const { return logicalH() - (float) stripH; }
    juce::Point<float> advKnobPos (int i) const;
    int advKnobAt (juce::Point<float>) const;
    bool bandOn (int b) const;
    bool advActive() const;
    void setAdvValue (int i, float plain);
    void drawAdvStatic (juce::Graphics&);
    void drawAdvanced (juce::Graphics&);
    void drawStrip (juce::Graphics&);
    void showAbout();
    void applySizeLimits();

    juce::RangedAudioParameter *xoParam[3] {}, *soloParam[4] {}, *muteParam[4] {}, *ovsParam = nullptr, *linParam = nullptr, *bypassParam = nullptr;

    // cached static panel
    juce::Image panel, grainTile;
    float panelScale = 0.0f;
    juce::String panelTheme;
    void buildPanel (float physScale);

    // interaction
    juce::Point<float> mouse { -100, -100 };
    int hoverKnob = -1, dragKnob = -1, dragXo = -1, hoverBand = -1;
    float dragLastY = 0.0f;
    float smFade[4] {};

    // animation state
    double lastTick = 0.0, clock = 0.0;
    float heat = 0.0f, meterL[2] {}, peakL[2] {}, peakAge[2] {};
    std::vector<float> ring = std::vector<float> (4096, 0.0f);
    int ringPos = 0;
    juce::dsp::FFT fft { 12 };
    std::vector<float> fftBuf = std::vector<float> (8192, 0.0f);
    std::array<float, 161> spectrum {};
    juce::Random rnd;
    juce::String knobSignature;

    float scale() const { return (float) getWidth() / (float) baseW; }
    juce::Point<float> toLogical (juce::Point<float> p) const { return p / scale(); }
    juce::Rectangle<int> toPixels (juce::Rectangle<float> r) const { return (r * scale()).getSmallestIntegerContainer().expanded (2); }

    float value (int knobIndex) const;
    float xoFreq (int i) const;
    bool flag (juce::RangedAudioParameter* p) const { return p != nullptr && p->getValue() > 0.5f; }
    bool audible (int band) const;
    bool bypassed() const { return flag (bypassParam); }
    void setParam (juce::RangedAudioParameter*, float plainValue);
    void toggle (juce::RangedAudioParameter*);

    // geometry
    float fx (float f) const;
    float xf (float x) const;
    int bandAt (juce::Point<float>) const;
    juce::Point<float> smButton (int band, bool mute) const;
    int knobAt (juce::Point<float>) const;
    int xoAt (juce::Point<float>) const;
    juce::Rectangle<float> knobArea (int k) const;
    juce::String hitKey (juce::Point<float>) const;

    // drawing
    void drawKnob (juce::Graphics&, int k, bool hot);
    void drawTube (juce::Graphics&);
    void drawMeter (juce::Graphics&, float x, float level, float peak);
    void drawScreen (juce::Graphics&);
    void drawSwitchText (juce::Graphics&, const juce::String& text, juce::Point<float> c, bool lit);
    void drawJewel (juce::Graphics&, juce::Point<float> c, float r, bool on, juce::Colour col);

    void timerCallback() override;
    void tick (double dt);
    void showGearMenu();
    void showOvsMenu();
    void applyHelp();
    juce::Rectangle<float> tourTarget (const juce::String&);
    static juce::String loadSetting (const juce::String& key, const juce::String& fallback);
    static void saveSetting (const juce::String& key, const juce::String& value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreatheEditor)
};
