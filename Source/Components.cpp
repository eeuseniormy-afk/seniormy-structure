#include "Components.h"
#include "LoopImporter.h"

using namespace juce;

//==============================================================================
Knob::Knob (APVTS& s, const String& id, const String& text)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (Slider::textBoxTextColourId, Sky::inkSoft);
    addAndMakeVisible (slider);
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (SkyLookAndFeel::uiFont (12.5f, true));
    label.setColour (Label::textColourId, Sky::ink);
    addAndMakeVisible (label);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

ParamToggle::ParamToggle (APVTS& s, const String& id, const String& text)
{
    setButtonText (text);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

ParamCombo::ParamCombo (APVTS& s, const String& id, const String& text)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (text, dontSendNotification);
    label.setFont (SkyLookAndFeel::uiFont (12.0f, true));
    label.setColour (Label::textColourId, Sky::ink);
    addAndMakeVisible (label);
    att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
}

void ParamCombo::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.reduced (0, 1));
}

//==============================================================================
void Panel::paint (Graphics& g)
{
    SkyLookAndFeel::paintPanel (g, getLocalBounds().toFloat().reduced (1.0f), dark, 10.0f);
    if (title.isNotEmpty())
    {
        auto r = getLocalBounds().reduced (14, 8).removeFromTop (18);
        g.setColour (dark ? Sky::goldLight : Sky::ink);
        g.setFont (SkyLookAndFeel::titleFont (16.0f));
        g.drawText (title, r, Justification::centredLeft);
        const float tw = GlyphArrangement::getStringWidth (SkyLookAndFeel::titleFont (16.0f), title);
        g.setColour (Sky::gold);
        g.fillRect ((float) r.getX() + tw + 10.0f, (float) r.getCentreY(), jmax (0.0f, (float) r.getWidth() - tw - 10.0f), 1.0f);
    }
}

//==============================================================================
void ChordDisplay::set (int root, int quality, int bass, float confidence, int keyTonic, int keyScale, bool lk)
{
    sms::Key k { keyTonic, (sms::Scale) keyScale };
    sms::Chord c; c.root = root; c.quality = (sms::Quality) quality; c.bass = bass;
    const auto name = root < 0 ? String::fromUTF8 ("—") : String::fromUTF8 (c.name (k.usesFlats()).c_str());
    const auto kn = String::fromUTF8 ("Tonalidade: ") + String::fromUTF8 (k.name().c_str());
    if (name != chord || kn != key || std::abs (confidence - conf) > 0.02f || lk != locked)
    {
        chord = name; key = kn; conf = confidence; locked = lk;
        repaint();
    }
}

void ChordDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::white.withAlpha (0.75f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    auto t = r.reduced (12.0f, 6.0f);
    g.setFont (SkyLookAndFeel::uiFont (11.5f, true));
    g.setColour (Sky::inkSoft);
    g.drawText (String::fromUTF8 ("ACORDE ATUAL"), t.removeFromTop (14.0f), Justification::centredLeft);
    auto bottom = t.removeFromBottom (16.0f);
    g.setFont (SkyLookAndFeel::titleFont (jmin (54.0f, t.getHeight())));
    g.setColour (Sky::ink);
    g.drawFittedText (chord, t.toNearestInt(), Justification::centredLeft, 1);
    if (locked)
    {
        g.setFont (SkyLookAndFeel::uiFont (11.0f, true));
        g.setColour (Sky::goldDark);
        g.drawText (String::fromUTF8 ("TRAVADO"), t.removeFromRight (70.0f).removeFromTop (16.0f), Justification::centredRight);
    }
    g.setFont (SkyLookAndFeel::uiFont (12.0f));
    g.setColour (Sky::inkSoft);
    g.drawText (key, bottom.removeFromLeft (bottom.getWidth() * 0.65f), Justification::centredLeft);
    auto bar = bottom.reduced (4.0f, 5.0f);
    g.setColour (Sky::ink.withAlpha (0.18f));
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (Sky::gold);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, conf)), 2.0f);
}

