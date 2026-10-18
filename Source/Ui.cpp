#include "Ui.h"
#include "Logo.h"

using namespace juce;

Font uiFont (float height, bool bold, float kerning)
{
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
    if (kerning != 0.0f)
        f.setExtraKerningFactor (kerning);
    return f;
}

//==============================================================================
BLook::BLook() { setTheme (Theme::all().front()); }

void BLook::setTheme (const Theme& t)
{
    current = &t;
    setColour (ResizableWindow::backgroundColourId, t.bg);
    setColour (PopupMenu::backgroundColourId, kWidget);
    setColour (PopupMenu::textColourId, kInk);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour (PopupMenu::highlightedTextColourId, Colour (0xff111114));
    setColour (TextButton::buttonColourId, Colour (0xff26262b));
    setColour (TextButton::textColourOffId, kInk);
    setColour (TooltipWindow::textColourId, kInk);
    setColour (Label::textColourId, kInk);
}

const Theme& themeOf (const Component& c)
{
    if (auto* l = dynamic_cast<BLook*> (&c.getLookAndFeel()))
        return l->theme();
    return Theme::all().front();
}

Font BLook::getPopupMenuFont() { return uiFont (14.0f); }

Rectangle<int> BLook::getTooltipBounds (const String& tip, Point<int> pos, Rectangle<int> parent)
{
    AttributedString s;
    s.append (tip, uiFont (13.0f), kInk);
    TextLayout tl;
    tl.createLayout (s, 300.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 22, h = (int) std::ceil (tl.getHeight()) + 16;
    const int x = pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18;
    const int y = pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18;
    return Rectangle<int> (x, y, w, h).constrainedWithin (parent);
}

void BLook::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    const auto r = Rectangle<int> (w, h).toFloat();
    g.setColour (Colour (0xf2141417));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (theme().accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    AttributedString s;
    s.append (text, uiFont (13.0f), kInk);
    TextLayout tl;
    tl.createLayout (s, (float) w - 22.0f);
    tl.draw (g, r.reduced (11.0f, 8.0f));
}

void BLook::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool hi, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hi ? base.brighter (0.12f) : base);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void BLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.findColour (TextButton::textColourOffId));
    g.setFont (uiFont (14.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}


//==============================================================================
TourOverlay::TourOverlay()
{
    for (auto* b : { &back, &next, &skip })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    back.onClick = [this] { showStep (step - 1); };
    next.onClick = [this]
    {
        if (step + 1 >= Help::tourLength)
        {
            setVisible (false);
            if (onFinish) onFinish();
        }
        else
        {
            showStep (step + 1);
        }
    };
    skip.onClick = [this] { setVisible (false); if (onFinish) onFinish(); };
}

void TourOverlay::start (Help::Lang l)
{
    lang = l;
    setVisible (true);
    toFront (false);
    showStep (0);
}

void TourOverlay::showStep (int s)
{
    step = jlimit (0, Help::tourLength - 1, s);
    const auto& t = themeOf (*this);
    back.setButtonText (Help::ui ("Back", "Назад", lang));
    next.setButtonText (step + 1 >= Help::tourLength ? Help::ui ("Done", "Готово", lang) : Help::ui ("Next", "Дальше", lang));
    skip.setButtonText (Help::ui ("Skip", "Пропустить", lang));
    next.setColour (TextButton::buttonColourId, t.accent);
    next.setColour (TextButton::textColourOffId, Colour (0xff111114));
    back.setVisible (step > 0);
    skip.setVisible (step + 1 < Help::tourLength);
    resized();
    repaint();
}

Rectangle<float> TourOverlay::calloutArea() const
{
    const auto& st = Help::tour[step];
    const auto target = resolveTarget ? resolveTarget (st.target) : Rectangle<float>();
    const float w = 380.0f;
    AttributedString s;
    s.append (Help::ui (st.textEn, st.textRu, lang), uiFont (14.0f), Colours::white);
    TextLayout tl;
    tl.createLayout (s, w - 36.0f);
    const float h = 54.0f + tl.getHeight() + 70.0f;
    const auto bounds = getLocalBounds().toFloat().reduced (16.0f);
    if (target.isEmpty())
        return bounds.withSizeKeepingCentre (w, h);
    float x = jlimit (bounds.getX(), bounds.getRight() - w, target.getCentreX() - w * 0.5f);
    float y = target.getBottom() + 22.0f;
    if (y + h > bounds.getBottom())
        y = target.getY() - 22.0f - h;
    if (y < bounds.getY())
    {
        // Not enough room above or below: put it to the side
        y = jlimit (bounds.getY(), bounds.getBottom() - h, target.getCentreY() - h * 0.5f);
        x = target.getRight() + 22.0f + w < bounds.getRight() ? target.getRight() + 22.0f : target.getX() - 22.0f - w;
    }
    return { x, y, w, h };
}

void TourOverlay::resized()
{
    const auto c = calloutArea().toNearestInt();
    next.setBounds (c.getRight() - 16 - 96, c.getBottom() - 16 - 30, 96, 30);
    back.setBounds (next.getX() - 8 - 86, next.getY(), 86, 30);
    skip.setBounds (c.getX() + 14, next.getY(), 104, 30);
}

void TourOverlay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto& st = Help::tour[step];
    const auto target = resolveTarget ? resolveTarget (st.target) : Rectangle<float>();

    Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    if (! target.isEmpty())
    {
        dim.addRoundedRectangle (target.expanded (6.0f), 12.0f);
        dim.setUsingNonZeroWinding (false);
    }
    g.setColour (Colours::black.withAlpha (0.68f));
    g.fillPath (dim);

    const auto c = calloutArea();
    if (! target.isEmpty())
    {
        g.setColour (t.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (target.expanded (9.0f), 14.0f, 5.0f);
        g.setColour (t.accent);
        g.drawRoundedRectangle (target.expanded (6.0f), 12.0f, 2.0f);

        // Arrow from the card to the highlighted part
        Point<float> from, to;
        if (c.getBottom() <= target.getY())       { from = { c.getCentreX(), c.getBottom() }; to = { jlimit (target.getX() + 10.0f, target.getRight() - 10.0f, c.getCentreX()), target.getY() - 8.0f }; }
        else if (c.getY() >= target.getBottom())  { from = { c.getCentreX(), c.getY() };      to = { jlimit (target.getX() + 10.0f, target.getRight() - 10.0f, c.getCentreX()), target.getBottom() + 8.0f }; }
        else if (c.getX() >= target.getRight())   { from = { c.getX(), c.getCentreY() };      to = { target.getRight() + 8.0f, jlimit (target.getY() + 10.0f, target.getBottom() - 10.0f, c.getCentreY()) }; }
        else                                      { from = { c.getRight(), c.getCentreY() };  to = { target.getX() - 8.0f, jlimit (target.getY() + 10.0f, target.getBottom() - 10.0f, c.getCentreY()) }; }
        Path arrow;
        arrow.addArrow (Line<float> (from, to), 2.5f, 12.0f, 10.0f);
        g.setColour (t.accent);
        g.fillPath (arrow);
    }

    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (c, 12.0f);
    g.setColour (t.accent.withAlpha (0.7f));
    g.drawRoundedRectangle (c.reduced (0.5f), 12.0f, 1.2f);

    g.setColour (t.accentHi);
    g.setFont (uiFont (17.0f, true));
    g.drawText (Help::ui (st.titleEn, st.titleRu, lang), c.reduced (18.0f, 0.0f).withY (c.getY() + 14.0f).withHeight (24.0f), Justification::centredLeft);
    g.setColour (Colour (0xff8a8a94));
    g.setFont (uiFont (12.0f));
    g.drawText (String (step + 1) + " / " + String (Help::tourLength), c.reduced (18.0f, 0.0f).withY (c.getY() + 14.0f).withHeight (24.0f), Justification::centredRight);

    AttributedString s;
    s.append (Help::ui (st.textEn, st.textRu, lang), uiFont (14.0f), Colour (0xffe2e2e8));
    s.setLineSpacing (3.0f);
    TextLayout tl;
    tl.createLayout (s, c.getWidth() - 36.0f);
    tl.draw (g, Rectangle<float> (c.getX() + 18.0f, c.getY() + 46.0f, c.getWidth() - 36.0f, tl.getHeight()));
}

//==============================================================================
WelcomePrompt::WelcomePrompt()
{
    for (auto* b : { &yes, &notNow, &never, &en, &ru })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    ru.setButtonText (String::fromUTF8 ("Русский"));
    yes.onClick = [this] { if (onChoice) onChoice (0); };
    notNow.onClick = [this] { if (onChoice) onChoice (1); };
    never.onClick = [this] { if (onChoice) onChoice (2); };
    en.onClick = [this] { setLang (Help::Lang::en); if (onLanguage) onLanguage (lang); };
    ru.onClick = [this] { setLang (Help::Lang::ru); if (onLanguage) onLanguage (lang); };
    setLang (Help::Lang::en);
}

void WelcomePrompt::setLang (Help::Lang l)
{
    lang = l;
    const auto& t = themeOf (*this);
    yes.setButtonText (Help::ui ("Yes, show me", "Да, покажи", lang));
    notNow.setButtonText (Help::ui ("Not now", "Не сейчас", lang));
    never.setButtonText (Help::ui ("Don't ask again", "Больше не спрашивать", lang));
    yes.setColour (TextButton::buttonColourId, t.accent);
    yes.setColour (TextButton::textColourOffId, Colour (0xff111114));
    for (auto* b : { &en, &ru })
    {
        const bool sel = (b == &ru) == (lang == Help::Lang::ru);
        b->setColour (TextButton::buttonColourId, sel ? t.accent : Colour (0xff26262b));
        b->setColour (TextButton::textColourOffId, sel ? Colour (0xff111114) : Colour (0xffececf0));
    }
    repaint();
}

Rectangle<float> WelcomePrompt::card() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (500.0f, 270.0f);
}

