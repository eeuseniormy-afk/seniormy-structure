#include "LoopImporter.h"

using namespace juce;

namespace LoopImporter
{
bool isSupported (const File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;mid;midi");
}

String wildcard() { return "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.mid;*.midi"; }

double bpmFromFileName (const String& name)
{
    // "loop_120bpm", "Piano 96 BPM", "92_Am_keys"
    const auto lower = name.toLowerCase();
    const int idx = lower.indexOf ("bpm");
    if (idx > 0)
    {
        int end = idx;
        while (end > 0 && lower[end - 1] == ' ') --end;
        int start = end;
        while (start > 0 && CharacterFunctions::isDigit (lower[start - 1])) --start;
        const double v = lower.substring (start, end).getDoubleValue();
        if (v >= 50 && v <= 220) return v;
    }
    StringArray tokens;
    tokens.addTokens (lower, " _-.", "");
    for (auto& t : tokens)
        if (t.containsOnly ("0123456789") && t.length() >= 2 && t.length() <= 3)
        {
            const double v = t.getDoubleValue();
            if (v >= 60 && v <= 200) return v;
        }
    return 0;
}

static std::shared_ptr<sms::Progression> analyseMidi (const File& f, String& error)
{
    FileInputStream in (f);
    MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { error = String::fromUTF8 ("Arquivo MIDI inválido."); return nullptr; }
    const short tf = mf.getTimeFormat();
    double bpm = 120;
    MidiMessageSequence tempos;
    mf.findAllTempoEvents (tempos);
    if (tempos.getNumEvents() > 0) bpm = 60.0 / tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote();
    std::vector<sms::MidiNoteIn> notes;
    if (tf > 0)
    {
        const double tpq = (double) tf;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn() || e->message.getChannel() == 10) continue;
                const double start = e->message.getTimeStamp() / tpq;
                const double end = e->noteOffObject ? e->noteOffObject->message.getTimeStamp() / tpq : start + 0.5;
                notes.push_back ({ start, jmax (0.05, end - start), e->message.getNoteNumber(), e->message.getFloatVelocity() });
            }
        }
    }
    else
    {
        mf.convertTimestampTicksToSeconds();
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn() || e->message.getChannel() == 10) continue;
                const double start = e->message.getTimeStamp() * bpm / 60.0;
                const double end = e->noteOffObject ? e->noteOffObject->message.getTimeStamp() * bpm / 60.0 : start + 0.5;
                notes.push_back ({ start, jmax (0.05, end - start), e->message.getNoteNumber(), e->message.getFloatVelocity() });
            }
        }
    }
    if (notes.empty()) { error = String::fromUTF8 ("O arquivo MIDI não tem notas (fora o canal de bateria)."); return nullptr; }
    // começa no primeiro compasso com nota
    double first = 1e9;
    for (auto& n : notes) first = jmin (first, n.startBeat);
    const double shift = std::floor (first / 4.0) * 4.0;
    for (auto& n : notes) n.startBeat -= shift;
    auto p = std::make_shared<sms::Progression> (sms::analyseMidiLoop (notes, bpm));
    if (p->lengthBeats > 64) p->lengthBeats = 64;   // até 16 compassos
    return p;
}

std::shared_ptr<sms::Progression> analyse (const File& f, double hostBpm, String& error)
{
    std::shared_ptr<sms::Progression> p;
    if (f.hasFileExtension ("mid;midi")) p = analyseMidi (f, error);
    else
    {
        AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
        if (! r) { error = String::fromUTF8 ("Formato de áudio não suportado."); return nullptr; }
        const auto maxLen = (int64) (r->sampleRate * 120.0);   // até 2 minutos
        const int n = (int) jmin (r->lengthInSamples, maxLen);
        if (n < r->sampleRate * 0.5) { error = String::fromUTF8 ("Arquivo curto demais."); return nullptr; }
        AudioBuffer<float> buf ((int) jmax (1u, jmin (2u, r->numChannels)), n);
        r->read (&buf, 0, n, 0, true, true);
        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float s = 0;
            for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getSample (c, i);
            mono[(size_t) i] = s / (float) buf.getNumChannels();
        }
        double hint = bpmFromFileName (f.getFileNameWithoutExtension());
        if (hint <= 0 && hostBpm > 0)
        {
            // se o loop fecha em compassos inteiros no andamento do projeto, usa-o
            const double beats = (double) n / r->sampleRate * hostBpm / 60.0;
            if (std::abs (beats / 4.0 - std::round (beats / 4.0)) < 0.04) hint = hostBpm;
        }
        p = std::make_shared<sms::Progression> (sms::analyseAudioLoop (mono, r->sampleRate, hint));
    }
    if (p && p->empty()) { error = String::fromUTF8 ("Não encontrei harmonia nesse arquivo."); return nullptr; }
    if (p) p->sourceName = f.getFileName().toStdString();
    return p;
}