//==============================================================================
void ChromaWheel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float d = jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto c = r.withSizeKeepingCentre (d, d);
    const float R = d * 0.5f, inner = R * 0.36f;
    g.setColour (Sky::white.withAlpha (0.6f));
    g.fillEllipse (c);
    g.setColour (Sky::ink);
    g.drawEllipse (c.reduced (0.5f), 1.0f);
    // ciclo de quintas
    static const int order[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };
    for (int i = 0; i < 12; ++i)
    {
        const int p = order[i];
        const float a0 = MathConstants<float>::twoPi * ((float) i - 0.45f) / 12.0f - MathConstants<float>::halfPi;
        const float a1 = MathConstants<float>::twoPi * ((float) i + 0.45f) / 12.0f - MathConstants<float>::halfPi;
        const float v = jlimit (0.0f, 1.0f, chroma[(size_t) p]);
        const float rr = inner + (R - inner - 14.0f) * v;
        Path seg;
        seg.addPieSegment (c.getCentreX() - rr, c.getCentreY() - rr, rr * 2, rr * 2, a0 + MathConstants<float>::halfPi, a1 + MathConstants<float>::halfPi, inner / rr);
        const bool inChord = (mask >> p) & 1u;
        g.setColour (inChord ? (p == root ? Sky::goldDark : Sky::gold) : Sky::midSky.withAlpha (0.55f));
        g.fillPath (seg);
        const float am = 0.5f * (a0 + a1);
        const float lx = c.getCentreX() + std::cos (am) * (R - 8.0f), ly = c.getCentreY() + std::sin (am) * (R - 8.0f);
        g.setColour (Sky::ink);
        g.setFont (SkyLookAndFeel::uiFont (10.5f, inChord));
        g.drawText (sms::pcName (p), Rectangle<float> (lx - 12, ly - 7, 24, 14), Justification::centred);
    }
    g.setColour (Sky::ink);
    g.fillEllipse (c.withSizeKeepingCentre (inner * 1.6f, inner * 1.6f));
    g.setColour (Sky::goldLight);
    g.setFont (SkyLookAndFeel::uiFont (10.0f, true));
    g.drawText ("CROMA", c.withSizeKeepingCentre (inner * 1.6f, 14.0f), Justification::centred);
}

//==============================================================================
void LevelMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::ink.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 2.0f);
    const float db = level > 0.00001f ? 20.0f * std::log10 (level) : -100.0f;
    const float n = jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f);
    auto f = vertical ? r.reduced (1.5f).withTrimmedTop (r.getHeight() * (1.0f - n)) : r.reduced (1.5f).withWidth ((r.getWidth() - 3.0f) * n);
    g.setColour (db > -1.0f ? Colour (0xffb5533c) : Sky::gold);
    g.fillRoundedRectangle (f, 1.5f);
}

//==============================================================================
SectionStrip::SectionStrip (APVTS& s, const String& id, int sec) : section (sec)
{
    fader.setSliderStyle (Slider::LinearVertical);
    fader.setTextBoxStyle (Slider::TextBoxBelow, false, 56, 14);
    fader.setColour (Slider::textBoxTextColourId, Sky::inkSoft);
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, fader);
    fader.setDoubleClickReturnValue (true, 0.0);
}

void SectionStrip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::sectionColour (section));
    g.fillRoundedRectangle (r.removeFromTop (3.0f).reduced (6.0f, 0.0f), 1.5f);
    g.setColour (Sky::ink);
    g.setFont (SkyLookAndFeel::uiFont (11.0f, true));
    static const char* shortNames[6] = { "Vln I", "Vln II", "Vla", "Vc", "Cb", "Tpa" };
    g.drawText (shortNames[jlimit (0, 5, section)], getLocalBounds().removeFromTop (18).withTrimmedTop (3), Justification::centred);
}

void SectionStrip::resized()
{
    auto r = getLocalBounds().withTrimmedTop (20);
    meter.setBounds (r.removeFromRight (6).withTrimmedBottom (18).reduced (0, 4));
    fader.setBounds (r);
}

