#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LoopImporter.h"
#include "Terms.h"

using namespace juce;

namespace ids
{
    static const char* levels[6] = { "lvlVln1", "lvlVln2", "lvlVla", "lvlVc", "lvlCb", "lvlHorns" };
    static const char* lanes[8] = { "lane1", "lane2", "lane3", "lane4", "lane5", "lane6", "lane7", "lane8" };
}

AudioProcessorValueTreeState::ParameterLayout StructureProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100)) + "%"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return x <= -59.5f ? String ("-inf dB") : String (x, 1) + " dB"; });
    auto add01 = [&] (const char* id, const char* name, float def)
    { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, String::fromUTF8 (name), NormalisableRange<float> (0.0f, 1.0f), def, pct)); };
    auto addDb = [&] (const char* id, const char* name, float lo, float hi, float def)
    { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, String::fromUTF8 (name), NormalisableRange<float> (lo, hi, 0.1f), def, db)); };
    auto addBool = [&] (const char* id, const char* name, bool def)
    { v.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, String::fromUTF8 (name), def)); };
    auto addChoice = [&] (const char* id, const char* name, StringArray items, int def)
    { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, String::fromUTF8 (name), items, def)); };

    addChoice ("mode", "Fonte", { String::fromUTF8 ("Áudio (trilha)"), "MIDI", String::fromUTF8 ("Áudio + MIDI"), String::fromUTF8 ("Progressão importada") }, 2);
    addBool ("autoKey", "Tonalidade automática", true);
    addChoice ("keyRoot", "Tônica", { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" }, 0);
    {
        StringArray sc { String::fromUTF8 ("Auto (maior/menor)") };
        for (int i = 0; i < (int) sms::Scale::count; ++i) sc.add (String::fromUTF8 (sms::scaleName ((sms::Scale) i)));
        addChoice ("scale", "Escala", sc, 0);
    }
    add01 ("sensitivity", "Sensibilidade", 0.6f);
    addBool ("sevenths", "Detectar sétimas", true);
    addBool ("lock", "Travar acorde", false);
    addBool ("conform", "Respeitar escala", true);
    addChoice ("extensions", "Extensões", { String::fromUTF8 ("Tríades"), String::fromUTF8 ("+ 7ª"), String::fromUTF8 ("+ 9ª"), String::fromUTF8 ("+ 11ª/13ª") }, 1);
    addChoice ("inversion", "Inversões", { String::fromUTF8 ("Automática"), "Fundamental", String::fromUTF8 ("1ª inversão"), String::fromUTF8 ("2ª inversão"), "Seguir o baixo" }, 0);
    add01 ("density", "Densidade", 0.6f);
    v.push_back (std::make_unique<AudioParameterInt> (ParameterID { "register", 1 }, String::fromUTF8 ("Registro"), -12, 12, 0));
    add01 ("spread", "Abertura", 0.5f);
    add01 ("humanize", "Humanização", 0.25f);
    add01 ("dynamics", "Dinâmica", 0.7f);
    add01 ("attack", "Ataque", 0.4f);
    add01 ("release", "Release", 0.5f);
    static const char* lvlNames[6] = { "Nível Violinos I", "Nível Violinos II", "Nível Violas", "Nível Violoncelos", "Nível Contrabaixos", "Nível Trompas" };
    static const float lvlDef[6] = { 0.0f, -1.0f, -1.0f, 0.0f, -0.5f, -4.0f };
    for (int i = 0; i < 6; ++i) addDb (ids::levels[i], lvlNames[i], -60.0f, 6.0f, lvlDef[i]);
    for (int i = 0; i < 8; ++i) addBool (ids::lanes[i], (String ("Faixa ") + String (i + 1)).toRawUTF8(), true);
    add01 ("reverbMix", "Reverb", 0.3f);
    add01 ("reverbSize", "Tamanho da sala", 0.7f);
    add01 ("reverbDecay", "Decaimento", 0.55f);
    addDb ("pianoLevel", "Nível do piano", -60.0f, 6.0f, -6.0f);
    addBool ("pianoInput", "Piano toca o MIDI", false);   // v1.0.1: desligado por padrão (só cobertura)
    addBool ("pianoCover", "Piano toca a cobertura", false);
    addBool ("coverOn", "Cobertura ligada", true);
    addDb ("monitor", "Retorno da entrada", -60.0f, 0.0f, -60.0f);
    addDb ("master", "Volume geral", -24.0f, 6.0f, 0.0f);
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "BPM interno", NormalisableRange<float> (40.0f, 240.0f, 0.1f), 110.0f));
    return { v.begin(), v.end() };
}

