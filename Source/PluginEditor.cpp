#include "PluginEditor.h"
#include "StructureBinaryData.h"
#include "LoopImporter.h"
#include "SampleLibrary.h"

using namespace juce;

static String u8 (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
// Página 1 — Cobertura: análise, harmonia, orquestra, faixas
class CoverPage : public Component
{
public:
    CoverPage (StructureEditor& e) : ed (e), s (e.proc.apvts),
        analysis (u8 ("Análise da trilha")), harmony (u8 ("Harmonia")), orchestra (u8 ("Orquestra")), lanesPanel (u8 ("Faixas do arranjo")),
        mode (s, "mode", u8 ("Fonte")), sensitivity (s, "sensitivity", u8 ("Sensibilidade")),
        sevenths (s, "sevenths", u8 ("Detectar sétimas")), lock (s, "lock", u8 ("Travar acorde")), conform (s, "conform", u8 ("Respeitar escala")),
        autoKey (s, "autoKey", u8 ("Tonalidade automática")), keyRoot (s, "keyRoot", u8 ("Tônica (manual)")), scale (s, "scale", u8 ("Escala / modo")),
        extensions (s, "extensions", u8 ("Extensões")), inversion (s, "inversion", u8 ("Inversões")),
        density (s, "density", u8 ("Densidade")), reg (s, "register", u8 ("Registro")), spread (s, "spread", u8 ("Abertura")), humanize (s, "humanize", u8 ("Humanização")),
        dynamics (s, "dynamics", u8 ("Dinâmica")), attack (s, "attack", u8 ("Ataque")), release (s, "release", u8 ("Release")),
        revMix (s, "reverbMix", u8 ("Sala")), revSize (s, "reverbSize", u8 ("Tamanho")), revDecay (s, "reverbDecay", u8 ("Cauda")),
        master (s, "master", u8 ("Volume"))
    {
        for (auto* c : std::initializer_list<Component*> { &analysis, &harmony, &orchestra, &lanesPanel }) addAndMakeVisible (c);
        for (auto* c : std::initializer_list<Component*> { &chord, &wheel, &inMeter, &mode, &sensitivity, &sevenths, &lock, &conform,
                                                           &autoKey, &keyRoot, &scale, &extensions, &inversion, &density, &reg, &spread, &humanize,
                                                           &dynamics, &attack, &release, &revMix, &revSize, &revDecay, &master, &inLabel, &voices })
            addAndMakeVisible (c);
        static const char* lvl[6] = { "lvlVln1", "lvlVln2", "lvlVla", "lvlVc", "lvlCb", "lvlHorns" };
        for (int i = 0; i < 6; ++i) { strips[(size_t) i] = std::make_unique<SectionStrip> (s, lvl[i], i); addAndMakeVisible (*strips[(size_t) i]); }
        for (int i = 0; i < 8; ++i)
        {
            auto& b = laneBtns[(size_t) i];
            b = std::make_unique<TextButton>();
            b->setClickingTogglesState (true);
            laneAtt[(size_t) i] = std::make_unique<APVTS::ButtonAttachment> (s, "lane" + String (i + 1), *b);
            addAndMakeVisible (*b);
        }
        inMeter.vertical = false;
        inLabel.setText (u8 ("Entrada"), dontSendNotification);
        inLabel.setFont (SkyLookAndFeel::uiFont (11.5f, true));
        voices.setFont (SkyLookAndFeel::uiFont (11.0f));
        voices.setColour (Label::textColourId, Sky::inkSoft);
        voices.setJustificationType (Justification::centredRight);
        sensitivity.slider.setTooltip (u8 ("Maior = acompanha trocas de acorde mais rápidas e sinais mais baixos"));
        refreshLanes();
    }

    void refreshLanes()
    {
        const auto& a = ed.proc.getArrangement();
        for (int i = 0; i < 8; ++i)
        {
            auto& b = *laneBtns[(size_t) i];
            if (i < (int) a.lanes.size())
            {
                const auto& l = a.lanes[(size_t) i];
                b.setButtonText (String::fromUTF8 (l.name.c_str()) + "\n" + String::fromUTF8 (sms::articName (l.artic)));
                b.setEnabled (l.enabled);
                b.setVisible (true);
            }
            else b.setVisible (false);
        }
    }

    void update()
    {
        auto& en = ed.proc.engine;
        const int root = en.chordRoot.load();
        chord.set (root, en.chordQuality.load(), en.chordBass.load(), en.chordConfidence.load(), en.keyTonic.load(), en.keyScale.load(),
                   ed.proc.apvts.getRawParameterValue ("lock")->load() > 0.5f);
        std::array<float, 12> c {};
        for (int i = 0; i < 12; ++i) c[(size_t) i] = en.chromaView[(size_t) i].load();
        sms::Chord ch; ch.root = root; ch.quality = (sms::Quality) en.chordQuality.load();
        wheel.set (c, root >= 0 ? ch.mask() : 0, root);
        inMeter.set (en.inputLevel.load());
        for (int i = 0; i < 6; ++i) strips[(size_t) i]->meter.set (en.sectionPeak[(size_t) i].load());
        voices.setText (String (en.voiceCount.load()) + u8 (" vozes"), dontSendNotification);
        const bool manualKey = ed.proc.apvts.getRawParameterValue ("autoKey")->load() < 0.5f;
        keyRoot.setEnabled (manualKey);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto lanesArea = r.removeFromBottom (78);
        r.removeFromBottom (8);
        auto a = r.removeFromLeft (380); r.removeFromLeft (8);
        auto o = r.removeFromRight (410); r.removeFromRight (8);
        analysis.setBounds (a); harmony.setBounds (r); orchestra.setBounds (o); lanesPanel.setBounds (lanesArea);

        auto ac = analysis.content().translated (a.getX(), a.getY());
        chord.setBounds (ac.removeFromTop (104));
        ac.removeFromTop (8);
        auto row = ac.removeFromTop (190);
        wheel.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (10);
        mode.setBounds (row.removeFromTop (42));
        row.removeFromTop (6);
        auto meterRow = row.removeFromTop (18);
        inLabel.setBounds (meterRow.removeFromLeft (52));
        inMeter.setBounds (meterRow.reduced (0, 5));
        row.removeFromTop (6);
        sensitivity.setBounds (row.removeFromTop (100).withSizeKeepingCentre (96, 100));
        ac.removeFromTop (6);
        sevenths.setBounds (ac.removeFromTop (24));
        lock.setBounds (ac.removeFromTop (24));
        conform.setBounds (ac.removeFromTop (24));

        auto hc = harmony.content().translated (r.getX(), r.getY());
        autoKey.setBounds (hc.removeFromTop (24));
        hc.removeFromTop (4);
        auto c1 = hc.removeFromTop (42);
        keyRoot.setBounds (c1.removeFromLeft (c1.getWidth() / 2 - 4)); c1.removeFromLeft (8); scale.setBounds (c1);
        hc.removeFromTop (6);
        auto c2 = hc.removeFromTop (42);
        extensions.setBounds (c2.removeFromLeft (c2.getWidth() / 2 - 4)); c2.removeFromLeft (8); inversion.setBounds (c2);
        hc.removeFromTop (12);
        auto k1 = hc.removeFromTop (hc.getHeight() / 2);
        const int kw = k1.getWidth() / 2;
        density.setBounds (k1.removeFromLeft (kw).reduced (16, 2)); reg.setBounds (k1.reduced (16, 2));
        spread.setBounds (hc.removeFromLeft (kw).reduced (16, 2)); humanize.setBounds (hc.reduced (16, 2));

        auto oc = orchestra.content().translated (o.getX(), o.getY());
        voices.setBounds (o.getRight() - 120, o.getY() + 8, 104, 18);
        auto stripsArea = oc.removeFromTop (226);
        const int sw = stripsArea.getWidth() / 6;
        for (int i = 0; i < 6; ++i) strips[(size_t) i]->setBounds (stripsArea.removeFromLeft (sw).reduced (3, 0));
        oc.removeFromTop (8);
        auto kr1 = oc.removeFromTop (oc.getHeight() / 2);
        const int ow = kr1.getWidth() / 4;
        dynamics.setBounds (kr1.removeFromLeft (ow)); attack.setBounds (kr1.removeFromLeft (ow)); release.setBounds (kr1.removeFromLeft (ow)); master.setBounds (kr1);
        revMix.setBounds (oc.removeFromLeft (ow)); revSize.setBounds (oc.removeFromLeft (ow)); revDecay.setBounds (oc.removeFromLeft (ow));

        auto lc = lanesPanel.content().translated (lanesArea.getX(), lanesArea.getY()).withTrimmedTop (-4);
        const int bw = lc.getWidth() / 8;
        for (int i = 0; i < 8; ++i) laneBtns[(size_t) i]->setBounds (lc.removeFromLeft (bw).reduced (3, 0));
    }

private:
    StructureEditor& ed;
    APVTS& s;
    Panel analysis, harmony, orchestra, lanesPanel;
    ChordDisplay chord;
    ChromaWheel wheel;
    LevelMeter inMeter;
    Label inLabel, voices;
    ParamCombo mode;
    Knob sensitivity;
    ParamToggle sevenths, lock, conform, autoKey;
    ParamCombo keyRoot, scale, extensions, inversion;
    Knob density, reg, spread, humanize, dynamics, attack, release, revMix, revSize, revDecay, master;
    std::array<std::unique_ptr<SectionStrip>, 6> strips;
    std::array<std::unique_ptr<TextButton>, 8> laneBtns;
    std::array<std::unique_ptr<APVTS::ButtonAttachment>, 8> laneAtt;
};

//==============================================================================
// Página 2 — Editor de arranjo (faixas, propriedades, grade, linhas geradas)
class ArrangePage : public Component
{
public:
    ArrangePage (StructureEditor& e) : ed (e), lanesPanel (u8 ("Faixas")), propsPanel (u8 ("Propriedades da faixa")), rollPanel (u8 ("Linhas geradas (ao vivo)"))
    {
        for (auto* c : std::initializer_list<Component*> { &lanesPanel, &propsPanel, &rollPanel, &grid, &roll, &addBtn, &dupBtn, &delBtn, &upBtn,
                                                           &nameEd, &typeBox, &articBox, &octave, &gain, &gate, &spbBox, &stepsBox, &enabled,
                                                           &onlyThis, &clearBtn, &fillBtn, &savePresetBtn, &nameL, &typeL, &articL, &octL, &gainL, &gateL, &spbL, &stepsL, &secL })
            addAndMakeVisible (c);
        for (int i = 0; i < 8; ++i)
        {
            laneSel[(size_t) i].setClickingTogglesState (false);
            laneSel[(size_t) i].onClick = [this, i] { select (i); };
            addAndMakeVisible (laneSel[(size_t) i]);
        }
        for (int s = 0; s < 6; ++s)
        {
            secBtns[(size_t) s].setButtonText (String::fromUTF8 (sms::sectionName (s)));
            secBtns[(size_t) s].onClick = [this, s] { if (auto* l = lane()) { l->sectionMask ^= (uint8_t) (1u << s); if (! l->sectionMask) l->sectionMask = (uint8_t) (1u << s); commit(); } };
            addAndMakeVisible (secBtns[(size_t) s]);
        }
        nameEd.setColour (TextEditor::textColourId, Sky::ink);
        nameEd.setColour (TextEditor::backgroundColourId, Sky::white.withAlpha (0.92f));
        nameEd.setColour (TextEditor::outlineColourId, Sky::ink);
        addBtn.setButtonText (u8 ("+ Nova"));    dupBtn.setButtonText (u8 ("Duplicar"));
        delBtn.setButtonText (u8 ("Excluir"));   upBtn.setButtonText (u8 ("Subir"));
        clearBtn.setButtonText (u8 ("Limpar grade")); fillBtn.setButtonText (u8 ("Arpejo automático"));
        savePresetBtn.setButtonText (u8 ("Salvar como preset…"));
        enabled.setButtonText (u8 ("Faixa ativa"));
        onlyThis.setButtonText (u8 ("Mostrar só esta faixa"));
        typeBox.addItemList ({ "Pad", "Linha (legato)", u8 ("Padrão (ritmo)"), "Tutti" }, 1);
        for (int a = 0; a < sms::numArtics; ++a) articBox.addItem (String::fromUTF8 (sms::articName (a)), a + 1);
        spbBox.addItemList ({ u8 ("Colcheias (2/tempo)"), u8 ("Tercinas (3/tempo)"), u8 ("Semicolcheias (4/tempo)") }, 1);
        for (int n : { 4, 6, 8, 12, 16, 24, 32 }) stepsBox.addItem (String (n) + u8 (" passos"), n);
        auto setupSlider = [] (Slider& sl, double lo, double hi, double step)
        {
            sl.setSliderStyle (Slider::LinearHorizontal);
            sl.setTextBoxStyle (Slider::TextBoxRight, false, 48, 18);
            sl.setRange (lo, hi, step);
        };
        setupSlider (octave, -2, 2, 1); setupSlider (gain, 0, 1.5, 0.01); setupSlider (gate, 0.05, 1.0, 0.01);
        for (auto* l : { &nameL, &typeL, &articL, &octL, &gainL, &gateL, &spbL, &stepsL, &secL })
        { l->setFont (SkyLookAndFeel::uiFont (12.0f, true)); l->setColour (Label::textColourId, Sky::ink); }
        nameL.setText (u8 ("Nome"), dontSendNotification); typeL.setText (u8 ("Tipo"), dontSendNotification);
        articL.setText (u8 ("Articulação"), dontSendNotification); octL.setText (u8 ("Oitava"), dontSendNotification);
        gainL.setText (u8 ("Volume"), dontSendNotification); gateL.setText (u8 ("Duração"), dontSendNotification);
        spbL.setText (u8 ("Grade"), dontSendNotification); stepsL.setText (u8 ("Tamanho"), dontSendNotification);
        secL.setText (u8 ("Seções"), dontSendNotification);

        addBtn.onClick = [this]
        {
            if ((int) arr.lanes.size() >= sms::kMaxLanes) return;
            sms::Lane l; l.name = "Nova faixa"; l.type = sms::LaneType::Pattern; l.artic = sms::Spiccato;
            l.sectionMask = (1 << sms::Vln1) | (1 << sms::Vln2); l.clearGrid();
            for (int s = 0; s < 16; s += 2) l.grid[2][(size_t) s] = 90;
            arr.lanes.push_back (l); sel = (int) arr.lanes.size() - 1; commit();
        };
        dupBtn.onClick = [this] { if (lane() && (int) arr.lanes.size() < sms::kMaxLanes) { auto c = *lane(); c.name += u8 (" (cópia)").toStdString(); arr.lanes.push_back (c); sel = (int) arr.lanes.size() - 1; commit(); } };
        delBtn.onClick = [this] { if (lane() && arr.lanes.size() > 1) { arr.lanes.erase (arr.lanes.begin() + sel); sel = jmax (0, sel - 1); commit(); } };
        upBtn.onClick = [this] { if (sel > 0 && sel < (int) arr.lanes.size()) { std::swap (arr.lanes[(size_t) sel], arr.lanes[(size_t) sel - 1]); --sel; commit(); } };
        nameEd.onTextChange = [this] { if (auto* l = lane()) { l->name = nameEd.getText().toStdString(); commit (false); } };
        typeBox.onChange = [this] { if (auto* l = lane()) { l->type = (sms::LaneType) (typeBox.getSelectedId() - 1); if (l->type == sms::LaneType::Line) l->artic = sms::Legato; commit(); } };
        articBox.onChange = [this] { if (auto* l = lane()) { l->artic = articBox.getSelectedId() - 1; commit(); } };
        octave.onValueChange = [this] { if (auto* l = lane()) { l->octave = (int) octave.getValue(); commit (false); } };
        gain.onValueChange = [this] { if (auto* l = lane()) { l->gain = (float) gain.getValue(); commit (false); } };
        gate.onValueChange = [this] { if (auto* l = lane()) { l->gate = (float) gate.getValue(); commit (false); grid.repaint(); } };
        spbBox.onChange = [this] { if (auto* l = lane()) { l->stepsPerBeat = spbBox.getSelectedId() + 1; commit(); } };
        stepsBox.onChange = [this] { if (auto* l = lane()) { l->numSteps = stepsBox.getSelectedId(); commit(); } };
        enabled.onClick = [this] { if (auto* l = lane()) { l->enabled = enabled.getToggleState(); commit(); } };
        onlyThis.onClick = [this] { roll.laneFilter = onlyThis.getToggleState() ? sel : -1; };
        clearBtn.onClick = [this] { if (auto* l = lane()) { l->clearGrid(); commit(); } };
        fillBtn.onClick = [this]
        {
            if (auto* l = lane())
            {
                l->type = sms::LaneType::Pattern; l->clearGrid();
                static const int pat[8] = { 1, 2, 3, 4, 3, 2, 1, 2 };
                for (int s = 0; s < l->numSteps; ++s) l->grid[(size_t) pat[s % 8]][(size_t) s] = (uint8_t) (s % l->stepsPerBeat == 0 ? 110 : 80);
                commit();
            }
        };
        savePresetBtn.onClick = [this] { if (onSavePreset) onSavePreset(); };
        grid.onChange = [this] { commit (false); };
        reload();
    }

    std::function<void()> onSavePreset;

    void reload()
    {
        arr = ed.proc.getArrangement();
        if (arr.lanes.empty()) { sms::Lane l; l.name = "Pad"; arr.lanes.push_back (l); }
        sel = jlimit (0, (int) arr.lanes.size() - 1, sel);
        refreshUi();
    }

    void pushDisplay (const sms::DisplayEvent& e) { roll.push (e); }
    void tick (double ppq)
    {
        roll.setNow (ppq);
        roll.repaint();
        if (auto* l = lane()) grid.setPlayStep (l->type == sms::LaneType::Pattern ? (int) std::fmod (std::floor (ppq * l->stepsPerBeat), (double) l->numSteps) : -1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto bottom = r.removeFromBottom (190);
        r.removeFromBottom (8);
        auto left = r.removeFromLeft (220); r.removeFromLeft (8);
        lanesPanel.setBounds (left);
        auto lc = lanesPanel.content().translated (left.getX(), left.getY());
        for (int i = 0; i < 8; ++i) laneSel[(size_t) i].setBounds (lc.removeFromTop (25).reduced (0, 1));
        lc.removeFromTop (4);
        auto b1 = lc.removeFromTop (26); addBtn.setBounds (b1.removeFromLeft (b1.getWidth() / 2 - 2)); b1.removeFromLeft (4); dupBtn.setBounds (b1);
        lc.removeFromTop (3);
        auto b2 = lc.removeFromTop (26); upBtn.setBounds (b2.removeFromLeft (b2.getWidth() / 2 - 2)); b2.removeFromLeft (4); delBtn.setBounds (b2);

        auto props = r.removeFromTop (150);
        r.removeFromTop (8);
        propsPanel.setBounds (props);
        auto pc = propsPanel.content().translated (props.getX(), props.getY());
        auto col = [&] (Rectangle<int>& area, int w) { auto c = area.removeFromLeft (w); area.removeFromLeft (10); return c; };
        auto row1 = pc.removeFromTop (40);
        auto c = col (row1, 220); nameL.setBounds (c.removeFromTop (16)); nameEd.setBounds (c);
        c = col (row1, 150); typeL.setBounds (c.removeFromTop (16)); typeBox.setBounds (c);
        c = col (row1, 130); articL.setBounds (c.removeFromTop (16)); articBox.setBounds (c);
        c = col (row1, 180); spbL.setBounds (c.removeFromTop (16)); spbBox.setBounds (c);
        c = col (row1, 110); stepsL.setBounds (c.removeFromTop (16)); stepsBox.setBounds (c);
        pc.removeFromTop (6);
        auto row2 = pc.removeFromTop (24);
        secL.setBounds (row2.removeFromLeft (60));
        for (int s = 0; s < 6; ++s) secBtns[(size_t) s].setBounds (row2.removeFromLeft (118).reduced (2, 0));
        pc.removeFromTop (8);
        auto row3 = pc.removeFromTop (24);
        octL.setBounds (row3.removeFromLeft (52)); octave.setBounds (row3.removeFromLeft (150)); row3.removeFromLeft (10);
        gainL.setBounds (row3.removeFromLeft (56)); gain.setBounds (row3.removeFromLeft (150)); row3.removeFromLeft (10);
        gateL.setBounds (row3.removeFromLeft (64)); gate.setBounds (row3.removeFromLeft (150)); row3.removeFromLeft (10);
        enabled.setBounds (row3.removeFromLeft (120));

        auto tools = r.removeFromBottom (30);
        r.removeFromBottom (6);
        grid.setBounds (r);
        clearBtn.setBounds (tools.removeFromLeft (140).reduced (0, 1)); tools.removeFromLeft (8);
        fillBtn.setBounds (tools.removeFromLeft (170).reduced (0, 1));
        savePresetBtn.setBounds (tools.removeFromRight (200).reduced (0, 1));

        rollPanel.setBounds (bottom);
        auto rc = rollPanel.content().translated (bottom.getX(), bottom.getY());
        onlyThis.setBounds (bottom.getRight() - 200, bottom.getY() + 6, 186, 20);
        roll.setBounds (rc);
    }

private:
    sms::Lane* lane() { return isPositiveAndBelow (sel, (int) arr.lanes.size()) ? &arr.lanes[(size_t) sel] : nullptr; }
    void select (int i) { if (i < (int) arr.lanes.size()) { sel = i; refreshUi(); if (onlyThis.getToggleState()) roll.laneFilter = sel; } }

    void commit (bool refresh = true)
    {
        ed.proc.setArrangement (arr);
        ed.arrangementChanged();
        if (refresh) refreshUi();
    }

    void refreshUi()
    {
        for (int i = 0; i < 8; ++i)
        {
            auto& b = laneSel[(size_t) i];
            if (i < (int) arr.lanes.size())
            {
                const auto& l = arr.lanes[(size_t) i];
                b.setVisible (true);
                b.setButtonText (String (i + 1) + ". " + String::fromUTF8 (l.name.c_str()));
                b.setToggleState (i == sel, dontSendNotification);
                b.setAlpha (l.enabled ? 1.0f : 0.55f);
            }
            else b.setVisible (false);
        }
        auto* l = lane();
        grid.setLane (l);
        if (! l) return;
        nameEd.setText (String::fromUTF8 (l->name.c_str()), dontSendNotification);
        nameEd.applyColourToAllText (Sky::ink);
        typeBox.setSelectedId ((int) l->type + 1, dontSendNotification);
        articBox.setSelectedId (l->artic + 1, dontSendNotification);
        octave.setValue (l->octave, dontSendNotification);
        gain.setValue (l->gain, dontSendNotification);
        gate.setValue (l->gate, dontSendNotification);
        spbBox.setSelectedId (jlimit (1, 3, l->stepsPerBeat - 1), dontSendNotification);
        stepsBox.setSelectedId (l->numSteps, dontSendNotification);
        enabled.setToggleState (l->enabled, dontSendNotification);
        for (int s = 0; s < 6; ++s) secBtns[(size_t) s].setToggleState ((l->sectionMask >> s) & 1, dontSendNotification);
        const bool pattern = l->type == sms::LaneType::Pattern;
        spbBox.setEnabled (pattern); stepsBox.setEnabled (pattern); gate.setEnabled (pattern); clearBtn.setEnabled (pattern);
        addBtn.setEnabled ((int) arr.lanes.size() < sms::kMaxLanes);
        dupBtn.setEnabled ((int) arr.lanes.size() < sms::kMaxLanes);
        delBtn.setEnabled (arr.lanes.size() > 1);
    }

    StructureEditor& ed;
    sms::Arrangement arr;
    int sel = 0;
    Panel lanesPanel, propsPanel, rollPanel;
    std::array<TextButton, 8> laneSel;
    std::array<TextButton, 6> secBtns;
    TextButton addBtn, dupBtn, delBtn, upBtn, clearBtn, fillBtn, savePresetBtn;
    TextEditor nameEd;
    ComboBox typeBox, articBox, spbBox, stepsBox;
    Slider octave, gain, gate;
    ToggleButton enabled, onlyThis;
    Label nameL, typeL, articL, octL, gainL, gateL, spbL, stepsL, secL;
    LaneGrid grid;
    GeneratedRoll roll;
};

//==============================================================================
// Página 3 — Importar loops
class ImportPage : public Component
{
public:
    ImportPage (StructureEditor& e) : ed (e), dropPanel (u8 ("Importar loop externo")), progPanel (u8 ("Harmonia e ritmo detectados"))
    {
        for (auto* c : std::initializer_list<Component*> { &dropPanel, &progPanel, &drop, &info, &view, &useBtn, &ostinatoBtn, &dragTile, &saveMidiBtn, &clearBtn, &repeats, &repL })
            addAndMakeVisible (c);
        drop.hint = u8 ("Arraste aqui um loop WAV, AIFF, FLAC, MP3 ou MIDI\n— ou clique para escolher —");
        drop.onFile = [this] (const File& f) { importFile (f); };
        info.setFont (SkyLookAndFeel::uiFont (13.5f));
        info.setJustificationType (Justification::topLeft);
        info.setColour (Label::textColourId, Sky::ink);
        info.setText (u8 ("O SeniorMy Structure detecta o andamento, os acordes, a tonalidade e o ritmo do loop. "
                          "Depois você pode tocar a cobertura sobre essa progressão (sincronizada com a DAW), "
                          "criar um ostinato com o ritmo do loop e exportar as linhas geradas em MIDI."), dontSendNotification);
        useBtn.setButtonText (u8 ("Tocar cobertura sobre a progressão"));
        ostinatoBtn.setButtonText (u8 ("Criar ostinato com o ritmo do loop"));
        saveMidiBtn.setButtonText (u8 ("Salvar MIDI da cobertura…"));
        clearBtn.setButtonText (u8 ("Limpar"));
        dragTile.text = u8 ("Arraste o MIDI da cobertura para a DAW");
        repL.setText (u8 ("Repetições"), dontSendNotification);
        repL.setFont (SkyLookAndFeel::uiFont (12.0f, true));
        for (int n : { 1, 2, 4, 8 }) repeats.addItem (String (n) + "x", n);
        repeats.setSelectedId (2, dontSendNotification);

        useBtn.onClick = [this]
        {
            if (auto* p = ed.proc.apvts.getParameter ("mode")) p->setValueNotifyingHost (p->convertTo0to1 (3.0f));
        };
        ostinatoBtn.onClick = [this]
        {
            auto prog = ed.proc.getProgression();
            if (! prog) return;
            auto a = ed.proc.getArrangement();
            if ((int) a.lanes.size() >= sms::kMaxLanes) a.lanes.pop_back();
            a.lanes.push_back (sms::laneFromRhythm (*prog, sms::Spiccato, (1 << sms::Vln1) | (1 << sms::Vln2) | (1 << sms::Vla), "Ostinato do loop"));
            ed.proc.setArrangement (a);
            ed.arrangementChanged();
            info.setText (u8 ("Faixa \"Ostinato do loop\" criada a partir do ritmo do arquivo. Ajuste-a no Editor de arranjo."), dontSendNotification);
        };
        dragTile.makeFile = [this] { String err; auto f = ed.proc.exportCoverMidi (repeats.getSelectedId(), err); if (! f.existsAsFile()) info.setText (err, dontSendNotification); return f; };
        saveMidiBtn.onClick = [this]
        {
            String err;
            auto tmp = ed.proc.exportCoverMidi (repeats.getSelectedId(), err);
            if (! tmp.existsAsFile()) { info.setText (err, dontSendNotification); return; }
            chooser = std::make_unique<FileChooser> (u8 ("Salvar MIDI da cobertura"), File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (tmp.getFileName()), "*.mid");
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this, tmp] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == File()) return;
                if (tmp.copyFileTo (f.withFileExtension ("mid"))) info.setText (u8 ("MIDI salvo em ") + f.getFullPathName(), dontSendNotification);
            });
        };
        clearBtn.onClick = [this] { ed.proc.clearProgression(); view.set (nullptr); refreshButtons(); };
        view.set (ed.proc.getProgression());
        refreshButtons();
    }

    void importFile (const File& f)
    {
        info.setText (u8 ("Analisando ") + f.getFileName() + u8 ("…"), dontSendNotification);
        SafePointer<ImportPage> safe (this);
        ed.proc.importLoop (f, [safe] (bool ok, String msg)
        {
            if (safe == nullptr) return;
            safe->info.setText (ok ? msg + u8 ("\nClique em \"Tocar cobertura sobre a progressão\" para ouvir.") : msg, dontSendNotification);
            safe->view.set (safe->ed.proc.getProgression());
            safe->refreshButtons();
        });
    }

    void tick (double ppq) { view.setPosition (ppq); }

    void refreshButtons()
    {
        const bool has = ed.proc.getProgression() != nullptr;
        for (auto* b : std::initializer_list<Component*> { &useBtn, &ostinatoBtn, &saveMidiBtn, &clearBtn, &dragTile, &repeats }) b->setEnabled (has);
        dragTile.setAlpha (has ? 1.0f : 0.4f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto left = r.removeFromLeft (440); r.removeFromLeft (8);
        dropPanel.setBounds (left);
        auto lc = dropPanel.content().translated (left.getX(), left.getY());
        drop.setBounds (lc.removeFromTop (190));
        lc.removeFromTop (10);
        info.setBounds (lc);
        progPanel.setBounds (r);
        auto pc = progPanel.content().translated (r.getX(), r.getY());
        view.setBounds (pc.removeFromTop (200));
        pc.removeFromTop (14);
        auto row = pc.removeFromTop (34);
        useBtn.setBounds (row.removeFromLeft (300)); row.removeFromLeft (10);
        ostinatoBtn.setBounds (row.removeFromLeft (300));
        pc.removeFromTop (14);
        auto row2 = pc.removeFromTop (60);
        dragTile.setBounds (row2.removeFromLeft (250)); row2.removeFromLeft (10);
        auto rc = row2.removeFromLeft (90); repL.setBounds (rc.removeFromTop (18)); repeats.setBounds (rc.removeFromTop (28));
        row2.removeFromLeft (10);
        saveMidiBtn.setBounds (row2.removeFromLeft (220).withSizeKeepingCentre (220, 34));
        row2.removeFromLeft (10);
        clearBtn.setBounds (row2.removeFromLeft (90).withSizeKeepingCentre (90, 34));
    }

private:
    StructureEditor& ed;
    Panel dropPanel, progPanel;
    DropZone drop;
    Label info, repL;
    ProgressionView view;
    TextButton useBtn, ostinatoBtn, saveMidiBtn, clearBtn;
    DragOutTile dragTile;
    ComboBox repeats;
    std::unique_ptr<FileChooser> chooser;
};

