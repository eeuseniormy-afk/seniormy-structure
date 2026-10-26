#include "PluginEditor.h"
#include "AGBinaryData.h"

using namespace juce;

static String utf (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
// Mixer
GuitarsEditor::MixStrip::MixStrip()
{
    level.setSliderStyle (Slider::LinearVertical);
    level.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
    level.setTextValueSuffix (" dB");
    addAndMakeVisible (level);
    for (auto* k : { &pan, &tone, &body, &width, &reverb, &noises }) addAndMakeVisible (k);
    addAndMakeVisible (mute); addAndMakeVisible (solo);
}

void GuitarsEditor::MixStrip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Wood::black.withAlpha (0.55f)); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (accent.withAlpha (0.9f)); g.fillRoundedRectangle (r.removeFromTop (4.0f).reduced (8.0f, 0), 2.0f);
    g.setColour (Wood::ivory); g.setFont (WoodLookAndFeel::uiFont (14.0f, true));
    g.drawFittedText (title, getLocalBounds().removeFromTop (30).reduced (8, 4), Justification::centredLeft, 1, 0.7f);
}

void GuitarsEditor::MixStrip::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (26);
    auto bottom = r.removeFromBottom (24);
    mute.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2)); solo.setBounds (bottom);
    level.setBounds (r.removeFromLeft (70));
    r.removeFromLeft (6);
    const int kw = r.getWidth() / 2, kh = r.getHeight() / 3;
    LabeledKnob* ks[] = { &pan, &tone, &body, &width, &reverb, &noises };
    for (int i = 0; i < 6; ++i) ks[i]->setBounds (r.getX() + (i % 2) * kw, r.getY() + (i / 2) * kh, kw, kh);
}

//==============================================================================
static void setupLabel (Label& l, const String& text, float size = 12.5f)
{
    l.setText (text, dontSendNotification);
    l.setFont (WoodLookAndFeel::uiFont (size));
    l.setColour (Label::textColourId, Wood::ivorySoft);
    l.setMinimumHorizontalScale (0.7f);
}

