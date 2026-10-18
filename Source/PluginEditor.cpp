#include "PluginEditor.h"
#include "Logo.h"

using namespace juce;

namespace
{
constexpr float W = (float) BreatheEditor::baseW, H = (float) BreatheEditor::baseH, cx = 590.0f;
constexpr float pi = MathConstants<float>::pi;
const Rectangle<float> tubeWin { cx - 58.0f, 134.0f, 116.0f, 192.0f };
const Rectangle<float> scr { 300.0f, 392.0f, 580.0f, 124.0f };
const Point<float> gearC { 1095.0f, 46.0f }, bypassC { 1066.0f, 386.0f }, ovsC { cx - 96.0f, 353.0f }, linC { cx + 96.0f, 353.0f },
                   masterC { cx - 24.0f, 353.0f }, mixC { cx + 32.0f, 353.0f };
constexpr float meterTop = 138.0f, meterBot = 320.0f;
const float lightAngle = std::atan2 (-1.0f, -0.7f);

float meterY (float db) { return meterTop + (1.0f - (db + 48.0f) / 54.0f) * (meterBot - meterTop); }
float dbToFrac (float g) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (g, -100.0f) + 48.0f) / 54.0f); }

String freqShort (float f)
{
    return f >= 999.5f ? String (f / 1000.0f, f < 9950.0f ? 1 : 0) + "k" : String (roundToInt (f));
}

Colour panelColour (const Theme& t)
{
    if (t.name == "Mango") return Colour (0xffeda62e);
    if (t.name == "Rage") return Colour (0xff1d1515);
    if (t.name == "Ultraviolet") return Colour (0xff2c1948);
    if (t.name == "Luxe") return Colour (0xff121214);
    return Colour (0xff24252a);
}

Colour mixWith (Colour c, float k)   // k < 0 darker, k > 0 lighter
{
    return k < 0 ? c.interpolatedWith (Colours::black, -k) : c.interpolatedWith (Colours::white, k);
}

void engrave (Graphics& g, const Theme& t, const String& s, float x, float y, const Font& f, Justification j, Colour c, float w = 300.0f)
{
    g.setFont (f);
    Rectangle<float> r (x, y - 10.0f, w, 20.0f);
    if (j.testFlags (Justification::horizontallyCentred)) r.setX (x - w * 0.5f);
    else if (j.testFlags (Justification::right)) r.setX (x - w);
    g.setColour (t.lightBg ? Colours::white.withAlpha (0.4f) : Colours::black.withAlpha (0.75f));
    g.drawText (s, r.translated (0.0f, 1.0f), j, false);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

void screw (Graphics& g, float x, float y, Random& r)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (x - 7.5f, y - 6.0f, 17.0f, 17.0f);
    ColourGradient gr (Colour (0xffd4d4da), x - 2.5f, y - 2.5f, Colour (0xff1d1d21), x + 6.0f, y + 6.0f, true);
    gr.addColour (0.55, Colour (0xff5d5e64));
    g.setGradientFill (gr);
    g.fillEllipse (x - 7.5f, y - 7.5f, 15.0f, 15.0f);
    const float a = r.nextFloat() * pi;
    g.setColour (Colours::black.withAlpha (0.75f));
    for (float d : { 0.0f, pi * 0.5f })
        g.drawLine (x - std::cos (a + d) * 5, y - std::sin (a + d) * 5, x + std::cos (a + d) * 5, y + std::sin (a + d) * 5, 2.0f);
}

Path gorilla (float cxp, float cyp, float scale, float angle)
{
    Path p (Logo::path());
    p.applyTransform (AffineTransform::translation (-500.0f, -432.0f).scaled (scale).rotated (angle).translated (cxp, cyp));
    return p;
}

void glowStroke (Graphics& g, const Path& p, Colour c, float w, float glow)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withMultipliedAlpha (glow * 0.10f * (float) (4 - i)));
        g.strokePath (p, PathStrokeType (w + (float) i * 4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
}
}

const BreatheEditor::KnobDef BreatheEditor::knobDefs[BreatheEditor::numKnobs] = {
    { Params::band[0], "LOW", 172, 205, 58, true, 0.0f },
    { Params::band[1], "LOW-MID", 356, 205, 58, true, 0.0f },
    { Params::band[2], "HIGH-MID", 824, 205, 58, true, 0.0f },
    { Params::band[3], "HIGH", 1008, 205, 58, true, 0.0f },
    { Params::mix, "MIX", 974, 452, 24, false, 1.0f },
    { Params::age, "AGE", 208, 452, 24, false, 0.4f },
    { Params::output, "OUTPUT", 1066, 452, 24, false, 0.0f },
    { Params::input, "IN", 112, 452, 24, false, 0.0f },
};

//==============================================================================
namespace
{
    bool askedThisRun = false;
    PropertiesFile::Options settingsOptions()
    {
        PropertiesFile::Options o;
        o.applicationName = "Breathe";
        o.folderName = "Primal Rage Lab";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 0;
        return o;
    }
}

String BreatheEditor::loadSetting (const String& key, const String& fallback)
{
    PropertiesFile f (settingsOptions());
    return f.getValue (key, fallback);
}

void BreatheEditor::saveSetting (const String& key, const String& v)
{
    PropertiesFile f (settingsOptions());
    f.setValue (key, v);
    f.saveIfNeeded();
}

BreatheEditor::BreatheEditor (BreatheProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int k = 0; k < numKnobs; ++k) knobParam[k] = proc.apvts.getParameter (knobDefs[k].id);
    for (int i = 0; i < 3; ++i) xoParam[i] = proc.apvts.getParameter (Params::xover[i]);
    for (int b = 0; b < 4; ++b) { soloParam[b] = proc.apvts.getParameter (Params::solo[b]); muteParam[b] = proc.apvts.getParameter (Params::mute[b]); }
    ovsParam = proc.apvts.getParameter (Params::ovs);
    linParam = proc.apvts.getParameter (Params::lin);
    bypassParam = proc.apvts.getParameter (Params::bypass);
    modeParam = proc.apvts.getParameter (Params::mode);

    const auto themeName = proc.apvts.state.getProperty ("theme", loadSetting ("theme", "Asphalt")).toString();
    look.setTheme (Theme::byName (themeName));
    helpOn = loadSetting ("help", "on") != "off";
    lang = loadSetting ("helpLang", "en") == "ru" ? Help::Lang::ru : Help::Lang::en;
    setLookAndFeel (&look);

    addChildComponent (tour);
    addChildComponent (welcome);
    tour.resolveTarget = [this] (const String& n) { return tourTarget (n); };
    tour.onFinish = [] { saveSetting ("tour", "never"); };
    welcome.onLanguage = [this] (Help::Lang l) { lang = l; saveSetting ("helpLang", l == Help::Lang::ru ? "ru" : "en"); applyHelp(); };
    welcome.onChoice = [this] (int choice)
    {
        welcome.setVisible (false);
        if (choice == 0) tour.start (lang);
        else if (choice == 2) saveSetting ("tour", "never");
    };
    welcome.setLang (lang);
    applyHelp();

    // grain texture for the metal
    grainTile = Image (Image::ARGB, 256, 256, true);
    {
        Random r (77);
        Image::BitmapData bd (grainTile, Image::BitmapData::writeOnly);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const float v = r.nextFloat();
                bd.setPixelColour (x, y, (v > 0.5f ? Colours::white : Colours::black).withAlpha ((float) std::abs (v - 0.5f) * 0.11f));
            }
    }

    const int w = (int) proc.apvts.state.getProperty ("uiW", loadSetting ("w", String (baseW)).getIntValue());
    setResizable (true, true);
    setResizeLimits (944, 448, 2360, 1120);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    ready = true;
    const int ww = jlimit (944, 2360, w);
    setSize (ww, roundToInt ((float) ww * H / W));

    lastTick = Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (40);

    if (loadSetting ("tour", "") != "never" && ! askedThisRun)
    {
        askedThisRun = true;
        welcome.setVisible (true);
        welcome.toFront (false);
    }
}

BreatheEditor::~BreatheEditor()
{
    setLookAndFeel (nullptr);
}

void BreatheEditor::applyHelp()
{
    if (helpOn && tooltipWindow == nullptr)
        tooltipWindow = std::make_unique<TooltipWindow> (this, 700);
    else if (! helpOn)
        tooltipWindow.reset();
}

void BreatheEditor::applyTheme (const String& name)
{
    proc.apvts.state.setProperty ("theme", name, nullptr);
    saveSetting ("theme", name);
    look.setTheme (Theme::byName (name));
    panel = {};
    sendLookAndFeelChange();
    welcome.setLang (lang);
    repaint();
}

void BreatheEditor::resized()
{
    if (ready)
    {
        proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
        saveSetting ("w", String (getWidth()));
    }
    tour.setBounds (getLocalBounds());
    welcome.setBounds (getLocalBounds());
    if (tour.isVisible()) tour.resized();
    panel = {};
}

//==============================================================================
float BreatheEditor::value (int k) const
{
    auto* p = knobParam[k];
    return p != nullptr ? p->getValue() : 0.0f;   // normalised 0..1
}

float BreatheEditor::xoFreq (int i) const
{
    return xoParam[i]->convertFrom0to1 (xoParam[i]->getValue());
}

bool BreatheEditor::audible (int b) const
{
    bool anySolo = false;
    for (auto* s : soloParam) anySolo |= flag (s);
    return anySolo ? flag (soloParam[b]) : ! flag (muteParam[b]);
}

void BreatheEditor::setParam (RangedAudioParameter* p, float plain)
{
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void BreatheEditor::toggle (RangedAudioParameter* p)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    p->endChangeGesture();
}

float BreatheEditor::fx (float f) const { return scr.getX() + std::log (f / 20.0f) / std::log (1000.0f) * scr.getWidth(); }
float BreatheEditor::xf (float x) const { return 20.0f * std::pow (1000.0f, (x - scr.getX()) / scr.getWidth()); }