//==============================================================================
// Página 4 — Sobre: ícone, versão, direitos, créditos e licenças
class AboutPage : public Component
{
public:
    AboutPage (StructureEditor& e) : ed (e), panel (u8 ("Sobre o SeniorMy Structure"))
    {
        addAndMakeVisible (panel);
        logo = ImageCache::getFromMemory (StructureBinaryData::logo256_png, StructureBinaryData::logo256_pngSize);
        credits.setMultiLine (true);
        credits.setReadOnly (true);
        credits.setScrollbarsShown (true);
        credits.setFont (SkyLookAndFeel::uiFont (12.5f));
        credits.setColour (TextEditor::textColourId, Sky::ink);
        credits.setColour (TextEditor::backgroundColourId, Sky::white.withAlpha (0.92f));
        credits.setColour (TextEditor::outlineColourId, Sky::ink);
        credits.setText (String::fromUTF8 (StructureBinaryData::THIRDPARTYNOTICES_txt, StructureBinaryData::THIRDPARTYNOTICES_txtSize));
        credits.applyColourToAllText (Sky::ink);
        addAndMakeVisible (credits);
        termsBtn.setButtonText (u8 ("Ler os Termos de Uso"));
        termsBtn.onClick = [this] { ed.showTermsReadOnly(); };
        addAndMakeVisible (termsBtn);
        folderBtn.setButtonText (u8 ("Abrir pasta de presets"));
        folderBtn.onClick = [] { auto d = PresetManager::userFolder(); d.createDirectory(); d.startAsProcess(); };
        addAndMakeVisible (folderBtn);
    }

