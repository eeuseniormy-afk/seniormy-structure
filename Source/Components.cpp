#include "Components.h"
#include "AGBinaryData.h"

using namespace juce;

Image instrumentImage (int inst)
{
    static std::array<Image, sms::NumInstruments> cache;
    auto& img = cache[(size_t) jlimit (0, (int) sms::NumInstruments - 1, inst)];
    if (! img.isValid())
    {
        int size = 0;
        if (const char* data = AGBinaryData::getNamedResource (sms::instrument (inst).imageName, size))
            img = ImageCache::getFromMemory (data, size);
    }
    return img;
}

static Colour accentOf (int inst) { return Colour (sms::instrument (inst).colour); }

//==============================================================================
void InstrumentFace::setInstrument (int inst, bool hasSamples)
{
    if (inst == instrument && hasSamples == samples) return;
    instrument = inst; samples = hasSamples; repaint();
}

void InstrumentFace::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    WoodLookAndFeel::paintPanel (g, r, 12.0f);
    const auto& d = sms::instrument (instrument);
    auto inner = r.reduced (12.0f);
    // título
    g.setColour (Wood::gold);
    g.setFont (WoodLookAndFeel::uiFont (12.5f, true));
    g.drawText (partLabel, inner.removeFromTop (16).toNearestInt(), Justification::centredLeft);
    g.setColour (Wood::ivory);
    g.setFont (WoodLookAndFeel::titleFont (24.0f));
    g.drawFittedText (String::fromUTF8 (d.namePt), inner.removeFromTop (30).toNearestInt(), Justification::centredLeft, 1, 0.7f);
    // selo de origem
    auto badgeRow = inner.removeFromTop (24);
    const auto src = (d.source != sms::Source::Modeled && ! samples) ? String::fromUTF8 ("Modelado (sem biblioteca)")
                                                                      : String::fromUTF8 (sms::sourceNamePt (d.source));
    const float bw = (float) GlyphArrangement::getStringWidthInt (WoodLookAndFeel::uiFont (12.0f, true), src) + 20.0f;
    auto badge = badgeRow.removeFromLeft (bw).reduced (0, 3);
    const Colour bc = d.source == sms::Source::Sampled ? Wood::gold : (d.source == sms::Source::Derived ? Wood::goldLight : Wood::violet);
    g.setColour (bc.withAlpha (0.18f)); g.fillRoundedRectangle (badge, 9.0f);
    g.setColour (bc); g.drawRoundedRectangle (badge, 9.0f, 1.0f);
    g.setFont (WoodLookAndFeel::uiFont (12.0f, true));
    g.drawText (src, badge.toNearestInt(), Justification::centred);
    // nota de origem (rodapé)
    auto foot = inner.removeFromBottom (34);
    g.setColour (Wood::ivorySoft);
    g.setFont (WoodLookAndFeel::uiFont (11.5f));
    g.drawFittedText (String::fromUTF8 (d.notePt), foot.toNearestInt(), Justification::centredLeft, 2, 0.85f);
    // ilustração com halo na cor do instrumento
    inner.removeFromTop (6);
    auto img = instrumentImage (instrument);
    auto halo = inner.withSizeKeepingCentre (inner.getWidth() * 0.9f, inner.getHeight() * 0.75f);
    ColourGradient hg (accentOf (instrument).withAlpha (0.28f), halo.getCentreX(), halo.getCentreY(),
                       Colours::transparentBlack, halo.getCentreX(), halo.getY() - 20.0f, true);
    g.setGradientFill (hg); g.fillEllipse (halo);
    if (img.isValid())
        g.drawImage (img, inner, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize);
}

//==============================================================================
Rectangle<float> InstrumentPicker::cell (int i) const
{
    const int cols = 5;
    const float w = (float) getWidth() / cols, h = (float) getHeight() / 2.0f;
    return { (float) (i % cols) * w, (float) (i / cols) * h, w, h };
}

int InstrumentPicker::cellIndexAt (Point<float> p) const
{
    for (int i = 0; i < sms::NumInstruments; ++i) if (cell (i).contains (p)) return i;
    return -1;
}