GuitarsEditor::GuitarsEditor (GuitarsProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    logo = ImageCache::getFromMemory (AGBinaryData::logo256_png, AGBinaryData::logo256_pngSize);
    addAndMakeVisible (root);
    st = proc.getState();

    // ---- cabeçalho
    for (auto* c : std::initializer_list<Component*> { &presetBox, &prevPreset, &nextPreset, &savePreset, &deletePreset, &termsButton,
                                                      &aboutButton, &bpm, &bpmLabel, &playButton, &stopButton, &statusLabel })
        root.addAndMakeVisible (c);
    presetBox.onChange = [this]
    {
        if (updating) return;
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && proc.loadPresetEntry (idx)) reloadFromProcessor();
    };
    prevPreset.onClick = [this] { const int n = proc.presets.entries().size(); const int i = presetBox.getSelectedId() - 1; if (n > 0 && proc.loadPresetEntry ((i - 1 + n) % n)) reloadFromProcessor(); };
    nextPreset.onClick = [this] { const int n = proc.presets.entries().size(); const int i = presetBox.getSelectedId() - 1; if (n > 0 && proc.loadPresetEntry ((i + 1) % n)) reloadFromProcessor(); };
    savePreset.onClick = [this]
    {
        auto* w = new AlertWindow (utf ("Salvar preset"), utf ("Nome do preset do usuário:"), MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.getPresetName().upToLastOccurrenceOf (" *", false, false));
        w->addButton ("Salvar", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancelar", 0, KeyPress (KeyPress::escapeKey));
        SafePointer<GuitarsEditor> sp (this);
        w->enterModalState (true, ModalCallbackFunction::create ([sp, w] (int r)
        {
            if (r != 1 || sp == nullptr) return;
            String err;
            const auto name = w->getTextEditorContents ("name").trim();
            sp->push();
            if (! sp->proc.saveUserPreset (name, err)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SeniorMy", err);
            sp->refreshPresetBox();
        }), true);
    };
    deletePreset.onClick = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        const auto& list = proc.presets.entries();
        if (idx < 0 || idx >= list.size() || list[idx].factory)
        { AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "SeniorMy", utf ("Presets de fábrica não podem ser excluídos.")); return; }
        proc.presets.remove (list[idx]);
        refreshPresetBox();
    };
    termsButton.onClick = [this] { showTermsReadOnly(); };
    aboutButton.onClick = [this] { showAbout(); };
    bpm.setSliderStyle (Slider::LinearHorizontal);
    bpm.setTextBoxStyle (Slider::TextBoxRight, false, 56, 20);
    bpmAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "bpm", bpm);
    setupLabel (bpmLabel, "BPM");
    playButton.setClickingTogglesState (true);
    playAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "play", playButton);
    stopButton.onClick = [this]
    {
        if (auto* pp = proc.apvts.getParameter ("play")) pp->setValueNotifyingHost (0.0f);
        proc.engine.stopAll();
    };
    setupLabel (statusLabel, {}, 11.5f);
    statusLabel.setJustificationType (Justification::centredRight);

    // ---- partes
    for (int i = 0; i < sms::kParts; ++i)
    {
        auto& b = partButtons[(size_t) i];
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { selectPart (i); };
        root.addAndMakeVisible (b);
    }
    root.addAndMakeVisible (face);

    // ---- abas
    for (auto* pg : { &pageInst, &pageHarm, &pagePat, &pageMix }) pg->onResized = [this] { layoutPages(); };
    tabs.setTabBarDepth (34);
    tabs.setOutline (0);
    tabs.addTab ("Instrumento", Colours::transparentBlack, &pageInst, false);
    tabs.addTab ("Harmonia", Colours::transparentBlack, &pageHarm, false);
    tabs.addTab (utf ("Padrão"), Colours::transparentBlack, &pagePat, false);
    tabs.addTab ("Mixer", Colours::transparentBlack, &pageMix, false);
    root.addAndMakeVisible (tabs);

    // ---- Instrumento
    pageInst.addAndMakeVisible (picker);
    pageInst.addAndMakeVisible (diagram);
    picker.onPick = [this] (int inst)
    {
        part().instrument = inst;
        part().tuning = 0;
        // padrão idiomático ao trocar de instrumento (se o atual não fizer sentido)
        static const char* idiom[sms::NumInstruments] = { "Folk pop (↓ ↓↑ ↑↓↑)", "Balada dedilhada (p-i-m-a)", "Rumba flamenca", "Folk pop (↓ ↓↑ ↑↓↑)",
            "Ponteado caipira", "Samba / pagode (cavaco)", "Trêmolo (bandolim)", "Island strum (ukulele)", "Roll para frente (banjo)", "Raga lento (sitar)" };
        if (part().mode == sms::ModeOff) part().mode = sms::ModePattern;
        part().pattern = sms::patternByName (idiom[inst]);
        push(); refreshAll();
    };
    for (auto* l : { &modeL, &tuningL, &capoL, &positionL, &octaveL }) pageInst.addAndMakeVisible (l);
    setupLabel (modeL, "Modo da parte"); setupLabel (tuningL, utf ("Afinação")); setupLabel (capoL, "Capotraste");
    setupLabel (positionL, utf ("Posição dos acordes")); setupLabel (octaveL, utf ("Oitava (modo Teclado)"));
    for (auto* c : { &modeBox, &tuningBox, &capoBox, &positionBox, &octaveBox }) pageInst.addAndMakeVisible (c);
    for (int m = 0; m < sms::NumModes; ++m) modeBox.addItem (utf (sms::modeNamePt (m)), m + 1);
    for (int c = 0; c <= 9; ++c) capoBox.addItem (c == 0 ? String ("Sem capotraste") : String (c) + utf ("ª casa"), c + 1);
    positionBox.addItem (utf ("1ª posição (cordas soltas)"), 1); positionBox.addItem (utf ("Região da 5ª casa"), 2);
    positionBox.addItem (utf ("Região da 7ª casa"), 3); positionBox.addItem (utf ("Região da 9ª casa"), 4);
    for (int o = -2; o <= 2; ++o) octaveBox.addItem ((o > 0 ? "+" : "") + String (o), o + 3);
    modeBox.onChange = [this] { if (! updating) { part().mode = modeBox.getSelectedId() - 1; push(); refreshAll(); } };
    tuningBox.onChange = [this] { if (! updating) { part().tuning = tuningBox.getSelectedId() - 1; push(); refreshAll(); } };
    capoBox.onChange = [this] { if (! updating) { part().capo = capoBox.getSelectedId() - 1; push(); } };
    positionBox.onChange = [this] { if (! updating) { part().position = positionBox.getSelectedId() - 1; push(); } };
    octaveBox.onChange = [this] { if (! updating) { part().octave = octaveBox.getSelectedId() - 3; push(); } };
    pageInst.addAndMakeVisible (descLabel);
    setupLabel (descLabel, {}, 13.0f);
    descLabel.setColour (Label::textColourId, Wood::ivory);

    // ---- Harmonia
    for (auto* c : std::initializer_list<Component*> { &keyBox, &scaleBox, &splitBox, &beatsBox, &keyL, &scaleL, &splitL, &beatsL, &progHint, &importStatus,
                                                      &tetrads, &fieldMode, &latch, &progOn, &progStrip, &removeSlot, &clearProg, &importButton, &exportButton })
        pageHarm.addAndMakeVisible (c);
    setupLabel (keyL, "Tom"); setupLabel (scaleL, "Escala / modo"); setupLabel (splitL, utf ("Zona de acordes (mão esquerda) até"));
    setupLabel (beatsL, utf ("Duração do acorde selecionado"));
    setupLabel (progHint, utf ("Toque um acorde na mão esquerda (abaixo da divisão) e o instrumento toca o padrão. Ou monte uma progressão: escolha um quadro e clique num grau do campo harmônico."), 12.0f);
    setupLabel (importStatus, {}, 12.0f);
    importStatus.setColour (Label::textColourId, Wood::goldLight);
    static const char* keys[12] = { "C (Dó)", "C# / Db", "D (Ré)", "Eb / D#", "E (Mi)", "F (Fá)", "F# / Gb", "G (Sol)", "Ab / G#", "A (Lá)", "Bb / A#", "B (Si)" };
    for (int k = 0; k < 12; ++k) keyBox.addItem (utf (keys[k]), k + 1);
    for (int s = 0; s < (int) sms::Scale::count; ++s) scaleBox.addItem (utf (sms::scaleName ((sms::Scale) s)), s + 1);
    static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int n = 36; n <= 84; n += 1) splitBox.addItem (String (noteNames[n % 12]) + String (n / 12 - 1), n + 1);
    splitBox.addItem ("Teclado inteiro", 129);
    const int beatOpts[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
    for (int b : beatOpts) beatsBox.addItem (String (b) + (b == 1 ? " tempo" : " tempos"), b);
    keyBox.onChange = [this] { if (! updating) { st.harmony.keyTonic = keyBox.getSelectedId() - 1; push(); refreshAll(); } };
    scaleBox.onChange = [this] { if (! updating) { st.harmony.keyScale = scaleBox.getSelectedId() - 1; push(); refreshAll(); } };
    splitBox.onChange = [this] { if (! updating) { st.harmony.split = splitBox.getSelectedId() - 1; push(); } };
    beatsBox.onChange = [this]
    {
        if (updating || selectedSlot >= st.harmony.numProg) return;
        st.harmony.prog[(size_t) selectedSlot].beats = beatsBox.getSelectedId(); push(); refreshAll();
    };
    tetrads.onClick = [this] { st.harmony.tetrads = tetrads.getToggleState(); push(); refreshAll(); };
    fieldMode.onClick = [this] { st.harmony.fieldMode = fieldMode.getToggleState(); push(); };
    latch.onClick = [this] { st.harmony.latch = latch.getToggleState(); push(); };
    progOn.onClick = [this] { st.harmony.progressionOn = progOn.getToggleState(); push(); };
    for (int d = 0; d < 7; ++d)
    {
        auto& b = degreeButtons[(size_t) d];
        b.onClick = [this, d] { assignDegree (d); };
        pageHarm.addAndMakeVisible (b);
    }
    progStrip.onSelect = [this] (int i) { selectedSlot = jmin (i, st.harmony.numProg); refreshAll(); };
    removeSlot.onClick = [this]
    {
        auto& h = st.harmony;
        if (selectedSlot >= h.numProg) return;
        for (int i = selectedSlot; i + 1 < h.numProg; ++i) h.prog[(size_t) i] = h.prog[(size_t) (i + 1)];
        --h.numProg; selectedSlot = jmin (selectedSlot, h.numProg); push(); refreshAll();
    };
    clearProg.onClick = [this] { st.harmony.numProg = 0; selectedSlot = 0; st.harmony.progressionOn = false; push(); refreshAll(); };
    importButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> (utf ("Importar loop (áudio ou MIDI)"), File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.mid;*.midi");
        SafePointer<GuitarsEditor> sp (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [sp] (const FileChooser& fc)
        {
            if (sp == nullptr) return;
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            sp->importStatus.setText (utf ("Analisando ") + f.getFileName() + utf ("…"), dontSendNotification);
            SafePointer<GuitarsEditor> sp2 (sp.getComponent());
            sp->proc.importLoop (f, [sp2] (bool ok, String msg)
            {
                if (sp2 == nullptr) return;
                sp2->importStatus.setText (msg, dontSendNotification);
                if (ok) sp2->reloadFromProcessor();
            });
        });
    };
    exportButton.onClick = [this]
    {
        push();
        chooser = std::make_unique<FileChooser> ("Exportar MIDI", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SeniorMy Acoustic Guitars.mid"), "*.mid");
        SafePointer<GuitarsEditor> sp (this);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [sp] (const FileChooser& fc)
        {
            if (sp == nullptr) return;
            auto f = fc.getResult();
            if (f == File()) return;
            if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
            String err;
            const auto out = sp->proc.exportMidi (2, f, err);
            sp->importStatus.setText (out.existsAsFile() ? utf ("MIDI exportado: ") + out.getFileName() : err, dontSendNotification);
        });
    };

    // ---- Padrão
    for (auto* c : std::initializer_list<Component*> { &patternBox, &spbBox, &stepsBox, &patternL, &spbL, &stepsL, &savePattern, &clearPattern,
                                                      &swing, &strum, &humanize, &palm, &accent, &grid })
        pagePat.addAndMakeVisible (c);
    pagePat.addAndMakeVisible (gridHint);
    setupLabel (gridHint, utf ("Grade: clique liga/desliga · arraste pinta · botão direito ou Shift muda a força (forte → médio → suave → fantasma). Cordas: 1 = a mais aguda."), 12.0f);
    setupLabel (patternL, utf ("Padrão")); setupLabel (spbL, utf ("Subdivisão")); setupLabel (stepsL, "Passos");
    const int spbs[] = { 1, 2, 3, 4, 6, 8 };
    const char* spbNames[] = { "1 por tempo (semínima)", "2 por tempo (colcheia)", "3 por tempo (tercina)", "4 por tempo (semicolcheia)", "6 por tempo (sextina)", "8 por tempo (fusa)" };
    for (int i = 0; i < 6; ++i) spbBox.addItem (utf (spbNames[i]), spbs[i]);
    for (int s = 1; s <= sms::kMaxSteps; ++s) stepsBox.addItem (String (s), s);
    patternBox.onChange = [this] { if (! updating) loadPatternChoice (patternBox.getSelectedId()); };
    spbBox.onChange = [this] { if (! updating) { part().pattern.stepsPerBeat = spbBox.getSelectedId(); push(); refreshAll(); } };
    stepsBox.onChange = [this] { if (! updating) { part().pattern.numSteps = stepsBox.getSelectedId(); push(); refreshAll(); } };
    auto knob = [this] (LabeledKnob& k, float sms::Pattern::* field)
    {
        k.slider.onValueChange = [this, &k, field] { if (! updating) { part().pattern.*field = (float) k.slider.getValue(); grid.setPattern (part().pattern); push(); } };
    };
    knob (swing, &sms::Pattern::swing); knob (strum, &sms::Pattern::strumScale); knob (humanize, &sms::Pattern::humanize);
    knob (palm, &sms::Pattern::mute); knob (accent, &sms::Pattern::accent);
    grid.onEdit = [this] { part().pattern.grid = grid.getPattern().grid; push(); };
    savePattern.onClick = [this]
    {
        auto* w = new AlertWindow (utf ("Salvar padrão"), utf ("Nome do padrão:"), MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", utf (part().pattern.name));
        w->addButton ("Salvar", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancelar", 0, KeyPress (KeyPress::escapeKey));
        SafePointer<GuitarsEditor> sp (this);
        w->enterModalState (true, ModalCallbackFunction::create ([sp, w] (int r)
        {
            if (r != 1 || sp == nullptr) return;
            auto pat = sp->part().pattern;
            pat.setName (w->getTextEditorContents ("name").trim().toRawUTF8(), "Usuário");
            String err;
            if (! PatternLibrary::save (pat, err)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SeniorMy", err);
            sp->part().pattern = pat; sp->push(); sp->refreshAll();
        }), true);
    };
    clearPattern.onClick = [this] { part().pattern.grid = {}; push(); refreshAll(); };

    // ---- Mixer
    for (int i = 0; i < sms::kParts; ++i)
    {
        auto& s = strips[(size_t) i];
        s.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "level" + String (i + 1), s.level);
        auto bind = [this, i] (LabeledKnob& k, float sms::PartState::* f)
        {
            k.slider.onValueChange = [this, i, &k, f] { if (! updating) { st.parts[(size_t) i].*f = (float) k.slider.getValue(); push(); } };
        };
        bind (s.pan, &sms::PartState::pan); bind (s.tone, &sms::PartState::tone); bind (s.body, &sms::PartState::body);
        bind (s.width, &sms::PartState::width); bind (s.reverb, &sms::PartState::reverb); bind (s.noises, &sms::PartState::noises);
        s.mute.onClick = [this, i] { st.parts[(size_t) i].mute = strips[(size_t) i].mute.getToggleState(); push(); };
        s.solo.onClick = [this, i] { st.parts[(size_t) i].solo = strips[(size_t) i].solo.getToggleState(); push(); };
        pageMix.addAndMakeVisible (s);
    }
    for (auto* c : std::initializer_list<Component*> { &masterVol, &masterRev, &room, &masterWidth, &masterL, &masterVolL, &masterRevL }) pageMix.addAndMakeVisible (c);
    masterVol.setSliderStyle (Slider::LinearVertical); masterVol.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16); masterVol.setTextValueSuffix (" dB");
    masterRev.setSliderStyle (Slider::LinearVertical); masterRev.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
    masterAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterVol);
    revAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "reverb", masterRev);
    setupLabel (masterL, "Master", 14.0f); masterL.setColour (Label::textColourId, Wood::ivory);
    setupLabel (masterVolL, "Volume"); setupLabel (masterRevL, "Reverb");
    masterVolL.setJustificationType (Justification::centred); masterRevL.setJustificationType (Justification::centred);
    room.slider.onValueChange = [this] { if (! updating) { st.master.room = (float) room.slider.getValue(); push(); } };
    masterWidth.slider.onValueChange = [this] { if (! updating) { st.master.width = (float) masterWidth.slider.getValue(); push(); } };

    // ---- rodapé
    root.addAndMakeVisible (keyboard);
    root.addAndMakeVisible (chordLabel);
    root.addAndMakeVisible (keyInfo);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (19.0f);
    keyboard.setScrollButtonsVisible (true);
    chordLabel.setFont (WoodLookAndFeel::titleFont (40.0f));
    chordLabel.setColour (Label::textColourId, Wood::ivory);
    chordLabel.setJustificationType (Justification::centred);
    setupLabel (keyInfo, {}, 12.5f);
    keyInfo.setJustificationType (Justification::centred);

    if (! proc.termsAccepted())
    {
        SafePointer<GuitarsEditor> sp (this);
        terms = std::make_unique<TermsOverlay> ([sp]
        {
            if (sp == nullptr) return;
            sp->proc.markTermsAccepted();
            MessageManager::callAsync ([sp] { if (sp != nullptr && sp->terms) { sp->root.removeChildComponent (sp->terms.get()); sp->terms.reset(); } });
        });
        root.addAndMakeVisible (*terms);
    }

    refreshPresetBox();
    reloadFromProcessor();
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    setSize (roundToInt (baseW * jlimit (0.6f, 2.0f, proc.uiScale)), roundToInt (baseH * jlimit (0.6f, 2.0f, proc.uiScale)));

    const auto snap = SystemStats::getEnvironmentVariable ("SENIORMY_SNAPSHOT_DIR", {});
    if (snap.isNotEmpty()) { snapshotDir = File (snap); snapshotDir.createDirectory(); snapshotCountdown = 0; }
    startTimerHz (30);
}