int BreatheEditor::bandAt (Point<float> p) const
{
    if (! scr.contains (p)) return -1;
    const float f = xf (p.x);
    for (int i = 0; i < 3; ++i)
        if (f < xoFreq (i)) return i;
    return 3;
}

Point<float> BreatheEditor::smButton (int b, bool isMute) const
{
    const float edges[5] { 20.0f, xoFreq (0), xoFreq (1), xoFreq (2), 20000.0f };
    const float x0 = fx (edges[b]), x1 = fx (edges[b + 1]), mx = (x0 + x1) * 0.5f;
    const float gap = jmin (17.0f, (x1 - x0) * 0.25f);
    return { isMute ? mx + gap : mx - gap, scr.getCentreY() };
}

int BreatheEditor::knobAt (Point<float> p) const
{
    for (int k = 0; k < numKnobs; ++k)
        if (p.getDistanceFrom ({ knobDefs[k].x, knobDefs[k].y }) < knobDefs[k].r + 8.0f) return k;
    return -1;
}

int BreatheEditor::xoAt (Point<float> p) const
{
    if (! scr.contains (p)) return -1;
    for (int i = 0; i < 3; ++i)
        if (std::abs (fx (xoFreq (i)) - p.x) < 8.0f) return i;
    return -1;
}

Rectangle<float> BreatheEditor::knobArea (int k) const
{
    const auto& d = knobDefs[k];
    const float R = d.r + (d.big ? 32.0f : 18.0f);
    return Rectangle<float> (d.x - R - 12.0f, d.y - R - (d.big ? 26.0f : 36.0f), (R + 12.0f) * 2.0f, R * 2.0f + (d.big ? 160.0f : 70.0f));
}

String BreatheEditor::hitKey (Point<float> p) const
{
    const int k = knobAt (p);
    if (k >= 0) return k < 4 ? Params::band[k] : k == 4 ? "mix" : k == 5 ? "age" : k == 6 ? "output" : "in";
    const int b = bandAt (p);
    if (b >= 0)
    {
        if (p.getDistanceFrom (smButton (b, false)) < 14.0f) return "solo";
        if (p.getDistanceFrom (smButton (b, true)) < 14.0f) return "mute";
        return "screen";
    }
    if (p.getDistanceFrom (gearC) < 20.0f) return "gear";
    if (p.getDistanceFrom (bypassC) < 18.0f) return "bypass";
    if (Rectangle<float> (ovsC.x - 34, ovsC.y - 12, 68, 24).contains (p)) return "ovs";
    if (Rectangle<float> (linC.x - 26, linC.y - 12, 52, 24).contains (p)) return "lin";
    if (Rectangle<float> (masterC.x - 30, masterC.y - 12, 58, 24).contains (p)) return "master";
    if (Rectangle<float> (mixC.x - 22, mixC.y - 12, 44, 24).contains (p)) return "mixMode";
    if (tubeWin.expanded (12.0f).contains (p)) return "tube";
    if (Rectangle<float> (cx - 112, meterTop - 8, 34, meterBot - meterTop + 16).contains (p)) return "inMeter";
    if (Rectangle<float> (cx + 78, meterTop - 8, 34, meterBot - meterTop + 16).contains (p)) return "outMeter";
    return {};
}

String BreatheEditor::getTooltip()
{
    if (! helpOn || tour.isVisible() || welcome.isVisible() || dragKnob >= 0 || dragXo >= 0) return {};
    const auto key = hitKey (mouse);
    if (key == "master" || key == "mixMode") return Help::tip ("mode", lang);
    return key.isEmpty() ? String() : Help::tip (key.toRawUTF8(), lang);
}

Rectangle<float> BreatheEditor::tourTarget (const String& n)
{
    const float s = scale();
    auto knobRect = [] (int k) { const auto& d = knobDefs[k]; return Rectangle<float> (d.x - d.r - 30, d.y - d.r - 30, (d.r + 30) * 2, (d.r + 30) * 2 + 60); };
    Rectangle<float> r;
    if (n == "knobs") r = knobRect (0).getUnion (knobRect (1)).getUnion (knobRect (2)).getUnion (knobRect (3));
    else if (n == Params::band[0]) r = knobRect (0);
    else if (n == Params::band[1]) r = knobRect (1);
    else if (n == Params::band[2]) r = knobRect (2);
    else if (n == Params::band[3]) r = knobRect (3);
    else if (n == "screen") r = scr.expanded (6.0f);
    else if (n == "tube") r = tubeWin.expanded (14.0f).withTop (90.0f);
    else if (n == "ovs") r = Rectangle<float> (ovsC.x - 38, ovsC.y - 14, 76, 28);
    else if (n == "lin") r = Rectangle<float> (linC.x - 30, linC.y - 14, 60, 28);
    else if (n == "mix") r = Rectangle<float> (knobDefs[4].x - 40, knobDefs[4].y - 40, 80, 100);
    else if (n == "age") r = Rectangle<float> (knobDefs[5].x - 40, knobDefs[5].y - 40, 80, 100);
    else if (n == "output") r = Rectangle<float> (knobDefs[6].x - 40, knobDefs[6].y - 40, 80, 100);
    else if (n == "in") r = Rectangle<float> (knobDefs[7].x - 40, knobDefs[7].y - 40, 80, 100);
    else if (n == "mode") r = Rectangle<float> (masterC.x - 34, masterC.y - 14, 104, 28);
    else if (n == "bypass") r = Rectangle<float> (bypassC.x - 30, bypassC.y - 24, 60, 64);
    else if (n == "gear") r = Rectangle<float> (gearC.x - 24, gearC.y - 24, 48, 48);
    return r * s;
}