void InstrumentPicker::paint (Graphics& g)
{
    for (int i = 0; i < sms::NumInstruments; ++i)
    {
        auto c = cell (i).reduced (4.0f);
        const bool sel = i == selected, hov = i == hover;
        g.setColour (sel ? Wood::violetDim.withAlpha (0.55f) : Wood::black.withAlpha (hov ? 0.85f : 0.7f));
        g.fillRoundedRectangle (c, 8.0f);
        g.setColour (sel ? Wood::violet : (hov ? Wood::goldLight : Wood::goldDark));
        g.drawRoundedRectangle (c, 8.0f, sel ? 1.6f : 1.0f);
        auto txt = c.removeFromBottom (30.0f);
        auto img = instrumentImage (i);
        if (img.isValid()) g.drawImage (img, c.reduced (6.0f, 6.0f), RectanglePlacement::centred);
        g.setColour (Wood::ivory);
        g.setFont (WoodLookAndFeel::uiFont (12.5f, sel));
        g.drawFittedText (String::fromUTF8 (sms::instrument (i).namePt), txt.reduced (4, 0).toNearestInt(), Justification::centred, 2, 0.75f);
        const auto src = sms::instrument (i).source;
        if (src != sms::Source::Sampled)
        {
            g.setColour (src == sms::Source::Modeled ? Wood::violet : Wood::goldLight);
            g.fillEllipse (cell (i).reduced (4.0f).getRight() - 14.0f, cell (i).getY() + 10.0f, 7.0f, 7.0f);
        }
    }
}

void InstrumentPicker::mouseUp (const MouseEvent& e)
{
    const int i = cellIndexAt (e.position);
    if (i >= 0) { selected = i; repaint(); if (onPick) onPick (i); }
}

void InstrumentPicker::mouseMove (const MouseEvent& e)
{
    const int i = cellIndexAt (e.position);
    if (i != hover) { hover = i; repaint(); }
}

//==============================================================================
void ChordDiagram::set (int inst, int tun, int cp, const std::array<int, sms::kMaxCourses>& f, const String& chordName)
{
    if (inst == instrument && tun == tuning && cp == capo && f == frets && chordName == name) return;
    instrument = inst; tuning = tun; capo = cp; frets = f; name = chordName; repaint();
}

void ChordDiagram::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    WoodLookAndFeel::paintPanel (g, r, 10.0f, true);
    auto inner = r.reduced (12.0f, 10.0f);
    g.setColour (Wood::ivory);
    g.setFont (WoodLookAndFeel::titleFont (22.0f));
    g.drawText (name.isEmpty() ? String::fromUTF8 ("—") : name, inner.removeFromTop (28).toNearestInt(), Justification::centred);
    const auto& tun = sms::tuningOf (instrument, tuning);
    const int n = (int) tun.courses.size();
    if (n == 0) return;
    int lo = 99, hi = 0; bool any = false;
    for (int i = 0; i < n; ++i) if (frets[(size_t) i] > 0) { lo = jmin (lo, frets[(size_t) i]); hi = jmax (hi, frets[(size_t) i]); any = true; }
    const int first = (! any || hi <= 4) ? 1 : lo;
    const int numFrets = 5;
    auto board = inner.reduced (18.0f, 14.0f).withTrimmedTop (6.0f);
    const float sx = board.getWidth() / (float) jmax (1, n - 1);
    const float fy = board.getHeight() / (float) numFrets;
    // casas
    g.setColour (Wood::ivorySoft.withAlpha (0.7f));
    for (int f = 0; f <= numFrets; ++f)
        g.drawLine (board.getX(), board.getY() + f * fy, board.getRight(), board.getY() + f * fy, (f == 0 && first == 1) ? 3.0f : 1.0f);
    // cordas (dupla = linha dupla)
    for (int i = 0; i < n; ++i)
    {
        const float x = board.getX() + i * sx;
        g.setColour (Wood::goldLight.withAlpha (0.85f));
        if (tun.courses[(size_t) i].pair >= 0) { g.drawLine (x - 1.6f, board.getY(), x - 1.6f, board.getBottom(), 0.9f); g.drawLine (x + 1.6f, board.getY(), x + 1.6f, board.getBottom(), 0.9f); }
        else g.drawLine (x, board.getY(), x, board.getBottom(), 1.2f);
    }
    g.setFont (WoodLookAndFeel::uiFont (11.0f));
    if (first > 1) { g.setColour (Wood::ivorySoft); g.drawText (String (first) + "ª", Rectangle<float> (board.getX() - 30.0f, board.getY(), 26.0f, fy).toNearestInt(), Justification::centredRight); }
    if (capo > 0) { g.setColour (Wood::gold); g.drawText ("Capo " + String (capo), Rectangle<float> (board.getRight() - 60.0f, board.getY() - 18.0f, 60.0f, 14.0f).toNearestInt(), Justification::centredRight); }
    for (int i = 0; i < n; ++i)
    {
        const float x = board.getX() + i * sx;
        const int f = frets[(size_t) i];
        if (f == -2) continue;
        if (f < 0) { g.setColour (Wood::ivorySoft); g.drawText ("x", Rectangle<float> (x - 7.0f, board.getY() - 17.0f, 14.0f, 14.0f).toNearestInt(), Justification::centred); }
        else if (f == 0) { g.setColour (Wood::ivory); g.drawEllipse (x - 4.5f, board.getY() - 14.0f, 9.0f, 9.0f, 1.2f); }
        else
        {
            const int row = f - first;
            if (row < 0 || row >= numFrets) continue;
            const float y = board.getY() + (row + 0.5f) * fy;
            g.setColour (Wood::violet); g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
            g.setColour (Wood::ivory); g.drawEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f, 1.0f);
        }
    }
}