    void paintOverChildren (Graphics& g) override
    {
        auto r = getLocalBounds().reduced (8);
        auto c = panel.content().translated (r.getX(), r.getY());
        auto left = c.removeFromLeft (300);
        g.drawImage (logo, left.removeFromTop (240).withSizeKeepingCentre (220, 220).toFloat(), RectanglePlacement::centred);
        g.setColour (Sky::ink);
        g.setFont (SkyLookAndFeel::titleFont (26.0f));
        g.drawText ("SeniorMy Structure", left.removeFromTop (34), Justification::centred);
        g.setFont (SkyLookAndFeel::uiFont (14.0f));
        g.setColour (Sky::inkSoft);
        g.drawText (u8 ("Versão ") + JucePlugin_VersionString, left.removeFromTop (20), Justification::centred);
        g.drawFittedText (u8 ("Coberturas orquestrais automáticas\ndentro da harmonia da sua música"), left.removeFromTop (40), Justification::centred, 2);
        left.removeFromTop (8);
        g.setColour (Sky::ink);
        g.setFont (SkyLookAndFeel::uiFont (13.0f, true));
        g.drawFittedText (u8 ("© 2026 W. Carolino — SeniorMy\nCNPJ 69.610.819/0001-77\nTodos os direitos reservados."), left.removeFromTop (54), Justification::centred, 3);
        g.setFont (SkyLookAndFeel::uiFont (12.0f));
        g.setColour (Sky::inkSoft);
        g.drawFittedText (ed.proc.getLibraryStatus(), left.removeFromTop (48).reduced (8, 0), Justification::centred, 3);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        panel.setBounds (r);
        auto c = panel.content().translated (r.getX(), r.getY());
        auto left = c.removeFromLeft (300);
        c.removeFromLeft (16);
        auto bottom = c.removeFromBottom (36);
        credits.setBounds (c.withTrimmedBottom (8));
        termsBtn.setBounds (bottom.removeFromLeft (220));
        bottom.removeFromLeft (10);
        folderBtn.setBounds (bottom.removeFromLeft (220));
        (void) left;
    }

private:
    StructureEditor& ed;
    Panel panel;
    Image logo;
    TextEditor credits;
    TextButton termsBtn, folderBtn;
};