void WelcomePrompt::resized()
{
    const auto c = card().toNearestInt();
    en.setBounds (c.getX() + 170, c.getY() + 128, 100, 28);
    ru.setBounds (en.getRight() + 8, en.getY(), 100, 28);
    yes.setBounds (c.getRight() - 24 - 150, c.getBottom() - 24 - 34, 150, 34);
    notNow.setBounds (yes.getX() - 10 - 120, yes.getY(), 120, 34);
    never.setBounds (c.getX() + 24, yes.getY(), 170, 34);
}

void WelcomePrompt::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (Colours::black.withAlpha (0.62f));
    const auto c = card();
    g.setColour (Colour (0xff141417));
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (t.accent.withAlpha (0.7f));
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.2f);
    Logo::draw (g, Rectangle<float> (c.getX() + 24.0f, c.getY() + 22.0f, 54.0f, 56.0f), FillType (t.lightBg ? t.accent : t.logo));
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (19.0f, true));
    g.drawText (Help::ui ("Hi! Want a quick tour of Breathe?", "Привет! Показать, как работает Breathe?", lang),
                c.withTrimmedLeft (94.0f).withTrimmedRight (20.0f).withY (c.getY() + 28.0f).withHeight (26.0f), Justification::centredLeft);
    g.setColour (Colour (0xffa5a5ad));
    g.setFont (uiFont (13.0f));
    g.drawText (Help::ui ("It takes a minute and shows what every knob does.", "Займёт пару минут: покажу, что делает каждая ручка.", lang),
                c.withTrimmedLeft (94.0f).withTrimmedRight (20.0f).withY (c.getY() + 56.0f).withHeight (20.0f), Justification::centredLeft);
    g.setColour (Colour (0xffececf0));
    g.setFont (uiFont (13.0f, true));
    g.drawText (Help::ui ("Help language", "Язык подсказок", lang), Rectangle<float> (c.getX() + 24.0f, c.getY() + 128.0f, 140.0f, 28.0f), Justification::centredLeft);
}