//==============================================================================
void LaneGrid::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::white.withAlpha (0.88f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (lane == nullptr) return;
    auto ga = gridArea();
    if (lane->type != sms::LaneType::Pattern)
    {
        g.setColour (Sky::inkSoft);
        g.setFont (SkyLookAndFeel::uiFont (14.0f));
        String t;
        switch (lane->type)
        {
            case sms::LaneType::Pad:   t = String::fromUTF8 ("Faixa de PAD: sustenta o voicing do acorde (condução de vozes suave). Notas comuns ficam ligadas entre os acordes."); break;
            case sms::LaneType::Line:  t = String::fromUTF8 ("Faixa de LINHA: melodia legato no topo, em notas do acorde e passagens da escala. A densidade controla o movimento."); break;
            case sms::LaneType::Tutti: t = String::fromUTF8 ("Faixa TUTTI: orquestra inteira com dobras de oitava e acentos nas trocas de acorde."); break;
            default: break;
        }
        g.drawFittedText (t + String::fromUTF8 ("\nMude o tipo para \"Padrão\" para desenhar um ritmo nesta grade."), getLocalBounds().reduced (24), Justification::centred, 4);
        return;
    }
    const int steps = jlimit (1, sms::kMaxSteps, lane->numSteps);
    const float cw = ga.getWidth() / (float) steps, rh = ga.getHeight() / (float) sms::kRows;
    static const char* rowNames[6] = { "Baixo", "Voz 2", "Voz 3", "Voz 4", "Voz 5", "Topo +" };
    for (int row = 0; row < sms::kRows; ++row)
    {
        const float y = ga.getBottom() - (float) (row + 1) * rh;
        g.setColour (row % 2 ? Sky::horizon.withAlpha (0.25f) : Colours::transparentBlack);
        g.fillRect (ga.getX(), y, ga.getWidth(), rh);
        g.setColour (Sky::ink);
        g.setFont (SkyLookAndFeel::uiFont (11.5f, row == 0));
        g.drawText (String::fromUTF8 (rowNames[row]), Rectangle<float> (4.0f, y, 58.0f, rh), Justification::centredLeft);
    }
    for (int s = 0; s <= steps; ++s)
    {
        const float x = ga.getX() + (float) s * cw;
        const bool beat = s % jmax (1, lane->stepsPerBeat) == 0;
        g.setColour (Sky::ink.withAlpha (beat ? 0.5f : 0.15f));
        g.drawVerticalLine ((int) x, ga.getY(), ga.getBottom());
        if (beat && s < steps)
        {
            g.setFont (SkyLookAndFeel::uiFont (10.0f));
            g.drawText (String (s / jmax (1, lane->stepsPerBeat) + 1), Rectangle<float> (x + 2, 1.0f, 30.0f, 14.0f), Justification::centredLeft);
        }
    }
    if (playStep >= 0 && playStep < steps)
    {
        g.setColour (Sky::gold.withAlpha (0.18f));
        g.fillRect (ga.getX() + (float) playStep * cw, ga.getY(), cw, ga.getHeight());
    }
    const auto col = Sky::articColour (lane->artic);
    for (int row = 0; row < sms::kRows; ++row)
        for (int s = 0; s < steps; ++s)
        {
            const int v = lane->grid[(size_t) row][(size_t) s];
            if (v <= 0) continue;
            auto cell = Rectangle<float> (ga.getX() + (float) s * cw, ga.getBottom() - (float) (row + 1) * rh, cw, rh).reduced (2.0f, 3.0f);
            auto len = cell.withWidth (jmax (4.0f, cell.getWidth() * jlimit (0.15f, 1.0f, lane->gate)));
            g.setColour (col.withAlpha (0.35f + 0.65f * (float) v / 127.0f));
            g.fillRoundedRectangle (len, 3.0f);
            g.setColour (Sky::ink);
            g.drawRoundedRectangle (len, 3.0f, 1.0f);
        }
    g.setColour (Sky::inkSoft);
    g.setFont (SkyLookAndFeel::uiFont (10.5f));
    g.drawText (String::fromUTF8 ("clique: liga/desliga · arraste para cima/baixo: velocity"), getLocalBounds().removeFromTop (15).withTrimmedRight (6), Justification::centredRight);
}

bool LaneGrid::cellAt (Point<float> p, int& row, int& step) const
{
    if (lane == nullptr || lane->type != sms::LaneType::Pattern) return false;
    auto ga = gridArea();
    if (! ga.contains (p)) return false;
    const int steps = jlimit (1, sms::kMaxSteps, lane->numSteps);
    step = jlimit (0, steps - 1, (int) ((p.x - ga.getX()) / ga.getWidth() * (float) steps));
    row = jlimit (0, sms::kRows - 1, (int) ((ga.getBottom() - p.y) / ga.getHeight() * (float) sms::kRows));
    return true;
}