//==============================================================================
void BreatheEditor::buildPanel (float phys)
{
    const auto& t = look.theme();
    const float s = scale();
    panel = Image (Image::RGB, jmax (1, roundToInt ((float) getWidth() * phys)), jmax (1, roundToInt ((float) getHeight() * phys)), true);
    panelScale = phys;
    panelTheme = t.name;
    Graphics g (panel);
    g.addTransform (AffineTransform::scale (s * phys));

    const Colour pc = panelColour (t);
    Random r (4321);

    // rack ears and body with an overhead light
    g.setColour (mixWith (pc, -0.4f));
    g.fillAll();
    g.setGradientFill (ColourGradient (mixWith (pc, -0.1f), 0, 0, mixWith (pc, -0.5f), 0, H, false));
    g.fillRect (0.0f, 0.0f, 34.0f, H);
    g.fillRect (W - 34.0f, 0.0f, 34.0f, H);
    {
        ColourGradient body (mixWith (pc, 0.14f), 0, 0, mixWith (pc, -0.28f), 0, H, false);
        body.addColour (0.35, mixWith (pc, 0.03f));
        g.setGradientFill (body);
        g.fillRect (34.0f, 0.0f, W - 68.0f, H);
    }
    g.setTiledImageFill (grainTile, 0, 0, 1.0f);
    g.fillRect (34.0f, 0.0f, W - 68.0f, H);
    // brushing
    for (int i = 0; i < 1100; ++i)
    {
        const float y = r.nextFloat() * H, x0 = 34.0f + r.nextFloat() * W, len = 80.0f + r.nextFloat() * 320.0f;
        g.setColour ((r.nextFloat() < 0.55f ? Colours::white : Colours::black).withAlpha (t.lightBg ? 0.06f : 0.05f));
        g.drawLine (x0, y, jmin (W - 34.0f, x0 + len), y, 0.2f + r.nextFloat() * 0.8f);
    }
    if (! t.lightBg)
        for (int i = 0; i < 320; ++i)
        {
            g.setColour (Colours::white.withAlpha (r.nextFloat() * 0.1f));
            g.fillRect (34.0f + r.nextFloat() * (W - 68.0f), r.nextFloat() * H, 1.0f, 1.0f);
        }
    {
        ColourGradient hot (Colours::white.withAlpha (t.lightBg ? 0.10f : 0.13f), cx, -60.0f, Colours::white.withAlpha (0.0f), cx, 560.0f, true);
        g.setGradientFill (hot);
        g.fillRect (34.0f, 0.0f, W - 68.0f, H);
        ColourGradient band (Colours::white.withAlpha (0.0f), 0, 0, Colours::white.withAlpha (0.0f), 0, H, false);
        band.addColour (0.16, Colours::white.withAlpha (0.06f));
        band.addColour (0.24, Colours::white.withAlpha (0.0f));
        g.setGradientFill (band);
        g.fillRect (34.0f, 0.0f, W - 68.0f, H);
        ColourGradient vig (Colours::transparentBlack, cx, H * 0.4f, t.lightBg ? Colour ((uint8) 90, 40, 0, 0.3f) : Colours::black.withAlpha (0.5f), cx + 780.0f, H * 0.5f, true);
        g.setGradientFill (vig);
        g.fillRect (34.0f, 0.0f, W - 68.0f, H);
    }
    // bevels, ears
    g.setColour (Colours::white.withAlpha (0.16f)); g.fillRect (34.0f, 0.0f, W - 68.0f, 1.5f);
    g.setColour (Colours::black.withAlpha (0.6f));  g.fillRect (34.0f, H - 2.0f, W - 68.0f, 2.0f);
    g.setColour (Colours::white.withAlpha (0.07f)); g.fillRect (35.0f, 0.0f, 1.0f, H);
    g.setColour (Colours::black.withAlpha (0.7f));  g.fillRect (W - 35.0f, 0.0f, 1.5f, H); g.fillRect (33.0f, 0.0f, 1.5f, H);
    for (auto pt : { Point<float> (6, 60), Point<float> (6, H - 104), Point<float> (W - 28, 60), Point<float> (W - 28, H - 104) })
    {
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (pt.x + 4, pt.y, 14, 44, 7);
        g.setColour (Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (pt.x + 4, pt.y, 14, 44, 7, 1.0f);
    }
    for (auto pt : { Point<float> (58, 22), Point<float> (W - 58, 22), Point<float> (58, H - 22), Point<float> (W - 58, H - 22), Point<float> (cx - 170, H - 22), Point<float> (cx + 170, H - 22) })
        screw (g, pt.x, pt.y, r);

    // header: logo, name, label
    {
        const auto la = Rectangle<float> (66.0f, 20.0f, 52.0f, 54.0f);
        const auto tr = RectanglePlacement (RectanglePlacement::centred).getTransformToFit (Logo::viewBox, la);
        Path lp (Logo::path());
        lp.applyTransform (tr);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillPath (lp, AffineTransform::translation (0.0f, 1.3f));
        DropShadow (t.logo.withAlpha (0.55f), 12, {}).drawForPath (g, lp);
        if (t.gloss)
        {
            ColourGradient gold (Colour (0xfff6dc9a), la.getX(), la.getY(), Colour (0xffa47a2c), la.getX(), la.getBottom(), false);
            gold.addColour (0.55, Colour (0xffe8c46a));
            g.setGradientFill (gold);
        }
        else
            g.setColour (t.logo);
        g.fillPath (lp);
        g.setColour (t.lightBg ? Colour ((uint8) 255, 240, 210, 0.5f) : Colours::white.withAlpha (0.2f));
        g.strokePath (lp, PathStrokeType (0.45f));
    }
    const Colour nameCol = t.lightBg ? Colour (0xff1e1300) : Colours::white;
    g.setFont (uiFont (32.0f, true, 0.04f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawText ("BREATHE", Rectangle<float> (128, 23, 400, 34), Justification::centredLeft);
    g.setColour (nameCol);
    g.drawText ("BREATHE", Rectangle<float> (128, 20, 400, 34), Justification::centredLeft);
    const Font labelFont = uiFont (10.5f, true, 0.28f);
    engrave (g, t, "PRIMAL RAGE LAB", 130, 64, labelFont, Justification::centredLeft, t.mute);

    // "ANALOG COLOR / PROCESSOR" plate above the tube: both lines exactly the same width
    {
        // line 1 with the house letter-spacing; line 2 spread letter by letter so that
        // its first and last letters sit exactly under the first and last of line 1
        GlyphArrangement a1;
        a1.addLineOfText (labelFont, "ANALOG COLOR", 0.0f, 0.0f);
        const float ink1 = a1.getGlyph (a1.getNumGlyphs() - 1).getRight() - a1.getGlyph (0).getLeft() - labelFont.getHeight() * 0.28f;
        a1.moveRangeOfGlyphs (0, -1, cx - ink1 * 0.5f - a1.getGlyph (0).getLeft(), 101.5f);

        const Font plain = uiFont (10.5f, true);
        const String l2 = "PROCESSOR";
        float widths[16] {}, sum = 0.0f;
        for (int i = 0; i < l2.length(); ++i) { widths[i] = GlyphArrangement::getStringWidth (plain, l2.substring (i, i + 1)); sum += widths[i]; }
        const float gap = (ink1 - sum) / (float) (l2.length() - 1);
        GlyphArrangement a2;
        float xx = cx - ink1 * 0.5f;
        for (int i = 0; i < l2.length(); ++i)
        {
            a2.addLineOfText (plain, l2.substring (i, i + 1), xx, 116.5f);
            xx += widths[i] + gap;
        }
        for (auto* a : { &a1, &a2 })
        {
            g.setColour (t.lightBg ? Colours::white.withAlpha (0.4f) : Colours::black.withAlpha (0.75f));
            a->draw (g, AffineTransform::translation (0.0f, 1.0f));
            g.setColour (nameCol);
            a->draw (g);
        }
    }

    // centre module groove
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (cx - 140, 80, 280, 298, 18, 2.0f);
    g.setColour (t.lightBg ? Colours::white.withAlpha (0.4f) : Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (cx - 139, 81.5f, 280, 298, 18, 1.0f);

    // tube window bezel
    {
        const auto b = tubeWin.expanded (12.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (b.translated (0, 4), 32);
        ColourGradient gr (Colour (0xff4a4a52), 0, b.getY(), Colour (0xff33333a), 0, b.getBottom(), false);
        gr.addColour (0.12, Colour (0xff1f1f24));
        gr.addColour (0.88, Colour (0xff141417));
        g.setGradientFill (gr);
        g.fillRoundedRectangle (b, 32);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (b.reduced (0.5f), 31, 1.0f);
    }

    // meter labels and scale
    const Font small = uiFont (9.0f), bold10 = uiFont (10.0f, true);
    engrave (g, t, "IN", cx - 96, 126, bold10, Justification::centred, t.mute, 40);
    engrave (g, t, "OUT", cx + 96, 126, bold10, Justification::centred, t.mute, 40);
    for (int db : { 6, 0, -6, -12, -24, -48 })
    {
        const String s2 = db > 0 ? "+" + String (db) : String (db);
        engrave (g, t, s2, cx - 114, meterY ((float) db), small, Justification::centredRight, t.mute, 30);
        engrave (g, t, s2, cx + 114, meterY ((float) db), small, Justification::centredLeft, t.mute, 30);
    }

    // knob scales, grooves and names
    for (int k = 0; k < numKnobs; ++k)
    {
        const auto& d = knobDefs[k];
        const int n = d.big ? 11 : 7;
        const float R = d.r + (d.big ? 27.0f : 15.0f);
        g.setColour (t.lightBg ? Colour ((uint8) 40, 20, 0, 0.6f) : Colours::white.withAlpha (0.45f));
        for (int i = 0; i < n; ++i)
        {
            const float a = -pi * 0.75f + (float) i / (float) (n - 1) * pi * 1.5f - pi * 0.5f;
            const float rr = d.big ? 2.2f : 1.5f;
            g.fillEllipse (d.x + std::cos (a) * R - rr, d.y + std::sin (a) * R - rr, rr * 2, rr * 2);
        }
        const float RG = d.r + (d.big ? 15.0f : 8.0f);
        Path groove;
        groove.addCentredArc (d.x, d.y, RG, RG, 0.0f, -pi * 0.75f, pi * 0.75f, true);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.strokePath (groove, PathStrokeType (d.big ? 9.0f : 5.0f));
        Path lip;
        lip.addCentredArc (d.x, d.y, RG + 5.0f, RG + 5.0f, 0.0f, -pi * 0.75f, pi * 0.75f, true);
        g.setColour (t.lightBg ? Colours::white.withAlpha (0.35f) : Colours::white.withAlpha (0.07f));
        g.strokePath (lip, PathStrokeType (1.0f));
        if (d.big)
        {
            engrave (g, t, "0", d.x - R * 0.8f, d.y + R * 0.86f, uiFont (10.0f), Justification::centred, t.mute, 40);
            engrave (g, t, "MAX", d.x + R * 0.8f, d.y + R * 0.86f, uiFont (10.0f), Justification::centred, t.mute, 40);
            engrave (g, t, d.name, d.x, d.y + 112, uiFont (15.0f, true, 0.03f), Justification::centred, t.ink, 200);
        }
        else
            engrave (g, t, d.name, d.x, d.y + 46, uiFont (10.0f, true, 0.05f), Justification::centred, t.ink, 100);
    }
    engrave (g, t, "BYPASS", bypassC.x, bypassC.y - 27, uiFont (9.0f, true, 0.05f), Justification::centred, t.mute, 100);

    // screen bezel
    {
        const auto b = scr.expanded (9.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (b.translated (0, 4), 15);
        ColourGradient gr (Colour (0xff3a3a41), 0, b.getY(), Colour (0xff101012), 0, b.getBottom(), false);
        gr.addColour (0.1, Colour (0xff1c1c20));
        g.setGradientFill (gr);
        g.fillRoundedRectangle (b, 15);
        g.setColour (Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (b.reduced (0.5f), 14, 1.0f);
    }

    // gear: a small knurled metal button with an engraved gear
    {
        const float x = gearC.x, y = gearC.y, rr = 17.0f, dd = 5.0f;
        DropShadow (Colours::black.withAlpha (0.65f), 6, { 1, 5 }).drawForPath (g, [&] { Path p; p.addEllipse (x - rr, y - rr, rr * 2, rr * 2); return p; }());
        ColourGradient side (Colour (0xff0a0a0c), x - rr, 0, Colour (0xff060607), x + rr, 0, false);
        side.addColour (0.25, Colour (0xff45454c));
        side.addColour (0.5, Colour (0xff1e1e22));
        g.setGradientFill (side);
        g.fillRect (x - rr, y, rr * 2, dd);
        g.fillEllipse (x - rr, y + dd - rr, rr * 2, rr * 2);
        for (int i = 0; i < 28; ++i)
        {
            const float ph = (float) i * pi * 2.0f / 28.0f, sn = std::sin (ph);
            if (sn <= 0.05f) continue;
            const float px = x + std::cos (ph) * rr, sq = std::sqrt (jmax (0.0f, rr * rr - (px - x) * (px - x)));
            g.setColour (Colours::white.withAlpha (0.04f + 0.3f * std::pow (jmax (0.0f, -std::cos (ph) * 0.8f + 0.35f) * sn, 2.0f)));
            g.drawLine (px, y + sq, px, y + dd + sq - 1.0f, 1.1f * sn);
        }
        ColourGradient top (Colour (0xff6a6b73), x - rr * 0.5f, y - rr, Colour (0xff0d0d10), x + rr * 0.5f, y + rr, false);
        top.addColour (0.5, Colour (0xff26262b));
        g.setGradientFill (top);
        g.fillEllipse (x - rr, y - rr, rr * 2, rr * 2);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (x - rr + 0.8f, y - rr + 0.8f, rr * 2 - 1.6f, rr * 2 - 1.6f, 0.8f);
        Path gp;
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * pi / 4.0f;
            gp.addRectangle (-1.8f, -9.5f, 3.6f, 4.0f);
            gp.applyTransform (AffineTransform::rotation (pi / 4.0f));
            ignoreUnused (a);
        }
        gp.addEllipse (-6.5f, -6.5f, 13.0f, 13.0f);
        Path hole;
        hole.addEllipse (-2.6f, -2.6f, 5.2f, 5.2f);
        gp.addPath (hole);
        gp.setUsingNonZeroWinding (false);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.fillPath (gp, AffineTransform::translation (x + 0.6f, y + 0.9f));
        g.setColour (Colour (0xff09090b));
        g.fillPath (gp, AffineTransform::translation (x, y));
    }
}

//==============================================================================
void BreatheEditor::buildKnobCache (int k, float px)
{
    // the parts of a knob that never move: contact shadow, side wall, top face, bevel and dome
    const auto& d = knobDefs[k];
    auto& c = knobCache[k];
    const float r = d.r, dd = r * (d.big ? 0.24f : 0.28f), cap = r * 0.8f;
    const float half = r + 14.0f;
    const int sz = roundToInt (half * 2.0f * px);
    c.base = Image (Image::ARGB, sz, roundToInt ((half * 2.0f + dd + 8.0f) * px), true);
    c.px = px;
    c.theme = look.theme().name;
    Graphics g (c.base);
    g.addTransform (AffineTransform::scale (px));
    const float x = half, y = half;
    {
        Path sh;
        sh.addEllipse (x + r * 0.08f - r * 1.02f, y + dd + r * 0.12f - r * 0.98f, r * 2.04f, r * 1.96f);
        DropShadow (Colours::black.withAlpha (0.7f), roundToInt (r * 0.25f * px), {}).drawForPath (g, sh);
    }
    ColourGradient wall (Colour (0xff08080a), x - r, 0, Colour (0xff050506), x + r, 0, false);
    wall.addColour (0.22, Colour (0xff3c3c43));
    wall.addColour (0.42, Colour (0xff1c1c20));
    g.setGradientFill (wall);
    g.fillRect (x - r, y, r * 2.0f, dd);
    g.fillEllipse (x - r, y + dd - r, r * 2.0f, r * 2.0f);
    Path lower;
    lower.addCentredArc (x, y + dd, r, r, 0.0f, pi * 0.5f, pi * 1.5f, true);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.strokePath (lower, PathStrokeType (1.2f));
    ColourGradient top (Colour (0xff4a4a52), x - r * 0.6f, y - r, Colour (0xff09090b), x + r * 0.6f, y + r, false);
    top.addColour (0.5, Colour (0xff1b1b1f));
    g.setGradientFill (top);
    g.fillEllipse (x - r, y - r, r * 2, r * 2);
    ColourGradient ch (Colours::white.withAlpha (0.55f), x, y - r, Colours::black.withAlpha (0.5f), x, y + r, false);
    ch.addColour (0.5, Colours::white.withAlpha (0.05f));
    g.setGradientFill (ch);
    Path rim;
    rim.addEllipse (x - r + 0.8f, y - r + 0.8f, r * 2 - 1.6f, r * 2 - 1.6f);
    g.strokePath (rim, PathStrokeType (1.6f));
    ColourGradient bev (Colours::black.withAlpha (0.65f), x, y - cap, Colours::white.withAlpha (0.18f), x, y + cap, false);
    g.setGradientFill (bev);
    g.fillEllipse (x - cap - 2, y - cap - 2, cap * 2 + 4, cap * 2 + 4);
    ColourGradient dome (Colour (0xff62636c), x - cap * 0.32f, y - cap * 0.42f, Colour (0xff060607), x - cap * 0.32f + cap * 1.45f, y - cap * 0.42f, true);
    dome.addColour (0.42, Colour (0xff222227));
    g.setGradientFill (dome);
    g.fillEllipse (x - cap, y - cap, cap * 2, cap * 2);

    // soft glow of the engraved gorilla, white; tinted and rotated when drawn
    if (d.big)
    {
        const float sc = cap * 1.02f / 534.0f;
        const float gh = cap + 18.0f;
        c.glow = Image (Image::ARGB, roundToInt (gh * 2.0f * px), roundToInt (gh * 2.0f * px), true);
        Graphics gg (c.glow);
        gg.addTransform (AffineTransform::scale (px));
        DropShadow (Colours::white, roundToInt (12.0f * px), {}).drawForPath (gg, gorilla (gh, gh, sc, 0.0f));
    }
}

void BreatheEditor::drawKnob (Graphics& g, int k, bool hot)
{
    const auto& d = knobDefs[k];
    const auto& t = look.theme();
    const float x = d.x, y = d.y, r = d.r, v = value (k);
    const float a = -pi * 0.75f + v * pi * 1.5f;
    const bool byp = bypassed();
    const float on = byp ? 0.25f : 1.0f, dim = (k < 4 && ! audible (k)) ? 0.35f : 1.0f;
    const float dd = r * (d.big ? 0.24f : 0.28f), cap = r * 0.8f;
    const bool bipolar = k == 6 || k == 7;
    const Colour acc = k == 7 ? inColour() : t.accent;
    const float px = scale() * physScale;
    auto& c = knobCache[k];
    if (! c.base.isValid() || c.px != px || c.theme != t.name)
        buildKnobCache (k, px);

    // light ring
    const float R = r + (d.big ? 15.0f : 8.0f);
    {
        Path full;
        full.addCentredArc (x, y, R, R, 0.0f, -pi * 0.75f, pi * 0.75f, true);
        g.setColour (acc.withAlpha (0.10f));
        g.strokePath (full, PathStrokeType (d.big ? 4.0f : 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        const float from = bipolar ? 0.0f : -pi * 0.75f, to = bipolar ? (v - 0.5f) * pi * 1.5f : -pi * 0.75f + v * pi * 1.5f;
        const float amt = bipolar ? (k == 7 ? 1.0f : std::abs (v - 0.5f) * 2.0f) : v;
        if (k == 7)
        {
            // IN: the whole ring is the level light
            glowStroke (g, full, acc.withAlpha (0.9f * on), 2.6f, 0.9f * on);
        }
        else if (std::abs (to - from) > 0.003f)
        {
            Path arc;
            arc.addCentredArc (x, y, R, R, 0.0f, jmin (from, to), jmax (from, to), true);
            const float gl = (0.4f + 0.6f * amt) * on * dim;
            glowStroke (g, arc, acc.withAlpha (gl), d.big ? 4.0f : 2.4f, gl);
        }
    }

    // cached body
    const float half = r + 14.0f;
    g.setOpacity (1.0f);
    g.drawImageTransformed (c.base, AffineTransform::scale (1.0f / px).translated (x - half, y - half));

    // ridges on the wall: they travel with the rotation, the light stays
    const int n = d.big ? 64 : 36;
    for (int i = 0; i < n; ++i)
    {
        const float ph = a + (float) i * pi * 2.0f / (float) n, sn = std::sin (ph);
        if (sn <= 0.02f) continue;
        const float cc = std::cos (ph), px2 = x + cc * r, bri = jmax (0.0f, -cc * 0.8f + 0.35f) * sn;
        const float sq = std::sqrt (jmax (0.0f, r * r - (px2 - x) * (px2 - x)));
        g.setColour (Colours::white.withAlpha (0.04f + 0.38f * bri * bri));
        g.drawLine (px2, y + sq, px2, y + dd + sq - 1.0f, (d.big ? 1.5f : 1.1f) * sn);
    }
    // fine radial brushing on the top ring (rotates)
    const int m = d.big ? 90 : 40;
    for (int i = 0; i < m; ++i)
    {
        const float ph = a + (float) i * pi * 2.0f / (float) m, b = jmax (0.0f, std::cos (ph - pi * 0.5f - lightAngle));
        const float b6 = b * b * b; const float al = 0.015f + 0.16f * b6 * b6;
        if (al < 0.02f) continue;
        const float cc = std::cos (ph - pi * 0.5f), s = std::sin (ph - pi * 0.5f);
        g.setColour (Colours::white.withAlpha (al));
        g.drawLine (x + cc * (cap + 2), y + s * (cap + 2), x + cc * (r - 1.5f), y + s * (r - 1.5f), 0.9f);
    }
    // engraved gorilla: rotates with the knob, its lit and shadowed edges stay put
    if (d.big)
    {
        const float sc = cap * 1.02f / 534.0f, lit = v * on * dim;
        Graphics::ScopedSaveState ss (g);
        Path clip;
        clip.addEllipse (x - cap, y - cap, cap * 2, cap * 2);
        g.reduceClipRegion (clip);
        g.setColour (Colours::white.withAlpha (0.18f)); g.fillPath (gorilla (x + 0.9f, y + 1.2f, sc, a));
        g.setColour (Colours::black.withAlpha (0.9f));  g.fillPath (gorilla (x - 0.5f, y - 0.7f, sc, a));
        const auto gp = gorilla (x, y, sc, a);
        g.setColour (Colour (0xff0a0a0d)); g.fillPath (gp);
        if (lit > 0.05f && c.glow.isValid())
        {
            const float gh = cap + 18.0f;
            g.setColour (t.logo.withAlpha (0.85f * lit));
            g.drawImageTransformed (c.glow, AffineTransform::scale (1.0f / px).translated (-gh, -gh).rotated (a).translated (x, y), true);
        }
        g.setColour (t.logo.withAlpha (0.08f + 0.92f * std::pow (lit, 0.85f)));
        g.fillPath (gp);
    }
    // pointer
    {
        const auto rot = AffineTransform::rotation (a, x, y);
        const float inner = d.big ? cap * 0.8f : cap * 0.25f;
        Path p1; p1.addLineSegment (Line<float> (x + 1, y - inner + 1, x + 1, y - r + 3), 1.0f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.strokePath (p1, PathStrokeType (d.big ? 4.5f : 3.0f, PathStrokeType::curved, PathStrokeType::rounded), rot);
        Path p2; p2.addLineSegment (Line<float> (x, y - inner, x, y - r + 4), 1.0f);
        g.setColour (Colour (0xfff6f3f4));
        g.strokePath (p2, PathStrokeType (d.big ? 2.6f : 2.0f, PathStrokeType::curved, PathStrokeType::rounded), rot);
    }
    // specular highlight (fixed)
    {
        Graphics::ScopedSaveState ss (g);
        const float hx = x - cap * 0.26f, hy = y - cap * 0.5f;
        g.addTransform (AffineTransform::scale (1.0f, 0.55f, hx, hy));
        ColourGradient sp (Colours::white.withAlpha (0.5f), hx, hy, Colours::white.withAlpha (0.0f), hx + cap * 0.62f, hy, true);
        g.setGradientFill (sp);
        g.fillEllipse (hx - cap * 0.62f, hy - cap * 0.62f, cap * 1.24f, cap * 1.24f);
    }
    // the ring's glow reflected on the wall
    const float reflAmt = bipolar ? (k == 7 ? 0.8f : std::abs (v - 0.5f) * 2.0f) : v;
    if (reflAmt > 0.01f)
    {
        Path refl;
        refl.addCentredArc (x, y + dd * 0.5f, r - 1.0f, r - 1.0f, 0.0f, pi * 0.6f, pi * 1.4f, true);
        g.setColour (acc.withAlpha (0.28f * reflAmt * on * dim));
        g.strokePath (refl, PathStrokeType (dd * 0.7f));
    }
    // readout while touching
    if (hot)
    {
        String txt;
        if (bipolar) { const float db = knobParam[k]->convertFrom0to1 (v); txt = (db > 0.05f ? "+" : "") + String (db, 1) + " dB"; }
        else txt = String (roundToInt (v * 100.0f)) + "%";
        const Font f = uiFont (12.0f, true);
        const float w = GlyphArrangement::getStringWidth (f, txt) + 14.0f;
        const float ty = y - r - (d.big ? 44.0f : 34.0f);
        g.setColour (Colour (0xe00a0a0c));
        g.fillRoundedRectangle (x - w * 0.5f, ty, w, 20.0f, 6.0f);
        g.setColour (Colours::white);
        g.setFont (f);
        g.drawText (txt, Rectangle<float> (x - w * 0.5f, ty, w, 20.0f), Justification::centred);
    }
}

// IN knob colour: pale when the iron gets too little, theme colour in the sweet spot, red when it is too hot
Colour BreatheEditor::inColour() const
{
    const auto& t = look.theme();
    const float lvl = shownInLevel;
    const float on = jlimit (0.0f, 1.0f, (lvl + 30.0f) / 6.0f);       // -30 .. -24 dB RMS: wakes up
    const float hot = jlimit (0.0f, 1.0f, (lvl + 12.0f) / 4.0f);      // -12 .. -8 dB RMS: turns red
    return t.mute.withAlpha (0.6f).interpolatedWith (t.accent, on).interpolatedWith (Colour (0xffff3030), hot);
}

void BreatheEditor::drawJewel (Graphics& g, Point<float> c, float r, bool on, Colour col)
{
    const float x = c.x, y = c.y, d = 6.0f;
    {
        Path sh; sh.addEllipse (x + 1 - (r + 5), y + d + 3 - (r + 4), (r + 5) * 2, (r + 4) * 2);
        DropShadow (Colours::black.withAlpha (0.65f), 6, {}).drawForPath (g, sh);
    }
    ColourGradient side (Colour (0xff2a2a2e), x - r - 4, 0, Colour (0xff151518), x + r + 4, 0, false);
    side.addColour (0.3, Colour (0xffcfcfd6));
    side.addColour (0.6, Colour (0xff5a5a60));
    g.setGradientFill (side);
    g.fillRect (x - r - 4, y, (r + 4) * 2, d);
    g.fillEllipse (x - r - 4, y + d - r - 4, (r + 4) * 2, (r + 4) * 2);
    ColourGradient top (Colour (0xfff2f2f6), x - r, y - r, Colour (0xff26262a), x + r, y + r, false);
    top.addColour (0.45, Colour (0xff77777e));
    g.setGradientFill (top);
    g.fillEllipse (x - r - 4, y - r - 4, (r + 4) * 2, (r + 4) * 2);
    g.setColour (Colour (0xff09090b));
    g.fillEllipse (x - r - 0.8f, y - r - 0.8f, (r + 0.8f) * 2, (r + 0.8f) * 2);
    const float ly = y - 3.0f;
    Path lens; lens.addEllipse (x - r, ly - r * 1.02f, r * 2, r * 2.04f);
    if (on) DropShadow (col.withAlpha (0.9f), roundToInt (r * 1.6f), {}).drawForPath (g, lens);
    ColourGradient lg (on ? Colours::white : col.withAlpha (0.5f), x - r * 0.25f, ly - r * 0.3f,
                       on ? mixWith (col, -0.55f) : Colour (0xff0a0507), x - r * 0.25f + r * 1.25f, ly - r * 0.3f, true);
    if (on) { lg.addColour (0.25, mixWith (col, 0.5f)); lg.addColour (0.7, col); }
    else lg.addColour (0.6, mixWith (col, -0.75f));
    g.setGradientFill (lg);
    g.fillPath (lens);
    Path thick; thick.addCentredArc (x, ly + 1.5f, r - 0.5f, r - 0.5f, 0.0f, pi * 0.65f, pi * 1.35f, true);
    g.setColour (on ? col.withAlpha (0.6f) : Colours::black.withAlpha (0.6f));
    g.strokePath (thick, PathStrokeType (2.0f));
    g.setColour (Colours::white.withAlpha (0.75f));
    g.fillEllipse (x - r * 0.35f - r * 0.34f, ly - r * 0.45f - r * 0.18f, r * 0.68f, r * 0.36f);
}

void BreatheEditor::drawSwitchText (Graphics& g, const String& text, Point<float> c, bool lit)
{
    const auto& t = look.theme();
    const Font f = uiFont (11.0f, true, 0.12f);
    const Rectangle<float> r (c.x - 40.0f, c.y - 9.0f, 80.0f, 18.0f);
    g.setFont (f);
    if (lit)
    {
        for (auto o : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) })
        {
            g.setColour (t.accent.withAlpha (0.25f));
            g.drawText (text, r + o * 1.5f, Justification::centred);
        }
        g.setColour (t.accent.withAlpha (0.12f));
        g.fillRoundedRectangle (r.withSizeKeepingCentre (GlyphArrangement::getStringWidth (f, text) + 16.0f, 18.0f), 6.0f);
        g.setColour (mixWith (t.accent, 0.35f));
        g.drawText (text, r, Justification::centred);
    }
    else
    {
        g.setColour (t.lightBg ? Colours::white.withAlpha (0.4f) : Colours::black.withAlpha (0.75f));
        g.drawText (text, r.translated (0, 1), Justification::centred);
        g.setColour (t.mute);
        g.drawText (text, r, Justification::centred);
    }
}

//==============================================================================
void BreatheEditor::drawTube (Graphics& g)
{
    const auto& t = look.theme();
    const Colour acc = t.accent;
    const float tx = tubeWin.getX(), ty = tubeWin.getY(), tw = tubeWin.getWidth(), th = tubeWin.getHeight();
    const float age = value (5);
    const float flick = 1.0f + 0.06f * std::sin ((float) clock * 37.0f) + 0.04f * std::sin ((float) clock * 91.3f)
                        + (rnd.nextFloat() - 0.5f) * 0.07f * (0.3f + age * 1.4f);
    const float h = jlimit (0.0f, 1.2f, heat * flick * 1.45f);
    auto hot = [h] (float alpha) { return Colour ((uint8) 255, (uint8) jmin (255, roundToInt (170 + 70 * h)), (uint8) jmin (255, roundToInt (150 + 60 * h)), jlimit (0.0f, 1.0f, alpha)); };

    Graphics::ScopedSaveState ss (g);
    Path win; win.addRoundedRectangle (tubeWin.expanded (2.0f), 26.0f);
    g.reduceClipRegion (win);
    g.setGradientFill (ColourGradient (Colour (0xff09080a), 0, ty, Colour (0xff020203), 0, ty + th, false));
    g.fillRect (tubeWin.expanded (2.0f));
    {
        ColourGradient glow (acc.withAlpha (0.85f * jmin (1.0f, h)), cx, ty + th * 0.48f, acc.withAlpha (0.0f), cx + 115.0f, ty + th * 0.48f, true);
        glow.addColour (0.45, acc.withAlpha (0.3f * jmin (1.0f, h)));
        g.setGradientFill (glow);
        g.fillRect (tubeWin);
    }
    const float bx = cx - 35.0f, bw = 70.0f, by = ty + 16.0f, bb = ty + th - 40.0f;
    Path bulb;
    bulb.startNewSubPath (bx, bb);
    bulb.lineTo (bx, by + bw * 0.5f);
    bulb.addCentredArc (cx, by + bw * 0.5f, bw * 0.5f, bw * 0.5f, 0.0f, -pi * 0.5f, pi * 0.5f);
    bulb.lineTo (bx + bw, bb);
    bulb.quadraticTo (bx + bw, bb + 10, bx + bw - 8, bb + 12);
    bulb.lineTo (bx + 8, bb + 12);
    bulb.quadraticTo (bx, bb + 10, bx, bb);
    bulb.closeSubPath();

    {
        Graphics::ScopedSaveState s2 (g);
        g.reduceClipRegion (bulb);
        g.setColour (Colour ((uint8) 120, 110, 120, 0.05f));
        g.fillRect (bx, by, bw, bb - by + 12);
        // support rods
        for (float ox : { -25.0f, 25.0f })
        {
            ColourGradient rod (Colour (0xff2a2a2e), cx + ox - 1.5f, 0, Colour (0xff2a2a2e), cx + ox + 1.5f, 0, false);
            rod.addColour (0.5, Colour (0xffb0b0b8));
            g.setGradientFill (rod);
            g.fillRect (cx + ox - 1.2f, by + 36, 2.4f, bb - by - 24);
        }
        g.setColour (Colour ((uint8) 170, 170, 180, 0.6f));
        g.drawEllipse (cx - 11, by + 22.5f, 22, 7, 1.5f);
        const float pt = by + 48.0f, pb = bb - 30.0f;
        auto mica = [&] (float yy)
        {
            g.setColour (Colour ((uint8) 225, 220, 210, 0.16f));
            g.fillEllipse (cx - 31, yy - 6.5f, 62, 13);
            g.setColour (Colours::white.withAlpha (0.28f));
            g.drawEllipse (cx - 31, yy - 6.5f, 62, 13, 0.8f);
            g.setColour (Colours::black.withAlpha (0.35f));
            for (float ox : { -18.0f, -6.0f, 6.0f, 18.0f }) g.fillEllipse (cx + ox - 2.2f, yy - 1.0f, 4.4f, 2.0f);
            g.setColour (hot (0.18f * h));
            g.fillEllipse (cx - 24, yy - 4, 48, 8);
        };
        mica (pt - 6.0f);
        for (int side = 0; side < 2; ++side)
        {
            const float px0 = side == 0 ? cx - 24.0f : cx + 3.0f, pw = 21.0f;
            ColourGradient plate (Colour (0xff1b1b1e), px0, 0, Colour (0xff18181b), px0 + pw, 0, false);
            plate.addColour (0.25, Colour (0xff5f6068));
            plate.addColour (0.5, Colour (0xff2c2c31));
            plate.addColour (0.8, Colour (0xff55565d));
            g.setGradientFill (plate);
            g.fillRoundedRectangle (px0, pt, pw, pb - pt, 2.0f);
            g.setColour (Colours::black.withAlpha (0.45f));
            for (int i = 1; i < 4; ++i) g.drawLine (px0 + (float) i * pw / 4.0f, pt + 3, px0 + (float) i * pw / 4.0f, pb - 3, 0.8f);
            const float sx = px0 + pw * 0.5f - 2.5f;
            g.setColour (Colour (0xff040405));
            g.fillRect (sx, pt + 6, 5.0f, pb - pt - 12);
            Path fil; fil.addRectangle (sx + 1.2f, pt + 8, 2.6f, pb - pt - 16);
            if (h > 0.02f)
            {
                const float gpx = scale() * physScale;
                if (! filGlow.isValid() || filGlowPx != gpx)
                {
                    // one soft vertical glow, made once and reused every frame
                    filGlowPx = gpx;
                    const float gw = 40.0f, ghh = pb - pt + 40.0f;
                    filGlow = Image (Image::ARGB, roundToInt (gw * gpx), roundToInt (ghh * gpx), true);
                    Graphics gg (filGlow);
                    gg.addTransform (AffineTransform::scale (gpx));
                    Path bar; bar.addRectangle (gw * 0.5f - 1.3f, 20.0f, 2.6f, ghh - 40.0f);
                    DropShadow (Colours::white, roundToInt (14.0f * gpx), {}).drawForPath (gg, bar);
                }
                const float gw = 40.0f;
                g.setColour (acc.withAlpha (jmin (1.0f, h)));
                g.drawImageTransformed (filGlow, AffineTransform::scale (1.0f / gpx).translated (sx + 2.5f - gw * 0.5f, pt - 20.0f), true);
                g.setColour (hot (0.5f * h));
                g.drawImageTransformed (filGlow, AffineTransform::scale (0.5f / gpx, 1.0f / gpx).translated (sx + 2.5f - gw * 0.25f, pt - 20.0f), true);
            }
            g.setColour (hot (0.15f + 0.85f * h));
            g.fillPath (fil);
            g.setColour (Colour ((uint8) 150, 150, 160, 0.45f));
            for (float yy = pt + 10; yy < pb - 8; yy += 3) g.drawLine (sx - 1, yy, sx + 6, yy, 0.6f);
            for (float yy : { pt + 2.0f, pb - 2.0f })
            {
                ColourGradient sp (hot (0.95f * h), sx + 2.5f, yy, acc.withAlpha (0.0f), sx + 16.5f, yy, true);
                sp.addColour (0.4, acc.withAlpha (0.6f * jmin (1.0f, h)));
                g.setGradientFill (sp);
                g.fillEllipse (sx + 2.5f - 14, yy - 14, 28, 28);
            }
        }
        mica (pb + 6.0f);
        // getter flash on the dome
        g.setGradientFill (ColourGradient (Colour ((uint8) 150, 150, 160, 0.75f), 0, by, Colour ((uint8) 40, 40, 48, 0.0f), 0, by + 34, false));
        g.fillRect (bx, by - 2, bw, 36.0f);
        {
            ColourGradient gl (Colours::white.withAlpha (0.0f), bx, 0, Colours::white.withAlpha (0.0f), bx + bw * 0.42f, 0, false);
            gl.addColour (0.7, Colours::white.withAlpha (0.35f));
            g.setGradientFill (gl);
            g.fillRect (bx, by, bw * 0.42f, 22.0f);
        }
        // warm light inside
        g.setGradientFill (ColourGradient (acc.withAlpha (0.4f * jmin (1.0f, h)), cx, (pt + pb) * 0.5f, acc.withAlpha (0.0f), cx + 60, (pt + pb) * 0.5f, true));
        g.fillRect (bx, by, bw, bb - by);
        // fresnel: glass is thicker at the edges
        {
            ColourGradient fr (Colours::white.withAlpha (0.34f), bx, 0, Colours::white.withAlpha (0.26f), bx + bw, 0, false);
            fr.addColour (0.07, Colours::white.withAlpha (0.10f));
            fr.addColour (0.22, Colours::white.withAlpha (0.015f));
            fr.addColour (0.78, Colours::white.withAlpha (0.015f));
            fr.addColour (0.93, Colours::white.withAlpha (0.08f));
            g.setGradientFill (fr);
            g.fillPath (bulb);
            ColourGradient rim (acc.withAlpha (0.35f * jmin (1.0f, h)), bx, 0, acc.withAlpha (0.45f * jmin (1.0f, h)), bx + bw, 0, false);
            rim.addColour (0.1, acc.withAlpha (0.0f));
            rim.addColour (0.9, acc.withAlpha (0.0f));
            g.setGradientFill (rim);
            g.fillRect (bx, by + 30, bw, bb - by - 30);
        }
        // studio window reflections
        {
            ColourGradient s1 (Colours::white.withAlpha (0.0f), bx + 7, 0, Colours::white.withAlpha (0.0f), bx + 19, 0, false);
            s1.addColour (0.5, Colours::white.withAlpha (0.55f));
            g.setGradientFill (s1);
            g.fillRoundedRectangle (bx + 8, by + 26, 10, bb - by - 40, 5);
            g.setColour (Colours::white.withAlpha (0.22f));
            g.fillRoundedRectangle (bx + 22, by + 40, 2, bb - by - 70, 1);
            ColourGradient s2c (Colours::white.withAlpha (0.0f), bx + bw - 16, 0, Colours::white.withAlpha (0.0f), bx + bw - 6, 0, false);
            s2c.addColour (0.5, Colours::white.withAlpha (0.18f));
            g.setGradientFill (s2c);
            g.fillRoundedRectangle (bx + bw - 16, by + 30, 10, 46, 5);
        }
    }
    // outline: brighter on the left
    g.setColour (Colours::white.withAlpha (0.28f));
    g.strokePath (bulb, PathStrokeType (1.0f));
    {
        Graphics::ScopedSaveState s3 (g);
        g.reduceClipRegion (Rectangle<int> ((int) bx - 2, (int) by, (int) (bw * 0.5f), (int) (bb - by)));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.strokePath (bulb, PathStrokeType (1.3f));
    }
    Path spec; spec.addCentredArc (cx, by + bw * 0.5f, bw * 0.5f - 5, bw * 0.5f - 5, 0.0f, -pi * 0.32f, -pi * 0.08f, true);
    g.setColour (Colours::white.withAlpha (0.7f));
    g.strokePath (spec, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    {
        Path tip;
        tip.startNewSubPath (cx - 5, by + 1);
        tip.quadraticTo (cx - 4, by - 9, cx, by - 10);
        tip.quadraticTo (cx + 4, by - 9, cx + 5, by + 1);
        tip.closeSubPath();
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), cx - 5, 0, Colours::white.withAlpha (0.05f), cx + 5, 0, false));
        g.fillPath (tip);
    }
    // base
    {
        ColourGradient base (Colour (0xff0b0b0c), bx - 6, 0, Colour (0xff060607), bx + bw + 6, 0, false);
        base.addColour (0.3, Colour (0xff3a3a3f));
        base.addColour (0.55, Colour (0xff141416));
        g.setGradientFill (base);
        g.fillRoundedRectangle (bx - 5, bb + 10, bw + 10, th - (bb - ty) - 6, 5);
        g.setColour (Colours::black.withAlpha (0.6f));
        for (int i = 1; i < 10; ++i) g.drawLine (bx - 5 + (float) i * (bw + 10) / 10.0f, bb + 14, bx - 5 + (float) i * (bw + 10) / 10.0f, ty + th, 1.0f);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.fillRect (bx - 5, bb + 10, bw + 10, 1.0f);
    }
    // window pane
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), tx, ty, Colours::white.withAlpha (0.0f), tx + tw * 0.35f, ty + th * 0.35f, false));
    g.fillRect (tubeWin);
}

void BreatheEditor::drawMeter (Graphics& g, float x, float lvl, float peak)
{
    const auto& t = look.theme();
    const int N = 28;
    const float top = meterTop, bot = meterBot, sh = (bot - top - (float) (N - 1) * 2.0f) / (float) N;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (x - 15, top - 7, 30, bot - top + 14, 7);
    g.setColour (Colour (0xff050506));
    g.fillRoundedRectangle (x - 14, top - 6, 28, bot - top + 12, 6);
    for (int i = 0; i < N; ++i)
    {
        const float fr = ((float) i + 0.5f) / (float) N, y = bot - (float) (i + 1) * sh - (float) i * 2.0f;
        const bool hotSeg = fr > 0.88f, lit = fr <= lvl, pk = std::abs (fr - peak) < 0.5f / (float) N && peak > 0.01f;
        const Rectangle<float> seg (x - 9, y, 18, sh);
        if (lit || pk)
        {
            g.setColour (t.accent.withAlpha (0.25f));
            g.fillRoundedRectangle (seg.expanded (2.0f), 2.5f);
            g.setColour (hotSeg ? Colour (0xfffff0f6) : t.accent);
            g.fillRoundedRectangle (seg, 1.5f);
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillRect (x - 8, y + 0.5f, 16.0f, 1.0f);
        }
        else
        {
            g.setColour (t.accent.withAlpha (0.08f));
            g.fillRoundedRectangle (seg, 1.5f);
        }
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), x - 14, 0, Colours::white.withAlpha (0.0f), x - 3, 0, false));
    g.fillRoundedRectangle (x - 14, top - 6, 28, bot - top + 12, 6);
}