StructureProcessor::StructureProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Trilha (entrada)", AudioChannelSet::stereo(), true)
                        .withOutput ("Saída", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STRUCTURE", createLayout())
{
    auto g = [this] (const char* id) { auto* x = apvts.getRawParameterValue (id); jassert (x != nullptr); return x; };
    ptr.mode = g ("mode"); ptr.autoKey = g ("autoKey"); ptr.keyRoot = g ("keyRoot"); ptr.scale = g ("scale");
    ptr.sensitivity = g ("sensitivity"); ptr.sevenths = g ("sevenths"); ptr.lock = g ("lock"); ptr.conform = g ("conform");
    ptr.extensions = g ("extensions"); ptr.inversion = g ("inversion"); ptr.density = g ("density"); ptr.reg = g ("register");
    ptr.spread = g ("spread"); ptr.humanize = g ("humanize"); ptr.dynamics = g ("dynamics"); ptr.attack = g ("attack");
    ptr.release = g ("release"); ptr.reverbMix = g ("reverbMix"); ptr.reverbSize = g ("reverbSize"); ptr.reverbDecay = g ("reverbDecay");
    ptr.pianoLevel = g ("pianoLevel"); ptr.pianoInput = g ("pianoInput"); ptr.pianoCover = g ("pianoCover"); ptr.coverOn = g ("coverOn");
    ptr.monitor = g ("monitor"); ptr.master = g ("master"); ptr.bpm = g ("bpm");
    for (int i = 0; i < 6; ++i) ptr.level[i] = g (ids::levels[i]);
    for (int i = 0; i < 8; ++i) ptr.lane[i] = g (ids::lanes[i]);

    termsOk.store (Terms::isAccepted());
    auto fp = sms::factoryPresets();
    arrangement = fp.front().arrangement;
    engine.setArrangement (arrangement);
    startTimerHz (4);
}

StructureProcessor::~StructureProcessor()
{
    stopTimer();
    alive.reset();
    importPool.removeAllJobs (true, 4000);
}

bool StructureProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void StructureProcessor::startLibraryLoad (double rate)
{
    loadedRate = rate;
    libraryStatus = String::fromUTF8 ("Carregando timbres…");
    std::weak_ptr<int> guard = alive;
    loader.loadAsync (rate, [this, guard] (std::shared_ptr<sms::InstrumentBank> bank, String status)
    {
        if (guard.expired()) return;
        usingFallback = bank && bank->synthesized;
        libraryStatus = status;
        engine.setBank (bank);
    });
}

void StructureProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, jmax (32, samplesPerBlock));
    engine.setArrangement (arrangement);
    if (progression) engine.setProgression (progression);
    keyboardState.reset();
    // as amostras são lidas na taxa original e reamostradas na reprodução; só o banco de reserva
    // depende da taxa — recarrega se ainda não existe ou se ele estiver ativo e a taxa mudou
    inCopy.setSize (2, jmax (32, samplesPerBlock), false, true, false);
    if (loadedRate <= 0 || (usingFallback && std::abs (loadedRate - sampleRate) > 1.0))
    {
        std::weak_ptr<int> guard = alive;
        MessageManager::callAsync ([this, guard, sampleRate] { if (! guard.expired()) startLibraryLoad (sampleRate); });
    }
    termsOk.store (Terms::isAccepted());
}

void StructureProcessor::timerCallback()
{
    engine.collectGarbage();
    // o banco já carregado precisa ser reentregue se prepareToPlay recriou o motor
    if (! engine.getSampler().hasBank() && ! loader.isLoading() && loadedRate > 0)
        startLibraryLoad (getSampleRate() > 0 ? getSampleRate() : loadedRate);
}

sms::ArrangerSettings StructureProcessor::currentArrangerSettings() const
{
    sms::ArrangerSettings a;
    a.extensions = (int) ptr.extensions->load();
    a.inversion = (sms::InversionMode) (int) ptr.inversion->load();
    a.density = ptr.density->load();
    a.registerShift = (int) ptr.reg->load();
    a.spread = ptr.spread->load();
    a.humanizeTime = ptr.humanize->load();
    a.humanizeVel = ptr.humanize->load();
    a.dynamics = ptr.dynamics->load();
    a.respectScale = ptr.conform->load() > 0.5f;
    return a;
}