GuitarsEditor::~GuitarsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GuitarsEditor::paint (Graphics& g) { g.fillAll (Wood::black); }

void GuitarsEditor::resized()
{
    const float sc = (float) getWidth() / (float) baseW;
    proc.uiScale = sc;
    root.setBounds (0, 0, baseW, baseH);
    root.setTransform (AffineTransform::scale (sc));

    // cabeçalho
    presetBox.setBounds (420, 20, 262, 32);
    prevPreset.setBounds (386, 20, 30, 32); nextPreset.setBounds (686, 20, 30, 32);
    savePreset.setBounds (722, 20, 68, 32); deletePreset.setBounds (794, 20, 68, 32);
    termsButton.setBounds (1128, 14, 96, 22); aboutButton.setBounds (1128, 40, 96, 22);
    bpmLabel.setBounds (870, 20, 34, 32); bpm.setBounds (900, 20, 154, 32);
    playButton.setBounds (1058, 14, 64, 24); stopButton.setBounds (1058, 40, 64, 22);
    statusLabel.setBounds (386, 52, 476, 14);

    for (int i = 0; i < sms::kParts; ++i) partButtons[(size_t) i].setBounds (310 + i * 200, 74, 194, 30);
    face.setBounds (10, 74, 290, 482);
    tabs.setBounds (310, 108, 920, 448);

    layoutPages();

    // rodapé
    chordLabel.setBounds (20, 580, 270, 60);
    keyInfo.setBounds (20, 640, 270, 40);
    keyboard.setBounds (316, 600, 908, 180);
    if (terms) terms->setBounds (root.getLocalBounds());
    if (about) about->setBounds (root.getLocalBounds());
}

