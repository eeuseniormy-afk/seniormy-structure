#include "LookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "AGBinaryData.h"

using namespace juce;

WoodLookAndFeel::WoodLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, Wood::wood);
    setColour (Label::textColourId, Wood::ivory);
    setColour (TextButton::buttonColourId, Wood::black);
    setColour (TextButton::buttonOnColourId, Wood::violetDim);
    setColour (TextButton::textColourOffId, Wood::ivory);
    setColour (TextButton::textColourOnId, Wood::ivory);
    setColour (ComboBox::backgroundColourId, Wood::well);
    setColour (ComboBox::textColourId, Wood::ivory);
    setColour (ComboBox::outlineColourId, Wood::goldDark);
    setColour (ComboBox::arrowColourId, Wood::gold);
    setColour (PopupMenu::backgroundColourId, Wood::black);
    setColour (PopupMenu::textColourId, Wood::ivory);
    setColour (PopupMenu::headerTextColourId, Wood::gold);
    setColour (PopupMenu::highlightedBackgroundColourId, Wood::violetDim);
    setColour (PopupMenu::highlightedTextColourId, Wood::ivory);
    setColour (TextEditor::backgroundColourId, Wood::well);
    setColour (TextEditor::textColourId, Wood::ivory);
    setColour (TextEditor::outlineColourId, Wood::goldDark);
    setColour (TextEditor::focusedOutlineColourId, Wood::gold);
    setColour (TextEditor::highlightColourId, Wood::violetDim);
    setColour (ToggleButton::textColourId, Wood::ivory);
    setColour (ToggleButton::tickColourId, Wood::violet);
    setColour (Slider::textBoxTextColourId, Wood::ivory);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ScrollBar::thumbColourId, Wood::gold);
    setColour (AlertWindow::backgroundColourId, Wood::black);
    setColour (AlertWindow::textColourId, Wood::ivory);
    setColour (AlertWindow::outlineColourId, Wood::gold);
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, Wood::violet.withAlpha (0.7f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Wood::gold.withAlpha (0.35f));
    setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.3f));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colours::black.withAlpha (0.5f));
    setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Wood::black);
    setColour (MidiKeyboardComponent::upDownButtonArrowColourId, Wood::gold);
}

Font WoodLookAndFeel::uiFont (float h, bool bold)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

Font WoodLookAndFeel::titleFont (float h)
{
    return Font (FontOptions ("Georgia", h, Font::plain));
}

void WoodLookAndFeel::paintWood (Graphics& g, Rectangle<float> area)
{
    static Image img = ImageCache::getFromMemory (AGBinaryData::woodbg_jpg, AGBinaryData::woodbg_jpgSize);
    if (img.isValid())
    {
        // cobre a área mantendo a proporção
        const float s = jmax (area.getWidth() / (float) img.getWidth(), area.getHeight() / (float) img.getHeight());
        const float w = (float) img.getWidth() * s, h = (float) img.getHeight() * s;
        g.drawImage (img, Rectangle<float> (area.getCentreX() - w / 2, area.getCentreY() - h / 2, w, h));
    }
    else g.fillAll (Wood::wood);
    // vinheta escura para dar leitura aos painéis
    ColourGradient vg (Colours::black.withAlpha (0.0f), area.getCentreX(), area.getCentreY(),
                       Colours::black.withAlpha (0.45f), area.getX(), area.getY(), true);
    g.setGradientFill (vg);
    g.fillRect (area);
}

void WoodLookAndFeel::paintPanel (Graphics& g, Rectangle<float> r, float radius, bool soft)
{
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 2), radius);
    g.setColour (soft ? Wood::panelSoft : Wood::panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (Wood::goldDark.withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (Wood::gold.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (3.0f), jmax (2.0f, radius - 3.0f), 0.6f);
}

void WoodLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = jmin (r.getWidth(), r.getHeight());
    auto c = r.withSizeKeepingCentre (d, d);
    const float ang = start + pos * (end - start);
    // trilha
    Path track; track.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.46f, d * 0.46f, 0, start, end, true);
    g.setColour (Wood::well); g.strokePath (track, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.46f, d * 0.46f, 0, start, ang, true);
    g.setColour (s.isEnabled() ? Wood::violet : Wood::faint);
    g.strokePath (val, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    // botão preto fosco com aro dourado
    auto k = c.reduced (d * 0.16f);
    ColourGradient kg (Colour (0xff2a2a2f), k.getCentreX(), k.getY(), Colour (0xff0d0d0f), k.getCentreX(), k.getBottom(), false);
    g.setGradientFill (kg); g.fillEllipse (k);
    g.setColour (Wood::gold); g.drawEllipse (k, 1.1f);
    Path p; p.addRoundedRectangle (-1.2f, -k.getHeight() * 0.45f, 2.4f, k.getHeight() * 0.3f, 1.0f);
    g.setColour (Wood::goldLight);
    g.fillPath (p, AffineTransform::rotation (ang).translated (k.getCentreX(), k.getCentreY()));
}

void WoodLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle style, Slider& s)
{
    const bool vertical = style == Slider::LinearVertical;
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (vertical)
    {
        auto tr = r.withSizeKeepingCentre (5.0f, r.getHeight() - 6.0f);
        g.setColour (Wood::well); g.fillRoundedRectangle (tr, 2.5f);
        g.setColour (s.isEnabled() ? Wood::violet : Wood::faint);
        g.fillRoundedRectangle (tr.withTop (pos), 2.5f);
        auto th = Rectangle<float> (r.getCentreX() - 11.0f, pos - 5.0f, 22.0f, 10.0f);
        g.setColour (Wood::black); g.fillRoundedRectangle (th, 3.0f);
        g.setColour (Wood::gold); g.drawRoundedRectangle (th, 3.0f, 1.0f);
        g.drawHorizontalLine ((int) pos, th.getX() + 3, th.getRight() - 3);
    }
    else
    {
        auto tr = r.withSizeKeepingCentre (r.getWidth() - 6.0f, 5.0f);
        g.setColour (Wood::well); g.fillRoundedRectangle (tr, 2.5f);
        g.setColour (s.isEnabled() ? Wood::violet : Wood::faint);
        g.fillRoundedRectangle (tr.withRight (pos), 2.5f);
        auto th = Rectangle<float> (pos - 5.0f, r.getCentreY() - 9.0f, 10.0f, 18.0f);
        g.setColour (Wood::black); g.fillRoundedRectangle (th, 3.0f);
        g.setColour (Wood::gold); g.drawRoundedRectangle (th, 3.0f, 1.0f);
    }
}

void WoodLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    Colour fill = on ? Wood::violetDim : Wood::black;
    if (down) fill = fill.brighter (0.15f); else if (over) fill = fill.brighter (0.07f);
    if (! b.isEnabled()) fill = fill.withAlpha (0.5f);
    g.setColour (fill); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? Wood::violet : (over ? Wood::goldLight : Wood::gold).withAlpha (b.isEnabled() ? 0.9f : 0.4f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void WoodLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour ((b.getToggleState() ? Wood::ivory : Wood::ivory).withAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (5, 2), Justification::centred, 2, 0.8f);
}

void WoodLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (4.0f);
    g.setColour (Wood::well); g.fillRoundedRectangle (box, 3.0f);
    g.setColour (over ? Wood::goldLight : Wood::gold); g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        g.setColour (Wood::violet);
        g.fillRoundedRectangle (box.reduced (3.5f), 2.0f);
    }
    g.setColour (Wood::ivory.withAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (uiFont (jmin (14.0f, r.getHeight() * 0.62f)));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (3), Justification::centredLeft, 2, 0.8f);
}

void WoodLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& cb)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Wood::well); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cb.hasKeyboardFocus (true) ? Wood::goldLight : Wood::goldDark); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path a; const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    a.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (Wood::gold); g.fillPath (a);
}

void WoodLookAndFeel::positionComboBoxText (ComboBox& cb, Label& l)
{
    l.setBounds (4, 1, cb.getWidth() - 24, cb.getHeight() - 2);
    l.setFont (getComboBoxFont (cb));
    l.setMinimumHorizontalScale (0.75f);
}

Font WoodLookAndFeel::getComboBoxFont (ComboBox& cb) { return uiFont (jmin (14.5f, (float) cb.getHeight() * 0.56f)); }
Font WoodLookAndFeel::getPopupMenuFont() { return uiFont (14.5f); }
Font WoodLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (14.5f, (float) h * 0.5f)); }
Font WoodLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

void WoodLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Wood::black);
    g.setColour (Wood::goldDark); g.drawRect (0, 0, w, h, 1);
}

void WoodLookAndFeel::drawTabButton (TabBarButton& b, Graphics& g, bool over, bool)
{
    auto r = b.getActiveArea().toFloat().reduced (2.0f, 3.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? Wood::black : Wood::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (front ? Wood::gold : (over ? Wood::goldLight : Wood::goldDark));
    g.drawRoundedRectangle (r, 6.0f, front ? 1.4f : 1.0f);
    if (front) { g.setColour (Wood::violet); g.fillRect (r.withTop (r.getBottom() - 2.5f).reduced (10.0f, 0)); }
    g.setColour (front ? Wood::ivory : Wood::ivorySoft);
    g.setFont (uiFont (15.0f, front));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), Justification::centred, 1, 0.8f);
}

int WoodLookAndFeel::getTabButtonBestWidth (TabBarButton& b, int)
{
    return jmax (120, GlyphArrangement::getStringWidthInt (uiFont (15.0f, true), b.getButtonText()) + 40);
}

void WoodLookAndFeel::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical,
                                     int thumbStart, int thumbSize, bool, bool)
{
    g.setColour (Wood::well.withAlpha (0.6f)); g.fillRect (x, y, w, h);
    auto t = vertical ? Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize) : Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (Wood::gold.withAlpha (0.8f)); g.fillRoundedRectangle (t.toFloat(), 3.0f);
}