void BreatheEditor::drawScreen (Graphics& g)
{
    const auto& t = look.theme();
    const Colour acc = t.accent;
    const bool byp = bypassed();
    const float x = scr.getX(), y = scr.getY(), w = scr.getWidth(), h = scr.getHeight();
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (scr, 9.0f);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colour (0xff120c10), 0, y, Colour (0xff050305), 0, y + h, false));
    g.fillRect (scr);
    g.setColour (acc.withAlpha (0.06f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine (roundToInt (fx (f)), y, y + h);

    const float edges[5] { 20.0f, xoFreq (0), xoFreq (1), xoFreq (2), 20000.0f };
    Path curve, fill;
    for (int i = 0; i <= 160; ++i)
    {
        const float px = x + (float) i / 160.0f * w;
        const float py = y + h - 10.0f - jlimit (0.0f, 1.0f, spectrum[(size_t) i]) * (h - 30.0f);
        if (i == 0) { curve.startNewSubPath (px, py); fill.startNewSubPath (x, y + h); }
        curve.lineTo (px, py);
        fill.lineTo (px, py);
    }
    fill.lineTo (x + w, y + h);
    fill.closeSubPath();

    for (int b = 0; b < 4; ++b)
    {
        const float x0 = fx (edges[b]), x1 = fx (edges[b + 1]);
        const bool au = audible (b);
        const float v = value (b);
        Graphics::ScopedSaveState s2 (g);
        g.reduceClipRegion (Rectangle<int> ((int) std::floor (x0), (int) y, (int) std::ceil (x1 - x0) + 1, (int) h));
        if (flag (soloParam[b])) { g.setColour (acc.withAlpha (0.07f)); g.fillRect (x0, y, x1 - x0, h); }
        g.setGradientFill (ColourGradient (acc.withAlpha ((0.14f + 0.5f * v) * (byp ? 0.3f : 1.0f) * (au ? 1.0f : 0.25f)), 0, y, acc.withAlpha (0.03f), 0, y + h, false));
        g.fillPath (fill);
        if (! au)
        {
            g.setColour (Colours::white.withAlpha (0.04f));
            for (float k = -h; k < x1 - x0; k += 14.0f) g.drawLine (x0 + k, y + h, x0 + k + h, y, 6.0f);
        }
        g.setFont (uiFont (9.0f, true, 0.05f));
        g.setColour (acc.withAlpha (au ? 0.6f : 0.2f));
        g.drawText (Params::bandName[b], Rectangle<float> (x0, y + h - 18, x1 - x0, 12), Justification::centred);
    }
    glowStroke (g, curve, acc.withAlpha (byp ? 0.35f : 0.95f), 1.7f, byp ? 0.3f : 0.9f);

    for (int i = 0; i < 3; ++i)
    {
        const float f = xoFreq (i), px = fx (f);
        Path dash;
        const float dl[] = { 3.0f, 3.0f };
        Path line;
        line.startNewSubPath (px, y + 20);
        line.lineTo (px, y + h);
        PathStrokeType (1.0f).createDashedStroke (dash, line, dl, 2);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillPath (dash);
        g.setColour (Colours::white);
        g.fillEllipse (px - 5, y + 15, 10, 10);
        g.setColour (acc);
        g.fillEllipse (px - 2.8f, y + 17.2f, 5.6f, 5.6f);
        g.setFont (uiFont (9.5f, true));
        g.setColour (Colours::white.withAlpha (0.75f));
        g.drawText (freqShort (f), Rectangle<float> (px - 30, y + 2, 60, 12), Justification::centred);
    }

    // S / M ghosts
    for (int b = 0; b < 4; ++b)
        for (int m = 0; m < 2; ++m)
        {
            const bool act = flag (m == 0 ? soloParam[b] : muteParam[b]);
            const float al = jmax (smFade[b], act ? 1.0f : 0.0f);
            if (al < 0.01f) continue;
            const auto c = smButton (b, m == 1);
            const Colour col = m == 0 ? acc : Colour (0xffff4a4a);
            const float a2 = al * (act ? 1.0f : 0.75f);
            const auto r = Rectangle<float> (c.x - 13, c.y - 13, 26, 26);
            g.setColour (act ? col.withAlpha (0.85f * a2) : Colour ((uint8) 20, 18, 22, 0.55f * a2));
            g.fillEllipse (r);
            if (act)
            {
                g.setColour (col.withAlpha (0.25f * a2));
                g.drawEllipse (r.expanded (3.0f), 4.0f);
            }
            g.setColour (act ? col.withAlpha (a2) : Colours::white.withAlpha (0.45f * a2));
            g.drawEllipse (r, 1.2f);
            g.setColour (Colours::white.withAlpha ((act ? 1.0f : 0.85f) * a2));
            g.setFont (uiFont (12.0f, true));
            g.drawText (m == 0 ? "S" : "M", r.translated (0, 0.5f), Justification::centred);
        }

    g.setColour (Colours::black.withAlpha (0.2f));
    for (float yy = y; yy < y + h; yy += 3.0f) g.fillRect (x, yy, w, 1.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.08f), 0, y, Colours::white.withAlpha (0.0f), 0, y + h * 0.5f, false));
    g.fillRect (x, y, w, h * 0.5f);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (scr, 9.0f, 1.5f);
}

