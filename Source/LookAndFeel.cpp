#include "LookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

using namespace juce;

Colour Sky::sectionColour (int s)
{
    static const uint32 c[] = { 0xff8e6f3a, 0xffa08550, 0xff5f7a8f, 0xff6c5c7e, 0xff4a5a4c, 0xff9a6a44 };
    return Colour (c[jlimit (0, 5, s)]);
}

Colour Sky::articColour (int a)
{
    static const uint32 c[] = { 0xffbf9f5f, 0xffd8c08a, 0xff4f6f96, 0xff7b5f8f, 0xff5f8a7a };
    return Colour (c[jlimit (0, 4, a)]);
}

SkyLookAndFeel::SkyLookAndFeel()
{
    setColour (Label::textColourId, Sky::ink);
    setColour (Slider::textBoxTextColourId, Sky::ink);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, Sky::ink);
    setColour (ComboBox::backgroundColourId, Sky::white.withAlpha (0.85f));
    setColour (ComboBox::outlineColourId, Sky::ink);
    setColour (ComboBox::arrowColourId, Sky::ink);
    setColour (PopupMenu::backgroundColourId, Sky::white);
    setColour (PopupMenu::textColourId, Sky::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, Sky::goldLight);
    setColour (PopupMenu::highlightedTextColourId, Sky::ink);
    setColour (TextButton::textColourOffId, Sky::ink);
    setColour (TextButton::textColourOnId, Sky::ink);
    setColour (TextEditor::backgroundColourId, Sky::white.withAlpha (0.92f));
    setColour (TextEditor::textColourId, Sky::ink);
    setColour (TextEditor::outlineColourId, Sky::ink);
    setColour (TextEditor::focusedOutlineColourId, Sky::goldDark);
    setColour (TextEditor::highlightColourId, Sky::goldLight.withAlpha (0.6f));
    setColour (CaretComponent::caretColourId, Sky::ink);
    setColour (ToggleButton::textColourId, Sky::ink);
    setColour (ToggleButton::tickColourId, Sky::ink);
    setColour (ScrollBar::thumbColourId, Sky::inkSoft);
    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (ListBox::textColourId, Sky::ink);
    setColour (AlertWindow::backgroundColourId, Sky::white);
    setColour (AlertWindow::textColourId, Sky::ink);
    setColour (AlertWindow::outlineColourId, Sky::ink);
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, Sky::gold.withAlpha (0.75f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Sky::goldLight.withAlpha (0.35f));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Sky::ink.withAlpha (0.6f));
    setColour (MidiKeyboardComponent::whiteNoteColourId, Sky::white);
    setColour (MidiKeyboardComponent::blackNoteColourId, Sky::ink);
    setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Sky::panelAlt);
    setColour (MidiKeyboardComponent::upDownButtonArrowColourId, Sky::ink);
    setColour (MidiKeyboardComponent::textLabelColourId, Sky::inkSoft);
}

Font SkyLookAndFeel::uiFont (float h, bool bold)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

Font SkyLookAndFeel::titleFont (float h)
{
    // serifada (clássica, orquestral) quando disponível
    auto f = Font (FontOptions ("Georgia", h, Font::plain));
    return f;
}