//==============================================================================
StructureEditor::StructureEditor (StructureProcessor& p)
    : AudioProcessorEditor (p), proc (p), keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);
    logo = ImageCache::getFromMemory (StructureBinaryData::logo256_png, StructureBinaryData::logo256_pngSize);
    addAndMakeVisible (root);

    for (auto* c : std::initializer_list<Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &saveAsBtn, &deleteBtn, &panicBtn, &statusLabel, &tabs, &keyboard, &outMeter })
        root.addAndMakeVisible (c);
    saveBtn.setButtonText (u8 ("Salvar"));
    saveAsBtn.setButtonText (u8 ("Salvar como…"));
    deleteBtn.setButtonText (u8 ("Excluir"));
    panicBtn.setButtonText (u8 ("Pânico"));
    statusLabel.setFont (SkyLookAndFeel::uiFont (11.5f));
    statusLabel.setColour (Label::textColourId, Sky::inkSoft);
    statusLabel.setJustificationType (Justification::centredRight);

    presetBox.onChange = [this]
    {
        if (presetBoxUpdating) return;
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) { proc.loadPresetEntry (idx); arrangementChanged(); }
    };
    prevBtn.onClick = [this] { const int n = proc.presets.entries().size(); if (n > 0) { proc.loadPresetEntry ((proc.getCurrentProgram() + n - 1) % n); arrangementChanged(); } };
    nextBtn.onClick = [this] { const int n = proc.presets.entries().size(); if (n > 0) { proc.loadPresetEntry ((proc.getCurrentProgram() + 1) % n); arrangementChanged(); } };
    saveBtn.onClick = [this]
    {
        const auto& list = proc.presets.entries();
        const int i = proc.getCurrentProgram();
        if (isPositiveAndBelow (i, list.size()) && ! list[i].factory)
        {
            String err;
            if (! proc.saveUserPreset (list[i].name, err)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SeniorMy Structure", err);
            refreshPresetBox();
        }
        else promptSaveAs();
    };
    saveAsBtn.onClick = [this] { promptSaveAs(); };
    deleteBtn.onClick = [this]
    {
        const auto& list = proc.presets.entries();
        const int i = proc.getCurrentProgram();
        if (! isPositiveAndBelow (i, list.size()) || list[i].factory) return;
        const auto entry = list[i];
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, u8 ("Excluir preset"), u8 ("Excluir \"") + entry.name + "\"?",
                                      u8 ("Excluir"), u8 ("Cancelar"), this,
                                      ModalCallbackFunction::create ([this, entry] (int r) { if (r == 1) { proc.presets.remove (entry); proc.loadPresetEntry (0); arrangementChanged(); } }));
    };
    panicBtn.onClick = [this] { proc.engine.requestPanic(); proc.keyboardState.allNotesOff (0); };

    cover = std::make_unique<CoverPage> (*this);
    arrange = std::make_unique<ArrangePage> (*this);
    importPage = std::make_unique<ImportPage> (*this);
    about = std::make_unique<AboutPage> (*this);
    arrange->onSavePreset = [this] { promptSaveAs(); };
    const auto tabBg = Colours::transparentBlack;
    tabs.addTab (u8 ("Cobertura"), tabBg, cover.get(), false);
    tabs.addTab (u8 ("Editor de arranjo"), tabBg, arrange.get(), false);
    tabs.addTab (u8 ("Importar loops"), tabBg, importPage.get(), false);
    tabs.addTab (u8 ("Sobre"), tabBg, about.get(), false);
    tabs.setTabBarDepth (40);
    tabs.setOutline (0);

    pianoLevel = std::make_unique<Knob> (proc.apvts, "pianoLevel", u8 ("Piano"));
    bpm = std::make_unique<Knob> (proc.apvts, "bpm", u8 ("BPM interno"));
    pianoInput = std::make_unique<ParamToggle> (proc.apvts, "pianoInput", u8 ("Piano toca o MIDI"));
    pianoCover = std::make_unique<ParamToggle> (proc.apvts, "pianoCover", u8 ("Piano toca a cobertura"));
    coverOn = std::make_unique<ParamToggle> (proc.apvts, "coverOn", u8 ("Cordas ligadas"));
    for (auto* c : std::initializer_list<Component*> { pianoLevel.get(), bpm.get(), pianoInput.get(), pianoCover.get(), coverOn.get() }) root.addAndMakeVisible (c);
    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (17.0f);
    keyboard.setScrollButtonsVisible (true);

    if (! proc.termsAccepted())
    {
        terms = std::make_unique<TermsOverlay> ([this]
        {
            proc.markTermsAccepted();
            if (terms) { root.removeChildComponent (terms.get()); terms.reset(); }
        });
        root.addAndMakeVisible (*terms);
    }

    refreshPresetBox();
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    setSize (roundToInt (baseW * jlimit (0.6f, 2.0f, proc.uiScale)), roundToInt (baseH * jlimit (0.6f, 2.0f, proc.uiScale)));

    const auto snap = SystemStats::getEnvironmentVariable ("SENIORMY_SNAPSHOT_DIR", {});
    if (snap.isNotEmpty()) { snapshotDir = File (snap); snapshotDir.createDirectory(); snapshotCountdown = 0; }
    startTimerHz (30);
}