void GuitarsEditor::layoutPages()
{
    // páginas (tamanho interno ≈ 920 × 414)
    auto pageBounds = [] (Component& c) { return c.getLocalBounds().reduced (12); };
    auto r = pageBounds (pageInst);
    if (r.getWidth() > 100)
    {
        picker.setBounds (r.getX(), r.getY(), 590, 250);
        diagram.setBounds (r.getX() + 600, r.getY(), r.getWidth() - 600, 250);
        int x = r.getX(); const int y = r.getY() + 262, w = 172;
        Label* ls[] = { &modeL, &tuningL, &capoL, &positionL, &octaveL };
        ComboBox* cs[] = { &modeBox, &tuningBox, &capoBox, &positionBox, &octaveBox };
        for (int i = 0; i < 5; ++i) { ls[i]->setBounds (x, y, w - 6, 18); cs[i]->setBounds (x, y + 20, w - 8, 30); x += w + 6; }
        descLabel.setBounds (r.getX(), y + 62, r.getWidth(), 60);
    }
    r = pageBounds (pageHarm);
    if (r.getWidth() > 100)
    {
        keyL.setBounds (r.getX(), r.getY(), 140, 18); keyBox.setBounds (r.getX(), r.getY() + 20, 140, 30);
        scaleL.setBounds (r.getX() + 150, r.getY(), 170, 18); scaleBox.setBounds (r.getX() + 150, r.getY() + 20, 170, 30);
        splitL.setBounds (r.getX() + 330, r.getY(), 230, 18); splitBox.setBounds (r.getX() + 330, r.getY() + 20, 150, 30);
        tetrads.setBounds (r.getX() + 570, r.getY() - 2, 150, 26); latch.setBounds (r.getX() + 730, r.getY() - 2, 160, 26);
        fieldMode.setBounds (r.getX() + 570, r.getY() + 24, 320, 26);
        const int dy = r.getY() + 62, dw = (r.getWidth() - 6 * 6) / 7;
        for (int d = 0; d < 7; ++d) degreeButtons[(size_t) d].setBounds (r.getX() + d * (dw + 6), dy, dw, 48);
        progStrip.setBounds (r.getX(), dy + 58, r.getWidth(), 130);
        const int by = dy + 196;
        progOn.setBounds (r.getX(), by + 4, 170, 26);
        beatsL.setBounds (r.getX() + 180, by - 14, 200, 16); beatsBox.setBounds (r.getX() + 180, by + 2, 140, 30);
        removeSlot.setBounds (r.getX() + 330, by + 2, 90, 30); clearProg.setBounds (r.getX() + 426, by + 2, 80, 30);
        importButton.setBounds (r.getRight() - 300, by + 2, 146, 30); exportButton.setBounds (r.getRight() - 146, by + 2, 146, 30);
        importStatus.setBounds (r.getX(), by + 38, r.getWidth(), 18);
        progHint.setBounds (r.getX(), by + 56, r.getWidth(), 34);
    }
    r = pageBounds (pagePat);
    if (r.getWidth() > 100)
    {
        patternL.setBounds (r.getX(), r.getY(), 280, 18); patternBox.setBounds (r.getX(), r.getY() + 20, 280, 30);
        spbL.setBounds (r.getX() + 290, r.getY(), 200, 18); spbBox.setBounds (r.getX() + 290, r.getY() + 20, 200, 30);
        stepsL.setBounds (r.getX() + 500, r.getY(), 70, 18); stepsBox.setBounds (r.getX() + 500, r.getY() + 20, 70, 30);
        savePattern.setBounds (r.getX() + 580, r.getY() + 20, 120, 30); clearPattern.setBounds (r.getX() + 706, r.getY() + 20, 110, 30);
        const int ky = r.getY() + 58; int kx = r.getX();
        for (auto* k : { &swing, &strum, &humanize, &palm, &accent }) { k->setBounds (kx, ky, 120, 74); kx += 128; }
        gridHint.setBounds (kx + 6, ky + 6, r.getRight() - kx - 6, 62);
        grid.setBounds (r.getX(), ky + 80, r.getWidth(), r.getBottom() - (ky + 80));
    }
    r = pageBounds (pageMix);
    if (r.getWidth() > 100)
    {
        const int sw = 230;
        for (int i = 0; i < sms::kParts; ++i) strips[(size_t) i].setBounds (r.getX() + i * (sw + 8), r.getY(), sw, r.getHeight());
        auto m = Rectangle<int> (r.getX() + 3 * (sw + 8), r.getY(), r.getRight() - (r.getX() + 3 * (sw + 8)), r.getHeight());
        masterL.setBounds (m.getX() + 8, m.getY() + 4, 120, 22);
        masterVolL.setBounds (m.getX() + 4, m.getY() + 30, 70, 16); masterVol.setBounds (m.getX() + 4, m.getY() + 48, 70, 200);
        masterRevL.setBounds (m.getX() + 80, m.getY() + 30, 70, 16); masterRev.setBounds (m.getX() + 80, m.getY() + 48, 70, 200);
        room.setBounds (m.getX() + 4, m.getY() + 260, 74, 80); masterWidth.setBounds (m.getX() + 80, m.getY() + 260, 74, 80);
    }

}

