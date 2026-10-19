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

    static constexpr int baseW = 1180, baseH = 560, stripH = 34, advH = 360, headerH = 40, splitH = 250;
    void setSplit (bool on);
    void setSideForTest (int s) { side = advSide = s; remap(); }
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
    static constexpr int numKnobs = 12;   // 4 bands, MIX, AGE, OUTPUT, IN, then the 4 bands of the second side (split view)
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
    std::vector<float> bandPre = std::vector<float> (1 << 18, 0.0f), bandPost = std::vector<float> (1 << 18, 0.0f);
    int bandPos = 0;
    std::array<float, 120> grHist {};
    int grPos = 0;
    std::array<float, 121> specPre {}, specPost {};
    float grShown = 0.0f, liftShown = 0.0f, waveScale = 0.01f;
    float bandDynShown[4] {};                 // EQ screen: opto squeeze (LOW, LOW-MID, negative) / lift (HIGH-MID, HIGH, positive), dB
    std::vector<float> envPre = std::vector<float> (361, -120.0f), envPost = std::vector<float> (361, -120.0f);
    float shownLiftTop = -6.0f;
    std::vector<float> waveHi = std::vector<float> (361, 0.0f), waveLo = std::vector<float> (361, 0.0f);
    void readBandScope();
    void drawEqScreen (juce::Graphics&, juce::Rectangle<float>);
    void drawCompScreen (juce::Graphics&, juce::Rectangle<float>, int band);
    void drawLiftScreen (juce::Graphics&, juce::Rectangle<float>, int band);
    void touchAdv (int index);
    // logical layout, top to bottom: header | main panel (body y 0..560) | split row | ADVANCED panel | strip.
    // Body space starts under the header; "lower" space (ADVANCED, strip) is body space shifted up by the split row.
    bool split = false;
    float splitAnim = 0.0f;
    int prefW = 0;                 // width the user chose; the window gets narrower only while it would not fit the screen
    bool autoSizing = false;
    void fitWindow();                    // 0 = one row, 1 = two rows; eased in splitOff()
    int side = 0, advSide = 0;                 // 0 = Mid / Left, 1 = Side / Right
    juce::RangedAudioParameter *band1Param[4] {}, *band2Param[4] {}, *advA[12] {}, *advB[12] {};
    juce::RangedAudioParameter *procModeParam = nullptr, *linkParam = nullptr, *monitorParam = nullptr, *autoInParam = nullptr, *autoOutParam = nullptr;
    float splitEase() const { const float a = juce::jlimit (0.0f, 1.0f, splitAnim); return a * a * (3.0f - 2.0f * a); }
    float splitOff() const { return (float) splitH * splitEase(); }     // the lower part of the main panel moves down this much
    float tubeOff() const { return splitOff() * 0.5f; }                   // the tube block sits centred between the two knob rows
    float logicalH() const { return (float) (headerH + baseH + stripH + (advOpen ? advH : 0)) + ((split || splitAnim > 0.0f) ? (float) splitH : 0.0f); }
    float stripY() const { return (float) baseH + (advOpen ? (float) advH : 0.0f); }      // lower space
    juce::Point<float> lowerOf (juce::Point<float> p) const { return p.translated (0.0f, -splitOff()); }
    bool linked() const { return flag (linkParam); }
    int editSide() const { return linked() ? side : (split ? advSide : side); }
    bool sideBandOn (int s, int b) const;
    int telSide() const { return linked() ? 0 : editSide(); }      // whose meters the Advanced panel shows
    float editValue (int b) const { auto* p = editSide() == 0 ? band1Param[b] : band2Param[b]; return p->getValue(); }
    int shownProcMode = -1;
    void remap();
    void mirror (juce::RangedAudioParameter*);
    void copySideFrom (int s);
    int sideOf (juce::RangedAudioParameter*) const;
    juce::String headerKeyAt (juce::Point<float> headerSpace) const;
    void drawHeader (juce::Graphics&);
    void headerClick (const juce::String&);
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

    // The panel always fits the window the host actually gave us: uniform scale, centred.
    // (Some hosts refuse or delay a resize request, e.g. when the Advanced panel opens.)
    float scale() const { return juce::jmax (0.05f, juce::jmin ((float) getWidth() / (float) baseW, (float) getHeight() / logicalH())); }
    juce::Point<float> origin() const { const float s = scale(); return { ((float) getWidth() - (float) baseW * s) * 0.5f, ((float) getHeight() - logicalH() * s) * 0.5f + (float) headerH * s }; }
    // body space -> screen (the header sits at body y -40..0)
    juce::AffineTransform toScreen() const { const auto o = origin(); return juce::AffineTransform::scale (scale()).translated (o.x, o.y); }
    juce::Point<float> toLogical (juce::Point<float> p) const { return (p - origin()) / scale(); }
    juce::Rectangle<int> toPixels (juce::Rectangle<float> r) const { return (r * scale() + origin()).getSmallestIntegerContainer().expanded (2); }
    juce::Rectangle<int> toPixelsLower (juce::Rectangle<float> r) const { return toPixels (r.translated (0.0f, splitOff())); }

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