StructureEditor::~StructureEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StructureEditor::paint (Graphics& g) { g.fillAll (Sky::zenith); }

void StructureEditor::resized()
{
    const float sc = (float) getWidth() / (float) baseW;
    proc.uiScale = sc;
    root.setBounds (0, 0, baseW, baseH);
    root.setTransform (AffineTransform::scale (sc));
}

void StructureEditor::Root::paint (Graphics& g)
{
    SkyLookAndFeel::paintSky (g, getLocalBounds().toFloat(), t);
    // cabeçalho
    auto head = Rectangle<float> (10, 8, (float) getWidth() - 20, 58);
    SkyLookAndFeel::paintPanel (g, head, false, 12.0f);
    g.drawImage (ed.logo, Rectangle<float> (20, 13, 48, 48), RectanglePlacement::centred);
    g.setColour (Sky::ink);
    g.setFont (SkyLookAndFeel::titleFont (26.0f));
    g.drawText ("SeniorMy", Rectangle<float> (78, 12, 130, 30), Justification::centredLeft);
    g.setColour (Sky::goldDark);
    g.drawText ("Structure", Rectangle<float> (194, 12, 140, 30), Justification::centredLeft);
    g.setColour (Sky::inkSoft);
    g.setFont (SkyLookAndFeel::uiFont (11.5f));
    g.drawText (String::fromUTF8 ("Coberturas orquestrais na harmonia da sua música"), Rectangle<float> (80, 40, 330, 18), Justification::centredLeft);
    // rodapé
    auto foot = Rectangle<float> (10, (float) getHeight() - 142, (float) getWidth() - 20, 134);
    SkyLookAndFeel::paintPanel (g, foot, false, 12.0f);
    g.setColour (Sky::inkSoft);
    g.setFont (SkyLookAndFeel::uiFont (10.5f));
    g.drawText (String::fromUTF8 ("© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77"), Rectangle<float> (foot.getRight() - 330, foot.getBottom() - 18, 316, 14), Justification::centredRight);
}