void GuitarsEditor::Root::paint (Graphics& g)
{
    WoodLookAndFeel::paintWood (g, getLocalBounds().toFloat());
    auto head = Rectangle<float> (10, 8, (float) getWidth() - 20, 60);
    WoodLookAndFeel::paintPanel (g, head, 12.0f);
    g.drawImage (ed.logo, Rectangle<float> (18, 12, 52, 52), RectanglePlacement::centred);
    g.setColour (Wood::ivory);
    g.setFont (WoodLookAndFeel::titleFont (26.0f));
    g.drawText ("SeniorMy", Rectangle<float> (80, 12, 120, 30), Justification::centredLeft);
    g.setColour (Wood::gold);
    g.drawText ("Acoustic Guitars", Rectangle<float> (190, 12, 200, 30), Justification::centredLeft);
    g.setColour (Wood::ivorySoft);
    g.setFont (WoodLookAndFeel::uiFont (12.0f));
    g.drawText (String::fromUTF8 ("Violões, viola, cavaquinho, banjo e mais"), Rectangle<float> (82, 42, 300, 18), Justification::centredLeft);
    // rodapé
    auto foot = Rectangle<float> (10, 564, (float) getWidth() - 20, 228);
    WoodLookAndFeel::paintPanel (g, foot, 12.0f);
    g.setColour (Wood::goldDark.withAlpha (0.6f));
    g.drawVerticalLine (304, 576, 780);
    g.setColour (Wood::gold);
    g.setFont (WoodLookAndFeel::uiFont (12.0f, true));
    g.drawText ("ACORDE", Rectangle<float> (20, 570, 270, 16), Justification::centred);
    g.drawText (String::fromUTF8 ("TECLADO — mão esquerda: acordes · mão direita: modo Teclado"), Rectangle<float> (316, 576, 908, 18), Justification::centredLeft);
    // medidor
    auto mt = Rectangle<float> (30, 700, 250, 8);
    g.setColour (Wood::well); g.fillRoundedRectangle (mt, 4.0f);
    g.setColour (ed.meterValue > 0.95f ? Colours::orangered : Wood::violet);
    g.fillRoundedRectangle (mt.withWidth (mt.getWidth() * jlimit (0.0f, 1.0f, ed.meterValue)), 4.0f);
    g.setColour (Wood::ivorySoft);
    g.setFont (WoodLookAndFeel::uiFont (11.0f));
    g.drawFittedText (String::fromUTF8 ("© 2026 W. Carolino — SeniorMy\nCNPJ 69.610.819/0001-77"), Rectangle<int> (20, 752, 280, 30), Justification::centred, 2, 0.9f);
    g.drawText ("v" JucePlugin_VersionString, Rectangle<float> (20, 734, 280, 16), Justification::centred);
}