//==============================================================================
void BreatheEditor::paint (Graphics& g)
{
    const float phys = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    physScale = phys;
    if (! panel.isValid() || panelScale != phys || panelTheme != look.theme().name
        || panel.getWidth() != roundToInt ((float) getWidth() * phys))
        buildPanel (phys);
    g.drawImageTransformed (panel, AffineTransform::scale (1.0f / phys));

    const auto& t = look.theme();
    const float s = scale();
    g.addTransform (AffineTransform::scale (s));
    auto visible = [&g] (Rectangle<float> r) { return g.clipRegionIntersects (r.getSmallestIntegerContainer().expanded (4)); };

    if (visible (Rectangle<float> (cx - 140, 80, 280, 298)))
    {
        drawTube (g);
        drawMeter (g, cx - 96, meterL[0], peakL[0]);
        drawMeter (g, cx + 96, meterL[1], peakL[1]);
        const int o = (int) ovsParam->convertFrom0to1 (ovsParam->getValue());
        drawSwitchText (g, "OVS " + Params::ovsNames[o], ovsC, o > 0);
        drawSwitchText (g, "LIN", linC, flag (linParam));
        const bool mixOn = modeParam->getValue() > 0.5f;
        drawSwitchText (g, "MASTER", masterC, ! mixOn);
        drawSwitchText (g, "MIX", mixC, mixOn);
    }
    if (visible (scr.expanded (4)))
        drawScreen (g);

    const float edges[5] { 20.0f, xoFreq (0), xoFreq (1), xoFreq (2), 20000.0f };
    for (int k = 0; k < numKnobs; ++k)
    {
        if (! visible (knobArea (k))) continue;
        drawKnob (g, k, k == hoverKnob || k == dragKnob);
        if (k < 4)
        {
            g.setFont (uiFont (10.0f));
            g.setColour (t.mute);
            g.drawText (freqShort (edges[k]) + String::fromUTF8 ("\xe2\x80\x93") + freqShort (edges[k + 1]) + " Hz",
                        Rectangle<float> (knobDefs[k].x - 80, knobDefs[k].y + 124, 160, 14), Justification::centred);
        }
    }
    if (visible (Rectangle<float> (bypassC.x - 24, bypassC.y - 24, 48, 48)))
        drawJewel (g, bypassC, 12.0f, bypassed(), Colour (0xffff3a3a));
}