void LaneGrid::mouseDown (const MouseEvent& e)
{
    int row, step;
    if (! cellAt (e.position, row, step)) return;
    auto& v = lane->grid[(size_t) row][(size_t) step];
    painting = v == 0;
    v = painting ? (uint8_t) 100 : (uint8_t) 0;
    dragRow = row; dragStep = step; dragStartY = e.position.y; dragStartVel = v;
    repaint();
    if (onChange) onChange();
}

void LaneGrid::mouseDrag (const MouseEvent& e)
{
    if (lane == nullptr || dragRow < 0) return;
    int row, step;
    if (e.mods.isShiftDown() || (std::abs (e.position.y - dragStartY) > 6.0f && cellAt (e.position, row, step) && step == dragStep))
    {
        // ajuste de velocity arrastando na vertical sobre a mesma célula
        auto& v = lane->grid[(size_t) dragRow][(size_t) dragStep];
        if (v > 0 || painting)
        {
            v = (uint8_t) jlimit (1, 127, dragStartVel + (int) ((dragStartY - e.position.y) * 1.2f));
            repaint();
            if (onChange) onChange();
        }
        return;
    }
    if (cellAt (e.position, row, step) && (step != dragStep || row != dragRow))
    {
        lane->grid[(size_t) row][(size_t) step] = painting ? (uint8_t) 100 : (uint8_t) 0;
        dragRow = row; dragStep = step; dragStartY = e.position.y; dragStartVel = lane->grid[(size_t) row][(size_t) step];
        repaint();
        if (onChange) onChange();
    }
}

void LaneGrid::mouseUp (const MouseEvent&) { dragRow = dragStep = -1; }

//==============================================================================
void GeneratedRoll::push (const sms::DisplayEvent& e)
{
    if (e.on)
    {
        bars.push_back ({ e.ppq, e.ppq, e.note, e.section, e.artic, e.lane, (float) e.velocity / 127.0f, true });
    }
    else
    {
        for (auto it = bars.rbegin(); it != bars.rend(); ++it)
            if (it->open && it->note == e.note && it->section == e.section && it->lane == e.lane) { it->open = false; it->end = e.ppq; break; }
    }
    if (bars.size() > 1500) bars.erase (bars.begin(), bars.begin() + 300);
}