//==============================================================================
void GuitarsEditor::push() { proc.setState (st); }

void GuitarsEditor::reloadFromProcessor()
{
    st = proc.getState();
    seenVersion = proc.stateVersion.load();
    refreshAll();
    refreshPresetBox();
}

void GuitarsEditor::refreshPresetBox()
{
    const ScopedValueSetter<bool> sv (updating, true);
    presetBox.clear (dontSendNotification);
    const auto& list = proc.presets.entries();
    String lastCat;
    const auto current = proc.getPresetName().upToLastOccurrenceOf (" *", false, false);
    int sel = 0;
    for (int i = 0; i < list.size(); ++i)
    {
        if (list[i].category != lastCat) { presetBox.addSectionHeading (list[i].category); lastCat = list[i].category; }
        presetBox.addItem (list[i].name, i + 1);
        if (list[i].name == current) sel = i + 1;
    }
    if (sel > 0) presetBox.setSelectedId (sel, dontSendNotification);
    else presetBox.setText (proc.getPresetName(), dontSendNotification);
}

void GuitarsEditor::selectPart (int p)
{
    proc.uiPart = jlimit (0, sms::kParts - 1, p);
    refreshAll();
}

String GuitarsEditor::chordName (const sms::Chord& c) const
{
    if (! c.valid()) return {};
    sms::Key k; k.tonic = st.harmony.keyTonic; k.scale = (sms::Scale) st.harmony.keyScale;
    return String::fromUTF8 (c.name (k.usesFlats()).c_str());
}

void GuitarsEditor::refreshPatternBox()
{
    patternBox.clear (dontSendNotification);
    const auto& fp = proc.engine.factory();
    String lastCat; int selId = 0;
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        const auto cat = String::fromUTF8 (fp[(size_t) i].category);
        if (cat != lastCat) { patternBox.addSectionHeading (cat); lastCat = cat; }
        patternBox.addItem (String::fromUTF8 (fp[(size_t) i].name), i + 1);
        if (std::strcmp (fp[(size_t) i].name, part().pattern.name) == 0) selId = i + 1;
    }
    userPatterns = PatternLibrary::userFiles();
    if (! userPatterns.isEmpty())
    {
        patternBox.addSectionHeading (String::fromUTF8 ("Meus padrões"));
        for (int i = 0; i < userPatterns.size(); ++i)
        {
            patternBox.addItem (userPatterns[i].getFileNameWithoutExtension(), 1000 + i);
            if (selId == 0 && userPatterns[i].getFileNameWithoutExtension() == String::fromUTF8 (part().pattern.name)) selId = 1000 + i;
        }
    }
    if (selId) patternBox.setSelectedId (selId, dontSendNotification);
    else patternBox.setText (String::fromUTF8 (part().pattern.name) + String::fromUTF8 (" (editado)"), dontSendNotification);
}

void GuitarsEditor::loadPatternChoice (int id)
{
    if (id >= 1000)
    {
        const int i = id - 1000;
        if (i >= 0 && i < userPatterns.size()) { sms::Pattern p; if (PatternLibrary::load (userPatterns[i], p)) part().pattern = p; }
    }
    else if (id >= 1 && id <= (int) proc.engine.factory().size()) part().pattern = proc.engine.factory()[(size_t) (id - 1)];
    push(); refreshAll();
}

void GuitarsEditor::assignDegree (int d)
{
    sms::Key k; k.tonic = st.harmony.keyTonic; k.scale = (sms::Scale) st.harmony.keyScale;
    const auto c = sms::diatonicChord (k, d, st.harmony.tetrads);
    auto& h = st.harmony;
    if (selectedSlot < sms::kMaxProg)
    {
        if (selectedSlot >= h.numProg) { selectedSlot = h.numProg; h.prog[(size_t) selectedSlot].beats = h.numProg > 0 ? h.prog[(size_t) (h.numProg - 1)].beats : 4; ++h.numProg; }
        auto& s = h.prog[(size_t) selectedSlot];
        s.root = c.root; s.quality = (int) c.quality; s.bass = -1;
        selectedSlot = jmin (selectedSlot + 1, sms::kMaxProg - 1);
        push();
    }
    proc.engine.auditionChord (c, 0.8f);
    refreshAll();
}