//==============================================================================
void BreatheEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    tick (jlimit (0.0, 0.1, now - lastTick));
    lastTick = now;
}

void BreatheEditor::tick (double dt)
{
    clock += dt;
    const float fdt = (float) dt;

    // meters
    const float now[2] { dbToFrac (jmax (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f))),
                         dbToFrac (jmax (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f))) };
    for (int m = 0; m < 2; ++m)
    {
        meterL[m] = jmax (now[m], meterL[m] - fdt * 0.55f);
        if (meterL[m] >= peakL[m]) { peakL[m] = meterL[m]; peakAge[m] = 0.0f; }
        else if ((peakAge[m] += fdt) > 1.0f) peakL[m] = jmax (0.0f, peakL[m] - fdt * 0.3f);
    }

    // tube heat
    float amt = 0.0f;
    for (int b = 0; b < 4; ++b) amt += audible (b) ? value (b) * 0.25f : 0.0f;
    amt *= value (4);
    const float target = bypassed() ? 0.0f : (0.2f + 0.8f * amt) * (0.75f + 0.25f * jmin (1.0f, proc.drive.load() * 1.5f));
    heat += (target - heat) * jmin (1.0f, fdt * (target > heat ? 1.6f : 0.7f));

    // S/M fades
    for (int b = 0; b < 4; ++b)
    {
        const float tg = b == hoverBand ? 1.0f : 0.0f;
        smFade[b] += (tg - smFade[b]) * jmin (1.0f, fdt * (tg > 0.5f ? 10.0f : 5.0f));
    }

    // spectrum
    {
        int s1, n1, s2, n2;
        const int ready = proc.scopeFifo.getNumReady();
        proc.scopeFifo.prepareToRead (ready, s1, n1, s2, n2);
        auto push = [this] (const float* d, int n) { for (int i = 0; i < n; ++i) { ring[(size_t) ringPos] = d[i]; ringPos = (ringPos + 1) & 4095; } };
        push (proc.scopeData.data() + s1, n1);
        push (proc.scopeData.data() + s2, n2);
        proc.scopeFifo.finishedRead (n1 + n2);
        for (int i = 0; i < 4096; ++i)
        {
            const float wnd = 0.5f - 0.5f * std::cos (2.0f * pi * (float) i / 4095.0f);
            fftBuf[(size_t) i] = ring[(size_t) ((ringPos + i) & 4095)] * wnd;
        }
        std::fill (fftBuf.begin() + 4096, fftBuf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);
        const double fs = proc.currentRate();
        for (int i = 0; i <= 160; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 160.0f);
            const float bin = f / (float) fs * 4096.0f;
            const int b0 = jlimit (1, 2047, (int) bin), b1 = jlimit (1, 2047, (int) (bin * 1.12f) + 1);
            float m = 0.0f;
            for (int k = b0; k <= b1; ++k) m = jmax (m, fftBuf[(size_t) k]);
            const float db = Decibels::gainToDecibels (m / 1024.0f, -120.0f) + 4.5f * std::log2 (f / 1000.0f);   // tilt like a pink-noise view
            const float tgt = jlimit (0.0f, 1.0f, (db + 78.0f) / 66.0f);
            auto& sp = spectrum[(size_t) i];
            sp += (tgt - sp) * (tgt > sp ? 0.6f : 0.12f);
        }
    }

    repaint (toPixels (Rectangle<float> (cx - 142, 78, 284, 302)));
    repaint (toPixels (scr.expanded (6)));

    // knobs only when something about them changed
    String sig;
    for (int k = 0; k < numKnobs; ++k) sig << String (value (k), 4) << ",";
    for (int i = 0; i < 3; ++i) sig << String (xoFreq (i), 1) << ",";
    for (int b = 0; b < 4; ++b) sig << (audible (b) ? "1" : "0");
    shownInLevel += (proc.inLevelDb.load() - shownInLevel) * jmin (1.0f, fdt * 6.0f);
    sig << hoverKnob << dragKnob << (bypassed() ? "B" : "b") << look.theme().name << inColour().toString();
    if (sig != knobSignature)
    {
        knobSignature = sig;
        for (int k = 0; k < numKnobs; ++k) repaint (toPixels (knobArea (k)));
        repaint (toPixels (Rectangle<float> (bypassC.x - 30, bypassC.y - 30, 60, 60)));
    }
}