//==============================================================================
int PatternGrid::visibleRows() const { int k = 0; for (int r = 0; r < sms::NumRows; ++r) if (rowVisible (r)) ++k; return k; }
int PatternGrid::rowAtIndex (int idx) const { int k = 0; for (int r = 0; r < sms::NumRows; ++r) if (rowVisible (r)) { if (k == idx) return r; ++k; } return -1; }
int PatternGrid::indexOfRow (int r) const { int k = 0; for (int i = 0; i < r; ++i) if (rowVisible (i)) ++k; return k; }

Rectangle<float> PatternGrid::gridArea() const { return getLocalBounds().toFloat().withTrimmedLeft (130.0f).withTrimmedTop (18.0f).reduced (2.0f); }

bool PatternGrid::cellAt (Point<float> p, int& row, int& step) const
{
    auto a = gridArea();
    if (! a.contains (p)) return false;
    const int rows = visibleRows();
    const int idx = (int) ((p.y - a.getY()) / (a.getHeight() / (float) rows));
    step = (int) ((p.x - a.getX()) / (a.getWidth() / (float) pattern.numSteps));
    row = rowAtIndex (jlimit (0, rows - 1, idx));
    step = jlimit (0, pattern.numSteps - 1, step);
    return row >= 0;
}

void PatternGrid::paint (Graphics& g)
{
    auto a = gridArea();
    const int rows = visibleRows();
    const float rh = a.getHeight() / (float) rows, cw = a.getWidth() / (float) pattern.numSteps;
    g.setColour (Wood::well.withAlpha (0.85f)); g.fillRoundedRectangle (a.expanded (2.0f), 6.0f);
    // números dos tempos
    g.setFont (WoodLookAndFeel::uiFont (11.0f));
    for (int s = 0; s < pattern.numSteps; ++s)
    {
        if (s % pattern.stepsPerBeat == 0)
        {
            g.setColour (Wood::gold);
            g.drawText (String (s / pattern.stepsPerBeat + 1), Rectangle<float> (a.getX() + s * cw, 0, cw * 2.0f, 16.0f).toNearestInt(), Justification::centredLeft);
        }
    }
    for (int i = 0; i < rows; ++i)
    {
        const int r = rowAtIndex (i);
        const float y = a.getY() + i * rh;
        g.setColour (r >= sms::RS1 ? Wood::ivorySoft : Wood::ivory);
        g.setFont (WoodLookAndFeel::uiFont (12.5f, r < sms::RS1));
        g.drawFittedText (String::fromUTF8 (sms::rowNamePt (r)), Rectangle<float> (4.0f, y, 122.0f, rh).toNearestInt(), Justification::centredLeft, 1, 0.7f);
        for (int s = 0; s < pattern.numSteps; ++s)
        {
            auto c = Rectangle<float> (a.getX() + s * cw, y, cw, rh).reduced (1.5f, 2.0f);
            const bool beat = s % pattern.stepsPerBeat == 0;
            g.setColour (beat ? Colour (0xff24242a) : Colour (0xff18181c));
            if (s == playStep) g.setColour (Wood::goldDark.withAlpha (0.55f));
            g.fillRoundedRectangle (c, 3.0f);
            const uint8_t v = pattern.grid[(size_t) r][(size_t) s];
            if (v > 0)
            {
                const float k = (float) v / 127.0f;
                auto f = c.reduced (1.0f); f = f.withTrimmedTop (f.getHeight() * (1.0f - (0.35f + 0.65f * k)));
                g.setColour (r == sms::RTap || r == sms::RChuck ? Wood::gold.withAlpha (0.4f + 0.6f * k) : Wood::violet.withAlpha (0.35f + 0.65f * k));
                g.fillRoundedRectangle (f, 2.5f);
            }
        }
    }
    g.setColour (Wood::ivorySoft);
    g.setFont (WoodLookAndFeel::uiFont (11.0f));
    g.drawText (String::fromUTF8 ("clique: liga/desliga · arraste: pinta · botão direito ou Shift: muda a força"), Rectangle<float> (4.0f, 0, 126.0f, 16.0f).toNearestInt(), Justification::centredLeft);
    (void) indexOfRow (0);
}