void GuitarsEditor::refreshAll()
{
    const ScopedValueSetter<bool> sv (updating, true);
    const int p = proc.uiPart;
    auto& ps = part();
    const auto& d = sms::instrument (ps.instrument);
    for (int i = 0; i < sms::kParts; ++i)
    {
        const auto& q = st.parts[(size_t) i];
        partButtons[(size_t) i].setButtonText ("Parte " + String (i + 1) + ": " + (q.mode == sms::ModeOff ? String::fromUTF8 ("desligada") : String::fromUTF8 (sms::instrument (q.instrument).namePt)));
        partButtons[(size_t) i].setToggleState (i == p, dontSendNotification);
        strips[(size_t) i].title = "Parte " + String (i + 1) + String::fromUTF8 (" · ") + (q.mode == sms::ModeOff ? String::fromUTF8 ("desligada") : String::fromUTF8 (sms::instrument (q.instrument).namePt));
        strips[(size_t) i].accent = Colour (sms::instrument (q.instrument).colour);
        strips[(size_t) i].pan.slider.setValue (q.pan, dontSendNotification);
        strips[(size_t) i].tone.slider.setValue (q.tone, dontSendNotification);
        strips[(size_t) i].body.slider.setValue (q.body, dontSendNotification);
        strips[(size_t) i].width.slider.setValue (q.width, dontSendNotification);
        strips[(size_t) i].reverb.slider.setValue (q.reverb, dontSendNotification);
        strips[(size_t) i].noises.slider.setValue (q.noises, dontSendNotification);
        strips[(size_t) i].mute.setToggleState (q.mute, dontSendNotification);
        strips[(size_t) i].solo.setToggleState (q.solo, dontSendNotification);
        strips[(size_t) i].setAlpha (q.mode == sms::ModeOff ? 0.55f : 1.0f);
        strips[(size_t) i].repaint();
    }
    face.setPartLabel ("PARTE " + String (p + 1) + String::fromUTF8 (" · ") + String::fromUTF8 (sms::modeNamePt (ps.mode)).toUpperCase());
    face.setInstrument (ps.instrument, d.source == sms::Source::Modeled || proc.engine.instrumentHasSamples (ps.instrument));
    picker.setSelected (ps.instrument);
    modeBox.setSelectedId (ps.mode + 1, dontSendNotification);
    tuningBox.clear (dontSendNotification);
    for (int t = 0; t < (int) d.tunings.size(); ++t) tuningBox.addItem (String::fromUTF8 (d.tunings[(size_t) t].name), t + 1);
    tuningBox.setSelectedId (ps.tuning + 1, dontSendNotification);
    capoBox.setSelectedId (ps.capo + 1, dontSendNotification);
    positionBox.setSelectedId (ps.position + 1, dontSendNotification);
    octaveBox.setSelectedId (ps.octave + 3, dontSendNotification);
    descLabel.setText (proc.getPresetName() + String::fromUTF8 (" — ") + proc.getPresetDescription(), dontSendNotification);

    keyBox.setSelectedId (st.harmony.keyTonic + 1, dontSendNotification);
    scaleBox.setSelectedId (st.harmony.keyScale + 1, dontSendNotification);
    splitBox.setSelectedId (st.harmony.split + 1, dontSendNotification);
    tetrads.setToggleState (st.harmony.tetrads, dontSendNotification);
    fieldMode.setToggleState (st.harmony.fieldMode, dontSendNotification);
    latch.setToggleState (st.harmony.latch, dontSendNotification);
    progOn.setToggleState (st.harmony.progressionOn, dontSendNotification);
    sms::Key k; k.tonic = st.harmony.keyTonic; k.scale = (sms::Scale) st.harmony.keyScale;
    for (int dg = 0; dg < 7; ++dg)
    {
        const auto c = sms::diatonicChord (k, dg, st.harmony.tetrads);
        degreeButtons[(size_t) dg].setButtonText (String (sms::degreeRoman (dg, c.isMinorThird())) + "\n" + chordName (c));
    }
    if (selectedSlot < st.harmony.numProg) beatsBox.setSelectedId (st.harmony.prog[(size_t) selectedSlot].beats, dontSendNotification);
    progStrip.setData (st.harmony, selectedSlot, -1);

    refreshPatternBox();
    spbBox.setSelectedId (ps.pattern.stepsPerBeat, dontSendNotification);
    stepsBox.setSelectedId (ps.pattern.numSteps, dontSendNotification);
    swing.slider.setValue (ps.pattern.swing, dontSendNotification);
    strum.slider.setValue (ps.pattern.strumScale, dontSendNotification);
    humanize.slider.setValue (ps.pattern.humanize, dontSendNotification);
    palm.slider.setValue (ps.pattern.mute, dontSendNotification);
    accent.slider.setValue (ps.pattern.accent, dontSendNotification);
    grid.setPattern (ps.pattern);
    grid.setStringCount ((int) sms::tuningOf (ps.instrument, ps.tuning).courses.size());
    room.slider.setValue (st.master.room, dontSendNotification);
    masterWidth.slider.setValue (st.master.width, dontSendNotification);
    root.repaint();
}

void GuitarsEditor::timerCallback()
{
    if (proc.stateVersion.load() != seenVersion) reloadFromProcessor();
    const int p = proc.uiPart;
    const auto& ps = st.parts[(size_t) p];
    // acorde atual (ou o primeiro da progressão, como prévia)
    auto chord = proc.engine.getChord();
    bool preview = false;
    std::array<int, sms::kMaxCourses> frets {};
    if (chord.valid()) frets = proc.engine.getFrets (p);
    else if (st.harmony.numProg > 0)
    {
        preview = true;
        chord.root = st.harmony.prog[0].root; chord.quality = (sms::Quality) st.harmony.prog[0].quality; chord.bass = st.harmony.prog[0].bass;
        sms::VoicingRequest rq; rq.chord = chord; rq.position = ps.position; rq.capo = ps.capo;
        if (ps.capo > 0) rq.chord.root = sms::pc (chord.root - ps.capo);
        const auto v = sms::findVoicing (sms::instrument (ps.instrument), sms::tuningOf (ps.instrument, ps.tuning), rq);
        for (int i = 0; i < sms::kMaxCourses; ++i) frets[(size_t) i] = i < v.numCourses ? v.fret[(size_t) i] : -2;
    }
    const auto name = chordName (chord);
    diagram.set (ps.instrument, ps.tuning, ps.capo, frets, name + (preview ? String::fromUTF8 ("  (prévia)") : String()));
    if (chordLabel.getText() != (name.isEmpty() ? String::fromUTF8 ("—") : name)) chordLabel.setText (name.isEmpty() ? String::fromUTF8 ("—") : name, dontSendNotification);
    sms::Key k; k.tonic = st.harmony.keyTonic; k.scale = (sms::Scale) st.harmony.keyScale;
    const auto info = String::fromUTF8 ("Tom: ") + String::fromUTF8 (k.name().c_str())
                    + (proc.hostPlaying.load() ? String::fromUTF8 (" · DAW ") + String (proc.hostBpm.load(), 1) + " BPM" : String());
    if (keyInfo.getText() != info) keyInfo.setText (info, dontSendNotification);
    grid.setPlayStep (proc.engine.isRunning() ? proc.engine.getStep (p) : -1);
    const int playing = (st.harmony.progressionOn && proc.engine.isRunning()) ? proc.engine.progressionIndexAt (proc.engine.getBeat()) : -1;
    if (playing != lastPlaying) { lastPlaying = playing; progStrip.setData (st.harmony, selectedSlot, playing); }
    const float m = proc.engine.getMeter();
    if (std::abs (m - meterValue) > 0.01f) { meterValue = m; root.repaint (20, 696, 270, 16); }
    String status = proc.getLibraryStatus();
    if (proc.isLibraryLoading()) status = String::fromUTF8 ("Carregando amostras… ") + String (roundToInt (proc.getLibraryProgress() * 100)) + "%";
    else if (! proc.hasLibrary()) status = String::fromUTF8 ("Biblioteca não instalada: instrumentos gravados tocam como modelos");
    if (statusLabel.getText() != status) statusLabel.setText (status, dontSendNotification);
    // a face reflete a biblioteca assim que ela termina de carregar
    face.setInstrument (ps.instrument, sms::instrument (ps.instrument).source == sms::Source::Modeled || proc.engine.instrumentHasSamples (ps.instrument));
    if (snapshotCountdown >= 0) takeSnapshots();
}