void SkyLookAndFeel::paintSky (Graphics& g, Rectangle<float> a, float time)
{
    ColourGradient grad (Sky::zenith, a.getX(), a.getY(), Sky::horizon, a.getX(), a.getBottom(), false);
    grad.addColour (0.38, Sky::midSky);
    grad.addColour (0.72, Sky::lowSky);
    g.setGradientFill (grad);
    g.fillRect (a);

    // estrelas sutis no terço superior
    Random rnd (20261025);
    for (int i = 0; i < 90; ++i)
    {
        const float x = a.getX() + rnd.nextFloat() * a.getWidth();
        const float y = a.getY() + std::pow (rnd.nextFloat(), 1.8f) * a.getHeight() * 0.42f;
        const float s = 0.6f + rnd.nextFloat() * 1.3f;
        const float tw = 0.5f + 0.5f * std::sin (time * (0.4f + rnd.nextFloat()) + (float) i);
        const float alpha = (0.15f + 0.45f * rnd.nextFloat()) * (1.0f - (y - a.getY()) / (a.getHeight() * 0.45f)) * (0.7f + 0.3f * tw);
        g.setColour (Sky::goldLight.withAlpha (jlimit (0.0f, 1.0f, alpha)));
        g.fillEllipse (x, y, s, s);
    }

    // nuvens: elipses suaves sobrepostas, deriva lenta
    Random cr (777);
    for (int c = 0; c < 7; ++c)
    {
        const float cy = a.getY() + a.getHeight() * (0.45f + 0.5f * cr.nextFloat());
        float cx = a.getX() + a.getWidth() * cr.nextFloat() + time * 3.0f * (0.5f + cr.nextFloat());
        cx = a.getX() + std::fmod (cx - a.getX() + a.getWidth() * 0.3f, a.getWidth() * 1.3f) - a.getWidth() * 0.15f;
        const float w = a.getWidth() * (0.18f + 0.22f * cr.nextFloat());
        for (int k = 0; k < 6; ++k)
        {
            const float ox = (cr.nextFloat() - 0.5f) * w, oy = (cr.nextFloat() - 0.5f) * w * 0.12f;
            const float ew = w * (0.35f + 0.4f * cr.nextFloat()), eh = ew * 0.28f;
            ColourGradient cg (Colours::white.withAlpha (0.10f), cx + ox, cy + oy, Colours::white.withAlpha (0.0f), cx + ox + ew * 0.5f, cy + oy, true);
            g.setGradientFill (cg);
            g.fillEllipse (cx + ox - ew * 0.5f, cy + oy - eh * 0.5f, ew, eh);
        }
    }
}

void SkyLookAndFeel::paintPanel (Graphics& g, Rectangle<float> r, bool dark, float radius)
{
    g.setColour (dark ? Sky::panelDark : Sky::panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (Sky::ink.withAlpha (dark ? 0.9f : 0.85f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.2f);
    g.setColour (Sky::gold.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (3.0f), jmax (1.0f, radius - 3.0f), 0.8f);
}

void SkyLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    auto c = b.withSizeKeepingCentre (d, d);
    const float ang = start + pos * (end - start);
    const float r = d * 0.5f;

    // trilha
    Path track; track.addCentredArc (c.getCentreX(), c.getCentreY(), r - 2, r - 2, 0, start, end, true);
    g.setColour (Sky::ink.withAlpha (0.25f));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc (c.getCentreX(), c.getCentreY(), r - 2, r - 2, 0, start, ang, true);
    g.setColour (s.isEnabled() ? Sky::gold : Sky::inkFaint);
    g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // botão: preto fosco com anel dourado acetinado
    auto knob = c.reduced (d * 0.17f);
    g.setColour (Sky::ink);
    g.fillEllipse (knob);
    g.setColour (Sky::goldDark);
    g.drawEllipse (knob.reduced (1.0f), 1.2f);
    Path p;
    const float pr = knob.getWidth() * 0.5f;
    p.addRoundedRectangle (-1.4f, -pr + 3.0f, 2.8f, pr * 0.55f, 1.2f);
    g.setColour (Sky::goldLight);
    g.fillPath (p, AffineTransform::rotation (ang).translated (knob.getCentreX(), knob.getCentreY()));
}

void SkyLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle st, Slider& s)
{
    const bool vert = st == Slider::LinearVertical || st == Slider::LinearBarVertical;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (vert)
    {
        auto tr = b.withSizeKeepingCentre (4.0f, b.getHeight() - 6.0f);
        g.setColour (Sky::ink.withAlpha (0.3f));
        g.fillRoundedRectangle (tr, 2.0f);
        auto filled = tr.withTop (pos);
        g.setColour (s.isEnabled() ? Sky::gold : Sky::inkFaint);
        g.fillRoundedRectangle (filled, 2.0f);
        auto th = Rectangle<float> (b.getCentreX() - 9.0f, pos - 4.0f, 18.0f, 8.0f);
        g.setColour (Sky::ink);
        g.fillRoundedRectangle (th, 2.0f);
        g.setColour (Sky::goldLight);
        g.fillRect (th.reduced (3.0f, 3.5f));
    }
    else
    {
        auto tr = b.withSizeKeepingCentre (b.getWidth() - 6.0f, 4.0f);
        g.setColour (Sky::ink.withAlpha (0.3f));
        g.fillRoundedRectangle (tr, 2.0f);
        g.setColour (s.isEnabled() ? Sky::gold : Sky::inkFaint);
        g.fillRoundedRectangle (tr.withRight (pos), 2.0f);
        auto th = Rectangle<float> (pos - 4.0f, b.getCentreY() - 8.0f, 8.0f, 16.0f);
        g.setColour (Sky::ink);
        g.fillRoundedRectangle (th, 2.0f);
        g.setColour (Sky::goldLight);
        g.fillRect (th.reduced (3.5f, 3.0f));
    }
}

void SkyLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    Colour fill = on ? Sky::gold : Sky::white.withAlpha (0.82f);
    if (over) fill = fill.interpolatedWith (Sky::goldLight, 0.3f);
    if (down) fill = fill.darker (0.12f);
    if (! b.isEnabled()) fill = fill.withAlpha (0.4f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r, 6.0f, 1.1f);
}

void SkyLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (Sky::ink.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), Justification::centred, 2);
}

void SkyLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (jmin (r.getHeight(), 20.0f)).reduced (3.0f);
    g.setColour (b.getToggleState() ? Sky::gold : Sky::white.withAlpha (over ? 0.95f : 0.8f));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (box, 3.0f, 1.1f);
    if (b.getToggleState())
    {
        Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.22f);
        g.strokePath (tick, PathStrokeType (1.8f));
    }
    g.setColour (Sky::ink.withAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.setFont (uiFont (13.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), Justification::centredLeft, 2);
}

void SkyLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& c)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Sky::white.withAlpha (c.isEnabled() ? 0.86f : 0.45f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r, 5.0f, 1.1f);
    Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Sky::goldDark);
    g.fillPath (arrow);
}

Font SkyLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (13.0f); }
Font SkyLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }
Font SkyLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (14.0f, (float) h * 0.5f), true); }
Font SkyLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

void SkyLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Sky::white);
    g.setColour (Sky::ink);
    g.drawRect (0, 0, w, h, 1);
}

int SkyLookAndFeel::getTabButtonBestWidth (TabBarButton& b, int)
{
    return jmax (110, (int) GlyphArrangement::getStringWidth (uiFont (14.0f, true), b.getButtonText()) + 40);
}

void SkyLookAndFeel::drawTabButton (TabBarButton& b, Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? Sky::white.withAlpha (0.92f) : (over ? Sky::white.withAlpha (0.35f) : Sky::white.withAlpha (0.16f)));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (front ? Sky::ink : Sky::white.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    if (front)
    {
        g.setColour (Sky::gold);
        g.fillRoundedRectangle (r.removeFromBottom (3.0f).reduced (14.0f, 0.0f), 1.5f);
    }
    g.setColour (front ? Sky::ink : Sky::white);
    g.setFont (uiFont (14.0f, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

void SkyLookAndFeel::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool, bool)
{
    Rectangle<int> thumb = vertical ? Rectangle<int> (x + 2, ts, w - 4, tsz) : Rectangle<int> (ts, y + 2, tsz, h - 4);
    g.setColour (Sky::inkSoft.withAlpha (0.6f));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}