File exportCoverMidi (const sms::Progression& prog, const sms::Arrangement& arr, const sms::ArrangerSettings& settings,
                      int repeats, const File& target, String& error)
{
    if (prog.empty()) { error = String::fromUTF8 ("Nenhuma progressão carregada."); return {}; }
    const double sr = 48000.0;
    sms::Arranger a;
    a.settings = settings;
    a.prepare (sr);
    a.setArrangement (arr);
    const double bpm = prog.bpm > 0 ? prog.bpm : 120.0;
    const double totalBeats = prog.lengthBeats * jmax (1, repeats);
    const int block = 256;
    const double samplesPerBeat = sr * 60.0 / bpm;
    std::vector<sms::NoteEvent> events, all;
    sms::Arranger::Clock clk; clk.bpm = bpm; clk.playing = true;
    for (int64 s = 0; (double) s < totalBeats * samplesPerBeat; s += block)
    {
        clk.ppq = (double) s / samplesPerBeat;
        events.clear();
        a.process (block, clk, prog.chordAt (clk.ppq), prog.key, false, events);
        all.insert (all.end(), events.begin(), events.end());
    }
    events.clear();
    a.allNotesOff (events);
    for (auto& e : events) e.time = (int64) (totalBeats * samplesPerBeat);
    all.insert (all.end(), events.begin(), events.end());

    MidiFile mf;
    mf.setTicksPerQuarterNote (480);
    MidiMessageSequence meta;
    meta.addEvent (MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
    meta.addEvent (MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    meta.addEvent (MidiMessage::textMetaEvent (3, "SeniorMy Structure"), 0);
    mf.addTrack (meta);
    for (int s = 0; s < sms::numSections; ++s)
    {
        MidiMessageSequence seq;
        auto nameMsg = MidiMessage::textMetaEvent (3, String::fromUTF8 (sms::sectionName (s)));
        seq.addEvent (nameMsg, 0);
        bool any = false;
        for (auto& e : all)
        {
            if (e.section != s) continue;
            const double tick = (double) e.time / samplesPerBeat * 480.0;
            const int ch = 1 + s;
            seq.addEvent (e.on ? MidiMessage::noteOn (ch, e.note, (uint8) e.velocity) : MidiMessage::noteOff (ch, e.note), tick);
            any = true;
        }
        if (! any) continue;
        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }
    target.deleteFile();
    FileOutputStream out (target);
    if (! out.openedOk() || ! mf.writeTo (out)) { error = "Falha ao gravar " + target.getFullPathName(); return {}; }
    return target;
}

var progressionToVar (const sms::Progression& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("bpm", p.bpm);
    o->setProperty ("length", p.lengthBeats);
    o->setProperty ("keyTonic", p.key.tonic);
    o->setProperty ("keyScale", (int) p.key.scale);
    o->setProperty ("source", String (p.sourceName));
    Array<var> chords, rhythm;
    for (auto& c : p.chords)
    {
        auto* co = new DynamicObject();
        co->setProperty ("start", c.startBeat);
        co->setProperty ("len", c.lengthBeats);
        co->setProperty ("root", c.chord.root);
        co->setProperty ("q", (int) c.chord.quality);
        co->setProperty ("bass", c.chord.bass);
        chords.add (var (co));
    }
    for (float r : p.rhythm) rhythm.add (r);
    o->setProperty ("chords", chords);
    o->setProperty ("rhythm", rhythm);
    return var (o);
}

std::shared_ptr<sms::Progression> progressionFromVar (const var& v)
{
    if (! v.isObject()) return nullptr;
    auto p = std::make_shared<sms::Progression>();
    p->bpm = (double) v.getProperty ("bpm", 120.0);
    p->lengthBeats = (double) v.getProperty ("length", 0.0);
    p->key.tonic = jlimit (0, 11, (int) v.getProperty ("keyTonic", 0));
    p->key.scale = (sms::Scale) jlimit (0, (int) sms::Scale::count - 1, (int) v.getProperty ("keyScale", 0));
    p->sourceName = v["source"].toString().toStdString();
    if (auto* cs = v["chords"].getArray())
        for (auto& c : *cs)
        {
            sms::ProgressionChord pc;
            pc.startBeat = (double) c.getProperty ("start", 0.0);
            pc.lengthBeats = (double) c.getProperty ("len", 4.0);
            pc.chord.root = jlimit (-1, 11, (int) c.getProperty ("root", -1));
            pc.chord.quality = (sms::Quality) jlimit (0, (int) sms::Quality::count - 1, (int) c.getProperty ("q", 0));
            pc.chord.bass = jlimit (-1, 11, (int) c.getProperty ("bass", -1));
            pc.chord.confidence = 1.0f;
            p->chords.push_back (pc);
        }
    if (auto* r = v["rhythm"].getArray())
        for (int i = 0; i < jmin (16, r->size()); ++i) p->rhythm[(size_t) i] = (float) (double) (*r)[i];
    if (p->chords.empty() || p->lengthBeats <= 0) return nullptr;
    return p;
}
} // namespace LoopImporter