//==============================================================================
namespace
{
    struct AboutOverlay : public Component
    {
        explicit AboutOverlay (std::function<void()> onClose) : closeCb (std::move (onClose))
        {
            text.setMultiLine (true); text.setReadOnly (true); text.setScrollbarsShown (true); text.setCaretVisible (false);
            text.setFont (WoodLookAndFeel::uiFont (13.0f));
            text.setText (String::fromUTF8 ("SeniorMy Acoustic Guitars ") + JucePlugin_VersionString + "\n"
                          + String::fromUTF8 (AGBinaryData::THIRDPARTYNOTICES_txt, AGBinaryData::THIRDPARTYNOTICES_txtSize));
            text.moveCaretToTop (false);
            addAndMakeVisible (text);
            close.onClick = [this] { if (closeCb) closeCb(); };
            addAndMakeVisible (close);
        }
        void paint (Graphics& g) override
        {
            g.fillAll (Colours::black.withAlpha (0.6f));
            WoodLookAndFeel::paintPanel (g, getLocalBounds().reduced (60, 34).toFloat(), 12.0f);
            g.setColour (Wood::ivory); g.setFont (WoodLookAndFeel::titleFont (26.0f));
            g.drawText (String::fromUTF8 ("Sobre · créditos e licenças"), getLocalBounds().reduced (90, 44).removeFromTop (34), Justification::centredLeft);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (90, 44);
            r.removeFromTop (40);
            close.setBounds (r.removeFromBottom (34).removeFromRight (130));
            r.removeFromBottom (8);
            text.setBounds (r);
        }
        TextEditor text;
        TextButton close { "Fechar" };
        std::function<void()> closeCb;
    };
}

void GuitarsEditor::showAbout()
{
    if (about) return;
    SafePointer<GuitarsEditor> sp (this);
    about = std::make_unique<AboutOverlay> ([sp]
    {
        MessageManager::callAsync ([sp] { if (sp != nullptr && sp->about) { sp->root.removeChildComponent (sp->about.get()); sp->about.reset(); } });
    });
    root.addAndMakeVisible (*about);
    about->setBounds (root.getLocalBounds());
}

void GuitarsEditor::showTermsReadOnly()
{
    if (terms) return;
    SafePointer<GuitarsEditor> sp (this);
    terms = std::make_unique<TermsOverlay> ([sp]
    {
        MessageManager::callAsync ([sp] { if (sp != nullptr && sp->terms && sp->proc.termsAccepted()) { sp->root.removeChildComponent (sp->terms.get()); sp->terms.reset(); } });
    }, true);
    root.addAndMakeVisible (*terms);
    terms->setBounds (root.getLocalBounds());
}

void GuitarsEditor::takeSnapshots()
{
    // verificação no CI: SENIORMY_SNAPSHOT_DIR=/pasta → grava capturas de cada tela
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
    if (c == 85) tabs.setCurrentTabIndex (0);
    if (c == 100) save ("02-instrumento.png");
    if (c == 105) tabs.setCurrentTabIndex (1);
    if (c == 120) save ("03-harmonia.png");
    if (c == 125) tabs.setCurrentTabIndex (2);
    if (c == 140) save ("04-padrao.png");
    if (c == 145) tabs.setCurrentTabIndex (3);
    if (c == 160) save ("05-mixer.png");
    if (c == 165) { tabs.setCurrentTabIndex (0); proc.loadPresetEntry (7); reloadFromProcessor(); }
    if (c == 185) save ("06-viola-caipira.png");
    if (c == 190) { proc.loadPresetEntry (11); reloadFromProcessor(); tabs.setCurrentTabIndex (2); }
    if (c == 205) save ("07-cavaquinho-padrao.png");
    if (c == 210) showAbout();
    if (c == 225) save ("08-sobre.png");
    if (c == 230)
    {
        if (about) { root.removeChildComponent (about.get()); about.reset(); }
        tabs.setCurrentTabIndex (0);
        if (terms) terms->setVisible (true);
        snapshotDir.getChildFile ("verificacao.txt").replaceWithText (
            "biblioteca: " + proc.getLibraryStatus() + "\npresets: " + String (proc.presets.entries().size())
            + "\ntermos aceitos: " + String (proc.termsAccepted() ? "sim" : "nao") + "\n");
        snapshotCountdown = -1;
    }
}