void StructureEditor::Root::resized()
{
    auto& e = ed;
    auto head = getLocalBounds().reduced (10, 8).removeFromTop (58).reduced (14, 13);
    head.removeFromLeft (350);
    e.panicBtn.setBounds (head.removeFromRight (76));
    head.removeFromRight (6);
    e.statusLabel.setBounds (head.removeFromRight (170));
    head.removeFromRight (8);
    e.prevBtn.setBounds (head.removeFromLeft (30)); head.removeFromLeft (4);
    e.presetBox.setBounds (head.removeFromLeft (190)); head.removeFromLeft (4);
    e.nextBtn.setBounds (head.removeFromLeft (30)); head.removeFromLeft (8);
    e.saveBtn.setBounds (head.removeFromLeft (72)); head.removeFromLeft (4);
    e.saveAsBtn.setBounds (head.removeFromLeft (112)); head.removeFromLeft (4);
    e.deleteBtn.setBounds (head.removeFromLeft (72));

    auto foot = Rectangle<int> (10, getHeight() - 142, getWidth() - 20, 134).reduced (12, 10);
    auto ctl = foot.removeFromLeft (330);
    auto knobs = ctl.removeFromLeft (150);
    e.pianoLevel->setBounds (knobs.removeFromLeft (75).withTrimmedBottom (20));
    e.bpm->setBounds (knobs.withTrimmedBottom (20));
    e.pianoInput->setBounds (ctl.removeFromTop (26));
    e.pianoCover->setBounds (ctl.removeFromTop (26));
    e.coverOn->setBounds (ctl.removeFromTop (26));
    e.outMeter.setBounds (foot.removeFromRight (10).withTrimmedBottom (18));
    foot.removeFromRight (6);
    e.keyboard.setBounds (foot.withTrimmedBottom (16));

    e.tabs.setBounds (10, 72, getWidth() - 20, getHeight() - 72 - 150);
    if (e.terms) e.terms->setBounds (getLocalBounds());
}