void BreatheEditor::renderForTest (double seconds)
{
    for (double t = 0; t < seconds; t += 0.025)
        tick (0.025);
}

void BreatheEditor::hoverForTest (Point<float> logical)
{
    mouse = logical;
    hoverKnob = knobAt (logical);
    hoverBand = bandAt (logical);
}

//==============================================================================
void BreatheEditor::mouseMove (const MouseEvent& e)
{
    mouse = toLogical (e.position);
    hoverKnob = knobAt (mouse);
    hoverBand = bandAt (mouse);
    const auto key = hitKey (mouse);
    MouseCursor c = MouseCursor::NormalCursor;
    if (hoverKnob >= 0) c = MouseCursor::UpDownResizeCursor;
    else if (xoAt (mouse) >= 0) c = MouseCursor::LeftRightResizeCursor;
    else if (key == "solo" || key == "mute" || key == "ovs" || key == "lin" || key == "gear" || key == "bypass" || key == "master" || key == "mixMode") c = MouseCursor::PointingHandCursor;
    setMouseCursor (c);
}

void BreatheEditor::mouseExit (const MouseEvent&)
{
    hoverKnob = -1;
    hoverBand = -1;
    mouse = { -100, -100 };
}

void BreatheEditor::mouseDown (const MouseEvent& e)
{
    mouse = toLogical (e.position);
    const auto key = hitKey (mouse);
    if (key == "gear") { showGearMenu(); return; }
    if (key == "bypass") { toggle (bypassParam); return; }
    if (key == "lin") { toggle (linParam); return; }
    if (key == "ovs") { showOvsMenu(); return; }
    if (key == "master" || key == "mixMode")
    {
        modeParam->beginChangeGesture();
        modeParam->setValueNotifyingHost (key == "mixMode" ? 1.0f : 0.0f);
        modeParam->endChangeGesture();
        return;
    }
    if (key == "solo" || key == "mute")
    {
        const int b = bandAt (mouse);
        auto* p = key == "solo" ? soloParam[b] : muteParam[b];
        auto* other = key == "solo" ? muteParam[b] : soloParam[b];
        const bool turnOn = ! flag (p);
        toggle (p);
        if (turnOn && flag (other)) toggle (other);
        return;
    }
    dragXo = xoAt (mouse);
    if (dragXo >= 0) { xoParam[dragXo]->beginChangeGesture(); return; }
    dragKnob = knobAt (mouse);
    if (dragKnob >= 0)
    {
        knobParam[dragKnob]->beginChangeGesture();
        dragLastY = e.position.y;
    }
}