void StructureProcessor::updateEngineParams()
{
    auto& P = engine.params;
    P.mode = (sms::SourceMode) (int) ptr.mode->load();
    P.autoKey = ptr.autoKey->load() > 0.5f;
    P.keyRoot = (int) ptr.keyRoot->load();
    P.scaleChoice = (int) ptr.scale->load() - 1;
    P.sensitivity = ptr.sensitivity->load();
    P.allowSevenths = ptr.sevenths->load() > 0.5f;
    P.lock = ptr.lock->load() > 0.5f;
    P.conformToScale = ptr.conform->load() > 0.5f;
    P.arr = currentArrangerSettings();
    for (int i = 0; i < 6; ++i) P.sectionDb[(size_t) i] = ptr.level[i]->load();
    for (int i = 0; i < 8; ++i) P.laneOn[(size_t) i] = ptr.lane[i]->load() > 0.5f;
    P.attack01 = ptr.attack->load();
    P.release01 = ptr.release->load();
    P.reverbMix = ptr.reverbMix->load();
    P.reverbSize = ptr.reverbSize->load();
    P.reverbDecay = ptr.reverbDecay->load();
    P.pianoDb = ptr.pianoLevel->load();
    P.pianoPlaysInput = ptr.pianoInput->load() > 0.5f;
    P.pianoPlaysCover = ptr.pianoCover->load() > 0.5f;
    P.coverOn = ptr.coverOn->load() > 0.5f;
    P.inputMonitorDb = ptr.monitor->load();
    P.masterDb = ptr.master->load();
    P.internalBpm = ptr.bpm->load();
}

void StructureProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int num = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    // teclado na tela → MIDI
    keyboardState.processNextMidiBuffer (midi, 0, num, true);

    if (! termsOk.load())
    {
        buffer.clear();
        midi.clear();
        return;
    }

    updateEngineParams();

    sms::HostClock hc;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { hc.bpm = *bpm; hc.valid = true; lastHostBpm = *bpm; }
            if (auto ppq = pos->getPpqPosition()) hc.ppq = *ppq;
            hc.playing = pos->getIsPlaying();
        }
   #if JucePlugin_Build_Standalone
    if (wrapperType == wrapperType_Standalone) hc.valid = false;   // app: relógio interno (BPM do painel)
   #endif

    // copia a entrada (o buffer de saída reaproveita os mesmos canais)
    const float* inPtrs[2] = { nullptr, nullptr };
    if (numIn > 0)
    {
        inCopy.setSize (jmin (2, numIn), num, false, false, true);
        for (int c = 0; c < inCopy.getNumChannels(); ++c) inCopy.copyFrom (c, 0, buffer, c, 0, num);
        for (int c = 0; c < inCopy.getNumChannels(); ++c) inPtrs[c] = inCopy.getReadPointer (c);
    }

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : outL;

    // divide o bloco nos instantes dos eventos MIDI (piano tocável sem atraso de bloco)
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        while (pos < end)
        {
            const int n = jmin (end - pos, engine.getMaxBlockSize());   // hosts podem mandar blocos maiores que o anunciado
            const float* chans[2] = { inPtrs[0] ? inPtrs[0] + pos : nullptr, inPtrs[1] ? inPtrs[1] + pos : nullptr };
            sms::HostClock sub = hc;
            if (hc.valid) sub.ppq = hc.ppq + (double) pos / getSampleRate() * hc.bpm / 60.0;
            engine.process (chans, inPtrs[0] ? jmin (2, numIn) : 0, outL + pos, outR + pos, n, sub);
            pos += n;
        }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int t = jlimit (0, num, meta.samplePosition);
        if (t - pos >= 16) renderTo (t);
        if (m.isNoteOn()) engine.noteOnInput (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOffInput (m.getNoteNumber());
        else if (m.isSustainPedalOn()) engine.sustainPedal (true);
        else if (m.isSustainPedalOff()) engine.sustainPedal (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.requestPanic();
    }
    renderTo (num);
    if (buffer.getNumChannels() > 2)
        for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, num);
    midi.clear();
}

//==============================================================================
void StructureProcessor::setArrangement (const sms::Arrangement& a, bool markEdited)
{
    arrangement = a;
    engine.setArrangement (arrangement);
    if (markEdited) presetEdited = true;
}

void StructureProcessor::applyPreset (const sms::PresetData& p)
{
    for (auto& [id, value] : p.params)
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (String (id))))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    // presets ligam todas as faixas
    for (int i = 0; i < 8; ++i)
        if (auto* prm = apvts.getParameter (ids::lanes[i])) prm->setValueNotifyingHost (1.0f);
    presetName = String::fromUTF8 (p.name.c_str());
    setArrangement (p.arrangement, false);
    presetEdited = false;
}

bool StructureProcessor::loadPresetEntry (int index)
{
    const auto& list = presets.entries();
    if (index < 0 || index >= list.size()) return false;
    sms::PresetData p;
    if (! presets.load (list[index], p)) return false;
    currentProgram = index;
    applyPreset (p);
    return true;
}

