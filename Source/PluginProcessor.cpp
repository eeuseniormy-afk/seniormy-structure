#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LoopImporter.h"
#include "Terms.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout GuitarsProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Volume geral", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -3.0f));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 }, "Reverb (sala)", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.22f));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "BPM interno", NormalisableRange<float> (40.0f, 240.0f, 0.1f), 96.0f));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, String::fromUTF8 ("Tocar (relógio interno)"), false));
    for (int p = 0; p < sms::kParts; ++p)
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level" + String (p + 1), 1 }, String::fromUTF8 ("Nível parte ") + String (p + 1),
                                                            NormalisableRange<float> (-48.0f, 6.0f, 0.1f), 0.0f));
    return { v.begin(), v.end() };
}

GuitarsProcessor::GuitarsProcessor()
    : AudioProcessor (BusesProperties().withOutput (String::fromUTF8 ("Saída"), AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GUITARS", createLayout())
{
    ptr.master = apvts.getRawParameterValue ("master");
    ptr.reverb = apvts.getRawParameterValue ("reverb");
    ptr.bpm = apvts.getRawParameterValue ("bpm");
    ptr.play = apvts.getRawParameterValue ("play");
    for (int p = 0; p < sms::kParts; ++p) ptr.level[p] = apvts.getRawParameterValue ("level" + String (p + 1));

    termsOk.store (Terms::isAccepted());
    const auto fp = sms::factoryPresets();
    applyPreset (fp.front());
    presetEdited = false;

    libraryStatus = String::fromUTF8 ("Carregando amostras…");
    std::weak_ptr<int> guard = alive;
    loader.loadAsync ([this, guard] (std::shared_ptr<sms::Bank> bank, String status)
    {
        if (guard.expired()) return;
        libraryStatus = status;
        banks.push_back (bank);
        engine.setBank (bank.get());
    });
}

GuitarsProcessor::~GuitarsProcessor()
{
    alive.reset();
    importPool.removeAllJobs (true, 4000);
    engine.setBank (nullptr);
}

bool GuitarsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void GuitarsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, jmax (32, samplesPerBlock));
    stateDirty.store (true);
    keyboardState.reset();
}

sms::EngineState GuitarsProcessor::getState() const
{
    const SpinLock::ScopedLockType l (stateLock);
    return uiState;
}

void GuitarsProcessor::setState (const sms::EngineState& s, bool markEdited)
{
    {
        const SpinLock::ScopedLockType l (stateLock);
        uiState = s;
    }
    stateDirty.store (true);
    if (markEdited) presetEdited = true;
}

void GuitarsProcessor::pushParamsFromState (const sms::EngineState& s)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set ("master", s.master.volumeDb);
    set ("reverb", s.master.reverbMix);
    set ("bpm", s.master.bpm);
    for (int p = 0; p < sms::kParts; ++p) set (("level" + String (p + 1)).toRawUTF8(), s.parts[(size_t) p].levelDb);
}

void GuitarsProcessor::applyPreset (const sms::PresetData& p)
{
    presetName = String::fromUTF8 (p.name.c_str());
    presetDescription = String::fromUTF8 (p.description.c_str());
    auto s = p.state;
    s.master.internalPlay = getState().master.internalPlay;
    setState (s, false);
    pushParamsFromState (s);
    presetEdited = false;
    engine.stopAll();
    ++stateVersion;
}

bool GuitarsProcessor::loadPresetEntry (int index)
{
    const auto& list = presets.entries();
    if (index < 0 || index >= list.size()) return false;
    sms::PresetData d;
    if (! presets.load (list[index], d)) return false;
    applyPreset (d);
    if (list[index].factory) currentProgram = list[index].factoryIndex;
    return true;
}

bool GuitarsProcessor::saveUserPreset (const String& name, String& error)
{
    sms::PresetData d;
    d.name = name.toStdString(); d.category = "User"; d.description = presetDescription.toStdString();
    d.state = getState();
    d.state.master.volumeDb = ptr.master->load(); d.state.master.reverbMix = ptr.reverb->load(); d.state.master.bpm = ptr.bpm->load();
    for (int p = 0; p < sms::kParts; ++p) d.state.parts[(size_t) p].levelDb = ptr.level[p]->load();
    if (! presets.save (d, error)) return false;
    presetName = name; presetEdited = false;
    return true;
}

void GuitarsProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    if (! termsOk.load()) { buffer.clear(); return; }   // bloqueado até o aceite dos Termos

    if (stateDirty.load())
    {
        const SpinLock::ScopedTryLockType l (stateLock);
        if (l.isLocked()) { engine.setState (uiState); stateDirty.store (false); }
    }
    std::array<float, sms::kParts> lev {};
    for (int p = 0; p < sms::kParts; ++p) lev[(size_t) p] = ptr.level[p]->load();
    engine.setLive (ptr.master->load(), ptr.reverb->load(), lev, ptr.bpm->load(), ptr.play->load() > 0.5f);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int off = jlimit (0, n - 1, meta.samplePosition);
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), off);
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber(), off);
        else if (m.isSustainPedalOn()) engine.setSustain (true);
        else if (m.isSustainPedalOff()) engine.setSustain (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }

    sms::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm()) { t.bpm = *b; hostBpm.store (*b); }
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; t.hasPpq = true; }
        }
    if (wrapperType == wrapperType_Standalone) t.hostPlaying = false;
    hostPlaying.store (t.hostPlaying);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, t);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

int GuitarsProcessor::getNumPrograms() { return (int) jmax<size_t> (1, sms::factoryPresets().size()); }

void GuitarsProcessor::setCurrentProgram (int i)
{
    const auto fp = sms::factoryPresets();
    if (i < 0 || i >= (int) fp.size()) return;
    currentProgram = i;
    applyPreset (fp[(size_t) i]);
}

const String GuitarsProcessor::getProgramName (int i)
{
    const auto fp = sms::factoryPresets();
    return (i >= 0 && i < (int) fp.size()) ? String::fromUTF8 (fp[(size_t) i].name.c_str()) : String();
}

void GuitarsProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("engine", JSON::toString (PresetIO::stateToVar (getState()), true), nullptr);
    st.setProperty ("preset", presetName, nullptr);
    st.setProperty ("presetEdited", presetEdited, nullptr);
    st.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void GuitarsProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = ValueTree::fromXml (*xml);
    sms::EngineState s;
    if (PresetIO::stateFromVar (JSON::parse (vt.getProperty ("engine").toString()), s)) setState (s, false);
    presetName = vt.getProperty ("preset", presetName).toString();
    presetEdited = (bool) vt.getProperty ("presetEdited", false);
    uiScale = jlimit (0.6f, 2.0f, (float) (double) vt.getProperty ("uiScale", 1.0));
    vt.removeProperty ("engine", nullptr); vt.removeProperty ("preset", nullptr);
    vt.removeProperty ("presetEdited", nullptr); vt.removeProperty ("uiScale", nullptr);
    apvts.replaceState (vt);
    ++stateVersion;
}

void GuitarsProcessor::importLoop (const File& f, std::function<void (bool, String)> done)
{
    std::weak_ptr<int> guard = alive;
    const double bpmHint = hostBpm.load();
    importPool.addJob ([this, f, done, guard, bpmHint]
    {
        String err;
        auto prog = LoopImporter::analyse (f, bpmHint, err);
        MessageManager::callAsync ([this, prog, err, done, guard, f]
        {
            if (guard.expired()) return;
            if (! prog) { if (done) done (false, err); return; }
            auto s = getState();
            LoopImporter::applyToHarmony (*prog, s);
            setState (s);
            ++stateVersion;
            if (auto* p = apvts.getParameter ("bpm")) p->setValueNotifyingHost (p->convertTo0to1 (s.master.bpm));
            String names;
            for (int i = 0; i < s.harmony.numProg; ++i)
            {
                sms::Chord c; c.root = s.harmony.prog[(size_t) i].root; c.quality = (sms::Quality) s.harmony.prog[(size_t) i].quality; c.bass = s.harmony.prog[(size_t) i].bass;
                names << String::fromUTF8 (c.name().c_str()) << (i + 1 < s.harmony.numProg ? "  " : "");
            }
            if (done) done (true, f.getFileName() + String::fromUTF8 (" → ") + String (s.harmony.numProg) + String::fromUTF8 (" acordes, ")
                                  + String (roundToInt (s.master.bpm)) + " BPM: " + names);
        });
    });
}

File GuitarsProcessor::exportMidi (int repeats, const File& target, String& error)
{
    auto s = getState();
    s.master.bpm = ptr.bpm->load();
    return LoopImporter::exportMidi (s, repeats, target, error);
}

AudioProcessorEditor* GuitarsProcessor::createEditor() { return new GuitarsEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GuitarsProcessor(); }