void BreatheEditor::mouseDrag (const MouseEvent& e)
{
    mouse = toLogical (e.position);
    if (dragXo >= 0)
    {
        const float lo = dragXo > 0 ? xoFreq (dragXo - 1) * 1.3f : 0.0f, hi = dragXo < 2 ? xoFreq (dragXo + 1) / 1.3f : 1.0e6f;
        const float f = jlimit (jmax (lo, Params::xoMin[dragXo]), jmin (hi, Params::xoMax[dragXo]), xf (mouse.x));
        setParam (xoParam[dragXo], f);
        return;
    }
    if (dragKnob >= 0)
    {
        auto* p = knobParam[dragKnob];
        const float dy = (dragLastY - e.position.y) / scale();
        dragLastY = e.position.y;
        p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + dy / (e.mods.isShiftDown() ? 900.0f : 220.0f)));
    }
}

void BreatheEditor::mouseUp (const MouseEvent&)
{
    if (dragXo >= 0) xoParam[dragXo]->endChangeGesture();
    if (dragKnob >= 0) knobParam[dragKnob]->endChangeGesture();
    dragXo = dragKnob = -1;
}

void BreatheEditor::mouseDoubleClick (const MouseEvent& e)
{
    mouse = toLogical (e.position);
    const int xi = xoAt (mouse);
    if (xi >= 0)
    {
        xoParam[xi]->beginChangeGesture();
        setParam (xoParam[xi], Params::xoDef[xi]);
        xoParam[xi]->endChangeGesture();
        return;
    }
    const int k = knobAt (mouse);
    if (k >= 0)
    {
        knobParam[k]->beginChangeGesture();
        setParam (knobParam[k], knobDefs[k].def);
        knobParam[k]->endChangeGesture();
    }
}

void BreatheEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int k = knobAt (toLogical (e.position));
    if (k < 0) return;
    auto* p = knobParam[k];
    p->beginChangeGesture();
    p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f)));
    p->endChangeGesture();
}

//==============================================================================
void BreatheEditor::showOvsMenu()
{
    PopupMenu m;
    const int cur = (int) ovsParam->convertFrom0to1 (ovsParam->getValue());
    m.addSectionHeader ("Oversampling");
    for (int i = 0; i < Params::ovsNames.size(); ++i)
        m.addItem (Params::ovsNames[i], true, i == cur, [this, i]
        {
            ovsParam->beginChangeGesture();
            ovsParam->setValueNotifyingHost (ovsParam->convertTo0to1 ((float) i));
            ovsParam->endChangeGesture();
        });
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toPixels (Rectangle<float> (ovsC.x - 34, ovsC.y - 10, 68, 20)))));
}

void BreatheEditor::showGearMenu()
{
    PopupMenu themes, help, langs, m;
    for (const auto& t : Theme::all())
        themes.addItem (t.name, true, t.name == look.theme().name, [this, n = t.name] { applyTheme (n); });
    help.addItem ("On", true, helpOn, [this] { helpOn = true; saveSetting ("help", "on"); applyHelp(); });
    help.addItem ("Off", true, ! helpOn, [this] { helpOn = false; saveSetting ("help", "off"); applyHelp(); });
    langs.addItem ("English", true, lang == Help::Lang::en, [this] { lang = Help::Lang::en; saveSetting ("helpLang", "en"); welcome.setLang (lang); });
    langs.addItem (String::fromUTF8 ("Русский"), true, lang == Help::Lang::ru, [this] { lang = Help::Lang::ru; saveSetting ("helpLang", "ru"); welcome.setLang (lang); });
    m.addSubMenu ("Theme", themes);
    m.addSubMenu ("Help", help);
    m.addSubMenu ("Help language", langs);
    m.addSeparator();
    m.addItem ("Start tour", [this] { tour.start (lang); });
    m.addItem ("Reset window size", [this] { setSize (baseW, baseH); });
    m.addSeparator();
    m.addItem (String ("Breathe ") + JucePlugin_VersionString + " by Primal Rage Lab", false, false, nullptr);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (toPixels (Rectangle<float> (gearC.x - 18, gearC.y - 18, 36, 36)))));
}