sms::PresetData StructureProcessor::currentAsPreset (const String& name) const
{
    sms::PresetData p;
    p.name = name.toStdString();
    p.category = "User";
    p.arrangement = arrangement;
    p.arrangement.name = p.name;
    for (auto* id : { "density", "extensions", "spread", "register", "dynamics", "attack", "release", "reverbMix",
                      "reverbSize", "reverbDecay", "humanize", "inversion", "sensitivity" })
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (id)))
            p.params.push_back ({ id, prm->convertFrom0to1 (prm->getValue()) });
    for (int i = 0; i < 6; ++i)
        if (auto* prm = dynamic_cast<RangedAudioParameter*> (apvts.getParameter (ids::levels[i])))
            p.params.push_back ({ ids::levels[i], prm->convertFrom0to1 (prm->getValue()) });
    return p;
}

bool StructureProcessor::saveUserPreset (const String& name, String& error)
{
    if (! presets.save (currentAsPreset (name.trim()), error)) return false;
    presetName = name.trim();
    presetEdited = false;
    currentProgram = jmax (0, presets.indexOf (presetName));
    return true;
}

int StructureProcessor::getNumPrograms() { return jmax (1, presets.entries().size()); }
void StructureProcessor::setCurrentProgram (int i) { loadPresetEntry (i); }
const String StructureProcessor::getProgramName (int i)
{
    const auto& l = presets.entries();
    return isPositiveAndBelow (i, l.size()) ? l[i].name : String();
}

//==============================================================================
void StructureProcessor::importLoop (const File& f, std::function<void (bool, String)> done)
{
    const double hostBpm = lastHostBpm;
    std::weak_ptr<int> guard = alive;
    importPool.addJob ([this, f, hostBpm, done, guard]
    {
        String err;
        auto prog = LoopImporter::analyse (f, hostBpm, err);
        MessageManager::callAsync ([this, prog, err, done, f, guard]
        {
            if (guard.expired()) return;
            if (! prog) { if (done) done (false, err); return; }
            progression = prog;
            engine.setProgression (prog);
            String msg;
            msg << f.getFileName() << ": " << String (prog->bpm, 1) << " BPM, " << String ((int) prog->lengthBeats / 4)
                << String::fromUTF8 (" compassos, tom ") << String::fromUTF8 (prog->key.name().c_str());
            if (done) done (true, msg);
        });
    });
}

void StructureProcessor::clearProgression()
{
    progression.reset();
    engine.setProgression (nullptr);
}

File StructureProcessor::exportCoverMidi (int repeats, String& error)
{
    if (! progression) { error = String::fromUTF8 ("Importe um loop primeiro."); return {}; }
    auto name = String::fromUTF8 ("SeniorMy Structure - ") + File::createLegalFileName (presetName) + ".mid";
    auto target = File::getSpecialLocation (File::tempDirectory).getChildFile (name);
    return LoopImporter::exportCoverMidi (*progression, arrangement, currentArrangerSettings(), repeats, target, error);
}

//==============================================================================
void StructureProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    auto* extra = xml->createNewChildElement ("SMS_EXTRA");
    extra->setAttribute ("arrangement", JSON::toString (PresetIO::arrangementToVar (arrangement), true));
    if (progression) extra->setAttribute ("progression", JSON::toString (LoopImporter::progressionToVar (*progression), true));
    extra->setAttribute ("preset", presetName);
    extra->setAttribute ("edited", presetEdited);
    extra->setAttribute ("uiScale", (double) uiScale);
    extra->setAttribute ("stateVersion", 2);
    copyXmlToBinary (*xml, dest);
}

void StructureProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    std::unique_ptr<XmlElement> extraCopy;
    if (auto* extra = xml->getChildByName ("SMS_EXTRA"))
    {
        extraCopy = std::make_unique<XmlElement> (*extra);
        xml->removeChildElement (extra, true);
    }
    if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
    // v1.0.1: projetos salvos na 1.0.0 tinham o piano interno ligado por padrão
    // (som de piano por baixo da cobertura). Na migração ele volta desligado.
    if (! extraCopy || extraCopy->getIntAttribute ("stateVersion", 1) < 2)
        if (auto* prm = apvts.getParameter ("pianoInput")) prm->setValueNotifyingHost (0.0f);
    if (extraCopy)
    {
        const auto a = JSON::parse (extraCopy->getStringAttribute ("arrangement"));
        if (a.isObject()) setArrangement (PresetIO::arrangementFromVar (a), false);
        const auto pj = extraCopy->getStringAttribute ("progression");
        if (pj.isNotEmpty())
            if (auto pr = LoopImporter::progressionFromVar (JSON::parse (pj))) { progression = pr; engine.setProgression (pr); }
        presetName = extraCopy->getStringAttribute ("preset", presetName);
        presetEdited = extraCopy->getBoolAttribute ("edited", false);
        uiScale = (float) extraCopy->getDoubleAttribute ("uiScale", 1.0);
    }
}

AudioProcessorEditor* StructureProcessor::createEditor() { return new StructureEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StructureProcessor(); }