void GeneratedRoll::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::panelDark);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto a = r.reduced (8.0f, 6.0f).withTrimmedLeft (26.0f);
    const double span = 8.0;   // tempos visíveis
    const double t0 = now - span;
    const int lo = 28, hi = 100;
    // linhas de oitava
    for (int n = lo; n <= hi; ++n)
        if (n % 12 == 0)
        {
            const float y = a.getBottom() - (float) (n - lo) / (float) (hi - lo) * a.getHeight();
            g.setColour (Sky::goldLight.withAlpha (0.18f));
            g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            g.setColour (Sky::goldLight.withAlpha (0.7f));
            g.setFont (SkyLookAndFeel::uiFont (10.0f));
            g.drawText ("C" + String (n / 12 - 1), Rectangle<float> (r.getX() + 4, y - 7, 26, 14), Justification::centredLeft);
        }
    for (double b = std::ceil (t0); b <= now; b += 1.0)
    {
        const float x = a.getX() + (float) ((b - t0) / span) * a.getWidth();
        g.setColour (Colours::white.withAlpha (std::fmod (b, 4.0) == 0.0 ? 0.22f : 0.08f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    }
    const float nh = jmax (2.0f, a.getHeight() / (float) (hi - lo));
    for (auto& b : bars)
    {
        if (laneFilter >= 0 && b.lane != laneFilter) continue;
        const double end = b.open ? now : b.end;
        if (end < t0) continue;
        const float x0 = a.getX() + (float) ((jmax (b.start, t0) - t0) / span) * a.getWidth();
        const float x1 = a.getX() + (float) ((end - t0) / span) * a.getWidth();
        const float y = a.getBottom() - (float) (b.note - lo) / (float) (hi - lo) * a.getHeight();
        g.setColour (Sky::sectionColour (b.section).brighter (0.6f).withAlpha (0.45f + 0.55f * b.vel));
        g.fillRoundedRectangle (x0, y - nh * 0.5f, jmax (3.0f, x1 - x0), nh, 1.5f);
    }
    g.setColour (Sky::gold);
    g.drawVerticalLine ((int) a.getRight() - 1, a.getY(), a.getBottom());
    // legenda
    auto leg = r.reduced (10.0f, 6.0f).removeFromTop (14.0f);
    for (int s = 0; s < 6; ++s)
    {
        auto cell = leg.removeFromRight (76.0f);
        g.setColour (Sky::sectionColour (s).brighter (0.6f));
        g.fillRoundedRectangle (cell.removeFromLeft (10.0f).reduced (0.0f, 3.0f), 2.0f);
        g.setColour (Sky::white.withAlpha (0.85f));
        g.setFont (SkyLookAndFeel::uiFont (10.5f));
        g.drawText (String::fromUTF8 (sms::sectionName (s)), cell.withTrimmedLeft (4.0f), Justification::centredLeft);
    }
}

//==============================================================================
void ProgressionView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Sky::white.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (! prog || prog->empty())
    {
        g.setColour (Sky::inkSoft);
        g.setFont (SkyLookAndFeel::uiFont (14.0f));
        g.drawText (String::fromUTF8 ("Nenhum loop importado ainda."), getLocalBounds(), Justification::centred);
        return;
    }
    auto a = r.reduced (8.0f).withTrimmedBottom (18.0f);
    const double L = prog->lengthBeats;
    const sms::Key k = prog->key;
    for (auto& c : prog->chords)
    {
        const float x0 = a.getX() + (float) (c.startBeat / L) * a.getWidth();
        const float w = (float) (c.lengthBeats / L) * a.getWidth();
        auto cell = Rectangle<float> (x0, a.getY(), w, a.getHeight()).reduced (2.0f, 0.0f);
        g.setColour (Sky::lowSky.withAlpha (0.35f));
        g.fillRoundedRectangle (cell, 5.0f);
        g.setColour (Sky::ink);
        g.drawRoundedRectangle (cell, 5.0f, 1.0f);
        g.setFont (SkyLookAndFeel::titleFont (jmin (26.0f, cell.getHeight() * 0.5f)));
        g.drawFittedText (String::fromUTF8 (c.chord.name (k.usesFlats()).c_str()), cell.toNearestInt().reduced (4), Justification::centred, 1);
    }
    for (int b = 0; b <= (int) L; b += 4)
    {
        const float x = a.getX() + (float) (b / L) * a.getWidth();
        g.setColour (Sky::inkSoft);
        g.setFont (SkyLookAndFeel::uiFont (10.0f));
        if (b < (int) L) g.drawText (String (b / 4 + 1), Rectangle<float> (x + 3, a.getBottom() + 2, 30, 14), Justification::centredLeft);
    }
    const double p = std::fmod (pos, L);
    const float x = a.getX() + (float) (p / L) * a.getWidth();
    g.setColour (Sky::goldDark);
    g.fillRect (x - 1.0f, a.getY() - 2.0f, 2.0f, a.getHeight() + 4.0f);
}

//==============================================================================
void DropZone::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (over ? Sky::goldLight.withAlpha (0.5f) : Sky::white.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 10.0f);
    Path border; border.addRoundedRectangle (r, 10.0f);
    const float dashes[] = { 7.0f, 5.0f };
    Path dashed;
    PathStrokeType (1.5f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (Sky::ink);
    g.fillPath (dashed);
    g.setFont (SkyLookAndFeel::titleFont (20.0f));
    g.drawFittedText (hint, getLocalBounds().reduced (20), Justification::centred, 3);
}

bool DropZone::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (LoopImporter::isSupported (File (f))) return true;
    return false;
}

void DropZone::filesDropped (const StringArray& files, int, int)
{
    over = false; repaint();
    for (auto& f : files) if (LoopImporter::isSupported (File (f))) { if (onFile) onFile (File (f)); break; }
}

void DropZone::mouseUp (const MouseEvent&)
{
    chooser = std::make_unique<FileChooser> (String::fromUTF8 ("Escolha um loop (áudio ou MIDI)"), File(), LoopImporter::wildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile() && onFile) onFile (f);
    });
}

//==============================================================================
void DragOutTile::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Sky::gold);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Sky::ink);
    g.drawRoundedRectangle (r, 7.0f, 1.2f);
    g.setFont (SkyLookAndFeel::uiFont (13.0f, true));
    g.drawFittedText (text, getLocalBounds().reduced (8, 2), Justification::centred, 2);
}

void DragOutTile::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile) return;
    const auto f = makeFile();
    if (! f.existsAsFile()) return;
    dragging = true;
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
}