void PatternGrid::mouseDown (const MouseEvent& e)
{
    int r, s;
    if (! cellAt (e.position, r, s)) return;
    auto& v = pattern.grid[(size_t) r][(size_t) s];
    if (e.mods.isPopupMenu() || e.mods.isShiftDown())
    {
        // ciclo de força: forte → médio → suave → fantasma → desligado
        v = (uint8_t) (v == 0 ? 120 : v >= 110 ? 96 : v >= 90 ? 72 : v >= 60 ? 48 : 0);
        paintValue = 255;
    }
    else
    {
        v = v > 0 ? 0 : 96;
        paintValue = v;
    }
    repaint();
    if (onEdit) onEdit();
}

void PatternGrid::mouseDrag (const MouseEvent& e)
{
    if (paintValue == 255) return;
    int r, s;
    if (! cellAt (e.position, r, s)) return;
    auto& v = pattern.grid[(size_t) r][(size_t) s];
    if (v != paintValue) { v = paintValue; repaint(); if (onEdit) onEdit(); }
}

//==============================================================================
void ProgressionStrip::setData (const sms::HarmonyState& h, int sel, int play)
{
    harmony = h; selected = sel; playing = play; repaint();
}

void ProgressionStrip::paint (Graphics& g)
{
    const int n = sms::kMaxProg;
    const float w = (float) getWidth() / (n / 2), h = (float) getHeight() / 2.0f;
    for (int i = 0; i < n; ++i)
    {
        auto c = Rectangle<float> ((float) (i % (n / 2)) * w, (float) (i / (n / 2)) * h, w, h).reduced (3.0f);
        const bool used = i < harmony.numProg;
        g.setColour (i == playing ? Wood::goldDark.withAlpha (0.8f) : (used ? Wood::black.withAlpha (0.85f) : Wood::black.withAlpha (0.35f)));
        g.fillRoundedRectangle (c, 6.0f);
        g.setColour (i == selected ? Wood::violet : Wood::goldDark.withAlpha (used ? 0.9f : 0.4f));
        g.drawRoundedRectangle (c, 6.0f, i == selected ? 1.8f : 1.0f);
        if (! used) { g.setColour (Wood::faint); g.setFont (WoodLookAndFeel::uiFont (16.0f)); g.drawText ("+", c.toNearestInt(), Justification::centred); continue; }
        const auto& st = harmony.prog[(size_t) i];
        sms::Chord ch; ch.root = st.root; ch.quality = (sms::Quality) st.quality; ch.bass = st.bass;
        sms::Key k; k.tonic = harmony.keyTonic; k.scale = (sms::Scale) harmony.keyScale;
        g.setColour (Wood::ivory);
        g.setFont (WoodLookAndFeel::uiFont (16.0f, true));
        g.drawFittedText (String::fromUTF8 (ch.name (k.usesFlats()).c_str()), c.withTrimmedBottom (14.0f).toNearestInt(), Justification::centred, 1, 0.6f);
        g.setColour (Wood::ivorySoft);
        g.setFont (WoodLookAndFeel::uiFont (11.0f));
        g.drawText (String (st.beats) + (st.beats == 1 ? " tempo" : " tempos"), c.withTrimmedTop (c.getHeight() - 16.0f).toNearestInt(), Justification::centred);
    }
}

void ProgressionStrip::mouseUp (const MouseEvent& e)
{
    const int n = sms::kMaxProg;
    const float w = (float) getWidth() / (n / 2), h = (float) getHeight() / 2.0f;
    const int i = jlimit (0, n / 2 - 1, (int) (e.position.x / w)) + (e.position.y >= h ? n / 2 : 0);
    if (onSelect) onSelect (i);
}

//==============================================================================
LabeledKnob::LabeledKnob (const String& name, double lo, double hi, double step, double def, const String& suffix)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
    slider.setRange (lo, hi, step);
    slider.setValue (def, dontSendNotification);
    slider.setDoubleClickReturnValue (true, def);
    slider.setTextValueSuffix (suffix);
    label.setText (name, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (WoodLookAndFeel::uiFont (12.5f));
    label.setColour (Label::textColourId, Wood::ivorySoft);
    label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}