void StructureEditor::refreshPresetBox()
{
    presetBoxUpdating = true;
    presetBox.clear (dontSendNotification);
    const auto& list = proc.presets.entries();
    String lastCat;
    for (int i = 0; i < list.size(); ++i)
    {
        if (list[i].category != lastCat) { presetBox.addSectionHeading (list[i].factory ? u8 ("Fábrica — ") + list[i].category : u8 ("Meus presets")); lastCat = list[i].category; }
        presetBox.addItem (list[i].name, i + 1);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.setText (proc.getPresetName(), dontSendNotification);
    const bool user = isPositiveAndBelow (proc.getCurrentProgram(), list.size()) && ! list[proc.getCurrentProgram()].factory;
    deleteBtn.setEnabled (user);
    presetBoxUpdating = false;
}

void StructureEditor::arrangementChanged()
{
    if (cover) cover->refreshLanes();
    if (arrange) arrange->reload();
    if (importPage) importPage->refreshButtons();
    refreshPresetBox();
}

void StructureEditor::promptSaveAs()
{
    saveDialog = std::make_unique<AlertWindow> (u8 ("Salvar preset"), u8 ("Nome do preset (salvar com um nome existente substitui o seu preset):"), MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", proc.getPresetName().upToFirstOccurrenceOf (" *", false, false), u8 ("Nome"));
    saveDialog->addButton (u8 ("Salvar"), 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton (u8 ("Cancelar"), 0, KeyPress (KeyPress::escapeKey));
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveDialog)
        {
            const auto name = saveDialog->getTextEditorContents ("name").trim();
            String err;
            if (! proc.saveUserPreset (name, err)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SeniorMy Structure", err);
            refreshPresetBox();
        }
        saveDialog.reset();
    }), false);
}

void StructureEditor::showTermsReadOnly()
{
    if (terms) return;
    SafePointer<StructureEditor> safeThis (this);
    terms = std::make_unique<TermsOverlay> ([safeThis]
    {
        MessageManager::callAsync ([sp = safeThis] { if (sp != nullptr && sp->terms && sp->proc.termsAccepted()) { sp->root.removeChildComponent (sp->terms.get()); sp->terms.reset(); } });
    }, true);
    root.addAndMakeVisible (*terms);
    terms->setBounds (root.getLocalBounds());
}

void StructureEditor::timerCallback()
{
    root.t += 1.0f / 30.0f;
    if (cover && tabs.getCurrentTabIndex() == 0) cover->update();
    sms::DisplayEvent e;
    int n = 0;
    while (proc.engine.displayQueue.pop (e) && n++ < 2000) if (arrange) arrange->pushDisplay (e);
    const double ppq = proc.engine.displayPpq.load();
    if (arrange && tabs.getCurrentTabIndex() == 1) arrange->tick (ppq);
    if (importPage && tabs.getCurrentTabIndex() == 2)
    {
        auto prog = proc.getProgression();
        importPage->tick (ppq);
    }
    outMeter.set (proc.engine.outputPeak.load());
    auto status = proc.getLibraryStatus();
    if (proc.isLibraryLoading()) status = String::fromUTF8 ("Carregando timbres… ") + String (roundToInt (proc.getLibraryProgress() * 100)) + "%";
    else if (proc.isUsingFallbackBank()) status = String::fromUTF8 ("Timbres de reserva (biblioteca não instalada)");
    else status = status.upToFirstOccurrenceOf (" —", false, false);
    if (statusLabel.getText() != status) statusLabel.setText (status, dontSendNotification);
    if (((int) (root.t * 30.0f)) % 15 == 0) root.repaint (0, 0, baseW, 70);   // estrelas cintilando no topo
    if (snapshotCountdown >= 0) takeSnapshots();
}

void StructureEditor::takeSnapshots()
{
    // modo de verificação no CI: SENIORMY_SNAPSHOT_DIR=/pasta → grava capturas de cada tela
    ++snapshotCountdown;
    auto save = [this] (const String& name)
    {
        auto img = root.createComponentSnapshot (root.getLocalBounds(), true, 1.0f);
        PNGImageFormat png;
        auto f = snapshotDir.getChildFile (name);
        f.deleteFile();
        FileOutputStream out (f);
        if (out.openedOk()) png.writeImageToStream (img, out);
    };
    const int c = snapshotCountdown;
    if (c == 75) save ("01-termos.png");
    if (c == 80 && terms) terms->setVisible (false);
    if (c == 85) { tabs.setCurrentTabIndex (0); }
    if (c == 100) save ("02-cobertura.png");
    if (c == 105) tabs.setCurrentTabIndex (1);
    if (c == 120) save ("03-editor.png");
    if (c == 125) tabs.setCurrentTabIndex (2);
    if (c == 140) save ("04-importar.png");
    if (c == 145) tabs.setCurrentTabIndex (3);
    if (c == 160) save ("05-sobre.png");
    if (c == 165)
    {
        tabs.setCurrentTabIndex (0);
        if (terms) terms->setVisible (true);
        snapshotDir.getChildFile ("verificacao.txt").replaceWithText (
            "biblioteca: " + proc.getLibraryStatus() + "\nreserva: " + String (proc.isUsingFallbackBank() ? "sim" : "nao")
            + "\npresets: " + String (proc.presets.entries().size()) + "\ntermos aceitos: " + String (proc.termsAccepted() ? "sim" : "nao") + "\n");
        snapshotCountdown = -1;
    }
}
